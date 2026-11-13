#include "MainWindow.h"
#include "ThemeManager.h"
#include "foundation/Logger.h"
#include "ui/FourViewer.h"
#include "ui/ParameterPanel.h"
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QStatusBar>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_theme(new ThemeManager(this)) {
    auto *root = new QWidget(this);
    auto *layout = new QHBoxLayout(root);
    m_parameters = new ParameterPanel(root);
    m_parameters->setFixedWidth(320);
    m_viewer = new FourViewer(root);
    layout->addWidget(m_parameters);
    layout->addWidget(m_viewer, 1);
    setCentralWidget(root);
    resize(1500, 920);
    setWindowTitle(tr("vtk3D · ITK / VTK 工作站"));
    m_status = new QLabel(tr("就绪"), this);
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setFixedWidth(180);
    statusBar()->addWidget(m_status, 1);
    statusBar()->addPermanentWidget(m_progress);
    auto *file = menuBar()->addMenu(tr("文件"));
    file->addAction(tr("导入影像…"), this, [this] {
        const auto path =
            QFileDialog::getOpenFileName(this, tr("导入影像"), "data",
                                         tr("影像 (*.mhd *.mha *.nrrd *.nhdr *.nii *.nii.gz);;所有文件 (*)"));
        if (!path.isEmpty())
            importPath(path);
    });
    file->addAction(tr("导入 DICOM 目录…"), this, [this] {
        const auto path = QFileDialog::getExistingDirectory(this, tr("DICOM 目录"));
        if (!path.isEmpty())
            importPath(path);
    });
    file->addAction(tr("打开项目…"), this, &MainWindow::openProject);
    file->addAction(tr("保存项目…"), this, &MainWindow::saveProject);
    file->addAction(tr("导出皮肤模型…"), this, [this] { exportSurface(true); });
    file->addAction(tr("导出骨骼模型…"), this, [this] { exportSurface(false); });
    file->addAction(tr("导出分割标签…"), this, [this] {
        if (!hasData() || m_tasks.busy())
            return;
        const auto path = QFileDialog::getSaveFileName(this, tr("导出标签"), "labels.mha",
                                                       tr("标签影像 (*.mha *.nii.gz *.nrrd)"));
        if (path.isEmpty())
            return;
        const auto snapshot = m_project.data().segmentation;
        m_tasks.submit(tr("导出标签"), [this, snapshot, path](const Task &task) {
            m_export.segmentation(snapshot, path, task);
        });
    });
    file->addAction(tr("退出"), qApp, &QApplication::quit);
    auto *processing = menuBar()->addMenu(tr("处理"));
    processing->addAction(tr("应用参数并重建"), this, &MainWindow::process);
    processing->addAction(tr("配准移动影像至当前 CT（平移）…"), this, &MainWindow::registerVolume);
    processing->addAction(tr("取消当前和待执行任务"), &m_tasks, &TaskManager::cancel);
    auto *view = menuBar()->addMenu(tr("视图"));
    view->addAction(tr("重置视图"), m_viewer, &FourViewer::resetCameras);
    view->addAction(tr("切换主题"), this, [this] {
        m_theme->apply(qApp, m_theme->theme() == ThemeManager::Theme::Dark ? ThemeManager::Theme::Light
                                                                           : ThemeManager::Theme::Dark);
    });
    connect(m_theme, &ThemeManager::themeChanged, m_viewer, &FourViewer::setDarkTheme);
    m_theme->apply(qApp, m_theme->theme());
    connect(m_parameters, &ParameterPanel::processRequested, this, &MainWindow::process);
    connect(m_parameters, &ParameterPanel::visibilityChanged, this, [this](int i, bool v) {
        if (i)
            m_viewer->setBoneVisible(v);
        else
            m_viewer->setSkinVisible(v);
    });
    connect(m_parameters, &ParameterPanel::opacityChanged, this, [this](int i, int v) {
        if (i)
            m_viewer->setBoneOpacity(v);
        else
            m_viewer->setSkinOpacity(v);
    });
    connect(&m_project, &ProjectService::changed, this, &MainWindow::showProject);
    connect(&m_tasks, &TaskManager::started, this, [this](const QString &text) {
        m_status->setText(text);
        m_progress->setValue(0);
    });
    connect(&m_tasks, &TaskManager::progress, this, [this](int v, const QString &text) {
        m_progress->setValue(v);
        m_status->setText(text);
    });
    connect(&m_tasks, &TaskManager::finished, this, [this](const QString &text) {
        m_progress->setValue(100);
        m_status->setText(text);
    });
    connect(&m_tasks, &TaskManager::failed, this, [this](const QString &text) {
        Logger::error(text);
        m_status->setText(text);
        emit taskFailed(text);
        if (!qApp->property("smokeTest").toBool())
            QMessageBox::critical(this, tr("任务失败"), text);
    });
    if (!qApp->property("smokeTest").toBool() && QFileInfo::exists("data/FullHead.mhd"))
        QTimer::singleShot(0, this, [this] { importPath("data/FullHead.mhd"); });
}
MainWindow::~MainWindow() {
    m_tasks.cancel();
}
void MainWindow::importPath(const QString &path) {
    if (m_tasks.busy()) {
        m_status->setText(tr("请等待任务完成，或先取消任务"));
        return;
    }
    auto result = std::make_shared<ProjectData>();
    auto display = std::make_shared<vtkSmartPointer<vtkImageData>>();
    if (QFileInfo(path).isDir()) {
        auto ids = std::make_shared<QStringList>();
        m_tasks.submit(
            tr("扫描 DICOM"),
            [this, path, ids](const Task &task) {
                task.check();
                *ids = m_import.series(path);
            },
            [this, path, ids] {
                if (ids->isEmpty()) {
                    throw std::runtime_error("No DICOM series found");
                }
                bool ok = true;
                const auto uid =
                    ids->size() == 1
                        ? ids->front()
                        : QInputDialog::getItem(this, tr("选择序列"), tr("Series UID"), *ids, 0, false, &ok);
                if (!ok)
                    return;
                auto snapshot = std::make_shared<ProjectData>();
                auto display = std::make_shared<vtkSmartPointer<vtkImageData>>();
                m_tasks.submit(
                    tr("导入 DICOM"),
                    [this, path, uid, snapshot, display](const Task &task) {
                        *snapshot = m_import.import(path, uid, task);
                        *display = m_processing.displayImage(snapshot->volume);
                    },
                    [this, snapshot, display] {
                        m_displayImage = *display;
                        m_project.replace(*snapshot);
                        process();
                    });
            });
        return;
    }
    m_tasks.submit(
        tr("导入影像"),
        [this, path, result, display](const Task &task) {
            *result = m_import.import(path, {}, task);
            *display = m_processing.displayImage(result->volume);
        },
        [this, result, display] {
            m_displayImage = *display;
            m_project.replace(*result);
            process();
        });
}
void MainWindow::process() {
    if (!hasData() || m_tasks.busy())
        return;
    const auto source = m_project.data();
    const auto pre = m_parameters->preprocessing();
    const auto seg = m_parameters->segmentation();
    const auto surface = m_parameters->surfaceProcessing();
    const auto skin = m_parameters->skinThreshold(), bone = m_parameters->boneThreshold();
    auto result = std::make_shared<ProjectData>();
    m_tasks.submit(
        tr("处理与重建"),
        [this, source, pre, seg, surface, skin, bone, result](const Task &task) {
            *result = m_processing.process(source, skin, bone, pre, seg, surface, task);
        },
        [this, result] {
            m_project.replace(*result);
            emit dataReady();
        });
}
void MainWindow::showProject() {
    const auto &p = m_project.data();
    if (!p.volume.image)
        return;
    m_viewer->setData(m_displayImage, p.skin.mesh, p.bone.mesh);
    const auto g = p.volume.geometry();
    m_status->setText(QString("%1 · %2 × %3 × %4")
                          .arg(p.volume.source)
                          .arg(g.dimensions[0])
                          .arg(g.dimensions[1])
                          .arg(g.dimensions[2]));
}
void MainWindow::exportSurface(bool skin) {
    if (!hasData() || m_tasks.busy())
        return;
    const auto path = QFileDialog::getSaveFileName(this, tr("导出表面"), skin ? "skin.stl" : "bone.stl",
                                                   tr("模型 (*.stl *.vtp *.obj *.ply)"));
    if (path.isEmpty())
        return;
    const auto snapshot = skin ? m_project.data().skin : m_project.data().bone;
    m_tasks.submit(tr("导出模型"),
                   [this, snapshot, path](const Task &task) { m_export.surface(snapshot, path, task); });
}
void MainWindow::registerVolume() {
    if (!hasData() || m_tasks.busy())
        return;
    const auto path = QFileDialog::getOpenFileName(this, tr("选择同模态移动 CT"), {},
                                                   tr("影像 (*.mhd *.mha *.nrrd *.nii *.nii.gz)"));
    if (path.isEmpty())
        return;
    const auto fixed = m_project.data();
    const auto parameter = m_parameters->registration();
    auto result = std::make_shared<ProjectData>();
    auto display = std::make_shared<vtkSmartPointer<vtkImageData>>();
    m_tasks.submit(
        tr("平移配准"),
        [this, path, fixed, parameter, result, display](const Task &task) {
            const auto moving = m_import.import(path, {}, task);
            *result = m_processing.registerVolume(fixed, moving.volume, parameter, task);
            *display = m_processing.displayImage(result->volume);
        },
        [this, result, display] {
            m_displayImage = *display;
            m_project.replace(*result);
            process();
        });
}
void MainWindow::saveProject() {
    if (!hasData() || m_tasks.busy())
        return;
    const auto path = QFileDialog::getSaveFileName(this, tr("保存项目"), "project.json", tr("项目 (*.json)"));
    if (path.isEmpty())
        return;
    const auto snapshot = m_project.data();
    m_tasks.submit(tr("保存项目"), [this, snapshot, path](const Task &task) {
        task.check();
        m_project.save(snapshot, path);
    });
}
void MainWindow::openProject() {
    if (m_tasks.busy())
        return;
    const auto path = QFileDialog::getOpenFileName(this, tr("打开项目"), {}, tr("项目 (*.json)"));
    if (path.isEmpty())
        return;
    auto result = std::make_shared<ProjectData>();
    auto display = std::make_shared<vtkSmartPointer<vtkImageData>>();
    m_tasks.submit(
        tr("打开项目"),
        [this, path, result, display](const Task &task) {
            task.check();
            *result = m_project.open(path);
            *display = m_processing.displayImage(result->volume);
        },
        [this, result, display] {
            m_displayImage = *display;
            m_parameters->restore(result->processing);
            m_project.replace(*result);
        });
}
