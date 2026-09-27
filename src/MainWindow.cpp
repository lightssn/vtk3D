#include "MainWindow.h"

#include "MedicalViewer.h"
#include "ThemeManager.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QLabel* sectionTitle(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("sectionTitle"));
    return label;
    }

QWidget* separator(QWidget* parent) {
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setObjectName(QStringLiteral("separator"));
    return line;
    }
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), m_themeManager(new ThemeManager(this)) {
    buildUi();
    createActions();
    resize(1500, 920);
    setMinimumSize(1120, 720);
    setWindowTitle(tr("vtk3D  ·  CT 重建工作站"));
    m_themeManager->apply(qApp, m_themeManager->theme());
    connect(m_themeManager, &ThemeManager::themeChanged, m_viewer, &MedicalViewer::setDarkTheme);
    m_viewer->setDarkTheme(m_themeManager->theme() == ThemeManager::Theme::Dark);
    tryLoadDefaultData();
    }

void MainWindow::buildUi() {
    auto* root = new QWidget(this);
    auto* rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(12);
    m_viewer = new MedicalViewer(root);
    rootLayout->addWidget(createSidebar());
    rootLayout->addWidget(m_viewer, 1);
    setCentralWidget(root);

    connect(m_viewer, &MedicalViewer::dataLoaded, this,
    [this](const QString& fileName, const QString& dimensions) {
        m_fileLabel->setText(QFileInfo(fileName).fileName());
        m_fileLabel->setToolTip(fileName);
        m_dimensionLabel->setText(dimensions + tr(" voxels"));
        setDataActionsEnabled(true);
        statusBar()->showMessage(tr("数据加载完成：%1").arg(fileName), 5000);
        });

    m_progressLabel = new QLabel(tr("就绪"), this);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedWidth(190);
    m_progressBar->setVisible(false);
    statusBar()->addPermanentWidget(m_progressLabel, 1);
    statusBar()->addPermanentWidget(m_progressBar);
    connect(m_viewer, &MedicalViewer::operationStarted, this, [this](const QString& text) {
        m_progressLabel->setText(text);
        m_progressBar->setValue(0);
        m_progressBar->setVisible(true);
        });
    connect(m_viewer, &MedicalViewer::operationProgress, this,
    [this](int value, const QString& text) {
        m_progressLabel->setText(text);
        m_progressBar->setValue(value);
        });
    connect(m_viewer, &MedicalViewer::operationFinished, this, [this](const QString& text) {
        m_progressLabel->setText(text);
        m_progressBar->setValue(100);
        QTimer::singleShot(1800, this, [this] { m_progressBar->setVisible(false); });
        });
    connect(m_viewer, &MedicalViewer::operationFailed, this, [this](const QString& text) {
        m_progressLabel->setText(tr("处理失败"));
        m_progressBar->setVisible(false);
        QMessageBox::critical(this, tr("后台任务失败"), text);
        });
    statusBar()->showMessage(tr("就绪 · 请打开 FullHead.mhd"));
    }

QWidget* MainWindow::createSidebar() {
    auto* sidebar = new QWidget(this);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(286);
    auto* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto* eyebrow = new QLabel(tr("MEDICAL VISUALIZATION"), sidebar);
    eyebrow->setObjectName(QStringLiteral("eyebrow"));
    auto* brand = new QLabel(tr("vtk<span>3D</span>"), sidebar);
    brand->setObjectName(QStringLiteral("brand"));
    brand->setTextFormat(Qt::RichText);
    auto* subtitle = new QLabel(tr("CT 表面重建工作站"), sidebar);
    subtitle->setObjectName(QStringLiteral("subtitle"));
    layout->addWidget(eyebrow);
    layout->addWidget(brand);
    layout->addWidget(subtitle);
    layout->addSpacing(8);

    auto* openButton = new QPushButton(tr("  打开 MetaImage 数据"), sidebar);
    openButton->setObjectName(QStringLiteral("primaryButton"));
    openButton->setMinimumHeight(42);
    connect(openButton, &QPushButton::clicked, this, &MainWindow::openVolume);
    layout->addWidget(openButton);

    auto* dataCard = new QWidget(sidebar);
    dataCard->setObjectName(QStringLiteral("infoCard"));
    auto* dataLayout = new QVBoxLayout(dataCard);
    dataLayout->setContentsMargins(12, 10, 12, 10);
    auto* dataCaption = new QLabel(tr("当前数据"), dataCard);
    dataCaption->setObjectName(QStringLiteral("caption"));
    m_fileLabel = new QLabel(tr("尚未加载"), dataCard);
    m_fileLabel->setObjectName(QStringLiteral("fileName"));
    m_fileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_dimensionLabel = new QLabel(tr("—"), dataCard);
    m_dimensionLabel->setObjectName(QStringLiteral("muted"));
    dataLayout->addWidget(dataCaption);
    dataLayout->addWidget(m_fileLabel);
    dataLayout->addWidget(m_dimensionLabel);
    layout->addWidget(dataCard);
    layout->addWidget(separator(sidebar));

    layout->addWidget(sectionTitle(tr("组织图层"), sidebar));
    auto addTissueControl = [this, layout, sidebar](const QString& name, const QString& dotClass,
    bool skin) {
        auto* row = new QWidget(sidebar);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto* dot = new QLabel(QStringLiteral("●"), row);
        dot->setObjectName(dotClass);
        auto* check = new QCheckBox(name, row);
        check->setChecked(true);
        rowLayout->addWidget(dot);
        rowLayout->addWidget(check, 1);
        layout->addWidget(row);

        auto* opacityRow = new QWidget(sidebar);
        auto* opacityLayout = new QHBoxLayout(opacityRow);
        opacityLayout->setContentsMargins(20, 0, 0, 0);
        auto* opacityText = new QLabel(tr("不透明度"), opacityRow);
        opacityText->setObjectName(QStringLiteral("muted"));
        auto* slider = new QSlider(Qt::Horizontal, opacityRow);
        slider->setRange(0, 100);
        slider->setValue(skin ? 34 : 100);
        auto* value = new QLabel(QString::number(slider->value()) + QStringLiteral("%"), opacityRow);
        value->setObjectName(QStringLiteral("valueLabel"));
        value->setFixedWidth(38);
        opacityLayout->addWidget(opacityText);
        opacityLayout->addWidget(slider, 1);
        opacityLayout->addWidget(value);
        layout->addWidget(opacityRow);

        auto* thresholdRow = new QWidget(sidebar);
        auto* thresholdLayout = new QHBoxLayout(thresholdRow);
        thresholdLayout->setContentsMargins(20, 0, 0, 4);
        auto* thresholdText = new QLabel(tr("等值阈值"), thresholdRow);
        thresholdText->setObjectName(QStringLiteral("muted"));
        auto* spin = new QSpinBox(thresholdRow);
        spin->setRange(-1000, 4000);
        spin->setValue(skin ? 500 : 1150);
        spin->setSuffix(QStringLiteral(" HU*"));
        thresholdLayout->addWidget(thresholdText);
        thresholdLayout->addStretch();
        thresholdLayout->addWidget(spin);
        layout->addWidget(thresholdRow);

        if (skin) {
            connect(check, &QCheckBox::toggled, m_viewer, &MedicalViewer::setSkinVisible);
            connect(slider, &QSlider::valueChanged, m_viewer, &MedicalViewer::setSkinOpacity);
            connect(spin, qOverload<int>(&QSpinBox::valueChanged), m_viewer,
                    &MedicalViewer::setSkinThreshold);
            }
        else {
            connect(check, &QCheckBox::toggled, m_viewer, &MedicalViewer::setBoneVisible);
            connect(slider, &QSlider::valueChanged, m_viewer, &MedicalViewer::setBoneOpacity);
            connect(spin, qOverload<int>(&QSpinBox::valueChanged), m_viewer,
                    &MedicalViewer::setBoneThreshold);
            }
        connect(slider, &QSlider::valueChanged, value,
        [value](int v) {
            value->setText(QString::number(v) + QStringLiteral("%"));
            });
        };
    addTissueControl(tr("皮肤表面"), QStringLiteral("skinDot"), true);
    addTissueControl(tr("骨骼表面"), QStringLiteral("boneDot"), false);

    auto* note = new QLabel(tr("* FullHead 为偏移后的 CT 标量，阈值沿用 VTK MedicalDemo3。"), sidebar);
    note->setObjectName(QStringLiteral("footnote"));
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addWidget(separator(sidebar));
    auto* debugToggle = new QToolButton(sidebar);
    debugToggle->setObjectName(QStringLiteral("debugToggle"));
    debugToggle->setText(tr("▸ 调试面板"));
    debugToggle->setCheckable(true);
    layout->addWidget(debugToggle);
    auto* debugPanel = new QWidget(sidebar);
    debugPanel->setObjectName(QStringLiteral("debugPanel"));
    auto* debugLayout = new QVBoxLayout(debugPanel);
    debugLayout->setContentsMargins(12, 10, 12, 10);
    auto* iterationRow = new QWidget(debugPanel);
    auto* iterationLayout = new QHBoxLayout(iterationRow);
    iterationLayout->setContentsMargins(0, 0, 0, 0);
    auto* iterationLabel = new QLabel(tr("表面平滑迭代"), iterationRow);
    auto* iterationSpin = new QSpinBox(iterationRow);
    iterationSpin->setRange(0, 100);
    iterationSpin->setValue(15);
    iterationSpin->setToolTip(tr("0 表示不平滑；数值越大，后台重建耗时越长。"));
    iterationLayout->addWidget(iterationLabel);
    iterationLayout->addStretch();
    iterationLayout->addWidget(iterationSpin);
    auto* debugHint = new QLabel(tr("修改后会在后台重新生成皮肤和骨骼表面。"), debugPanel);
    debugHint->setObjectName(QStringLiteral("footnote"));
    debugHint->setWordWrap(true);
    debugLayout->addWidget(iterationRow);
    debugLayout->addWidget(debugHint);
    debugPanel->setVisible(false);
    layout->addWidget(debugPanel);
    connect(debugToggle, &QToolButton::toggled, debugPanel, &QWidget::setVisible);
    connect(debugToggle, &QToolButton::toggled, debugToggle, [debugToggle](bool open) {
        debugToggle->setText(open ? QObject::tr("▾ 调试面板") : QObject::tr("▸ 调试面板"));
        });
    connect(iterationSpin, qOverload<int>(&QSpinBox::valueChanged),
            m_viewer, &MedicalViewer::setSmoothingIterations);
    layout->addStretch();

    auto* resetButton = new QPushButton(tr("重置全部视图"), sidebar);
    resetButton->setObjectName(QStringLiteral("secondaryButton"));
    connect(resetButton, &QPushButton::clicked, [this] { m_viewer->resetCameras(); });
    layout->addWidget(resetButton);

    m_themeButton = new QToolButton(sidebar);
    m_themeButton->setText(tr("切换日间 / 夜间模式"));
    m_themeButton->setObjectName(QStringLiteral("themeButton"));
    m_themeButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(m_themeButton, &QToolButton::clicked, this, &MainWindow::toggleTheme);
    layout->addWidget(m_themeButton);
    return sidebar;
    }

void MainWindow::createActions() {
    auto* fileMenu = menuBar()->addMenu(tr("文件(&F)"));
    auto* openAction = fileMenu->addAction(tr("打开体数据…"), this, &MainWindow::openVolume);
    openAction->setShortcut(QKeySequence::Open);
    fileMenu->addSeparator();
    m_exportSkinAction = fileMenu->addAction(tr("导出皮肤 STL…"), this, &MainWindow::exportSkin);
    m_exportBoneAction = fileMenu->addAction(tr("导出骨骼 STL…"), this, &MainWindow::exportBone);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("退出"), qApp, &QApplication::quit, QKeySequence::Quit);

    auto* viewMenu = menuBar()->addMenu(tr("视图(&V)"));
    viewMenu->addAction(tr("重置视图"), m_viewer, &MedicalViewer::resetCameras,
                        QKeySequence(QStringLiteral("R")));
    viewMenu->addAction(tr("切换主题"), this, &MainWindow::toggleTheme,
                        QKeySequence(QStringLiteral("Ctrl+T")));

    auto* helpMenu = menuBar()->addMenu(tr("帮助(&H)"));
    helpMenu->addAction(tr("关于"), this, [this] {
        QMessageBox::about(this, tr("关于 vtk3D"),
                           tr("<h3>vtk3D %1</h3>"
           "<p>基于 Qt 与 VTK 的 CT 多平面显示和表面重建演示程序。</p>"
           "<p>数据与算法参考 VTK MedicalDemo3。</p>").arg(QCoreApplication::applicationVersion()));
        });
    setDataActionsEnabled(false);
    }

void MainWindow::openVolume() {
    const QString start = m_viewer->loadedFile().isEmpty()
                          ? QDir::current().filePath(QStringLiteral("data")) : m_viewer->loadedFile();
    const QString fileName = QFileDialog::getOpenFileName(
                                 this, tr("打开 MetaImage 体数据"), start,
                                 tr("MetaImage (*.mhd *.mha);;所有文件 (*.*)"));
    if (fileName.isEmpty()) return;

    statusBar()->showMessage(tr("正在加载 %1…").arg(QFileInfo(fileName).fileName()));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = m_viewer->loadMetaImage(fileName, &error);
    QApplication::restoreOverrideCursor();
    if (!ok) QMessageBox::critical(this, tr("加载失败"), error);
    }

void MainWindow::tryLoadDefaultData() {
    const QStringList candidates = {
        QDir::current().filePath(QStringLiteral("data/FullHead.mhd")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data/FullHead.mhd")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../data/FullHead.mhd"))
        };
    for (const QString& candidate : candidates) {
        if (!QFileInfo::exists(candidate)) continue;
        QString error;
        if (m_viewer->loadMetaImage(candidate, &error)) return;
        }
    }

void MainWindow::exportSkin() {
    exportSurface(true);
    }
void MainWindow::exportBone() {
    exportSurface(false);
    }

void MainWindow::exportSurface(bool skin) {
    const QString suggested = QDir(QFileInfo(m_viewer->loadedFile()).absolutePath())
                              .filePath(skin ? QStringLiteral("skin.stl") : QStringLiteral("bone.stl"));
    const QString fileName = QFileDialog::getSaveFileName(
                                 this, skin ? tr("导出皮肤表面") : tr("导出骨骼表面"), suggested,
                                 tr("STL 模型 (*.stl)"));
    if (fileName.isEmpty()) return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = skin ? m_viewer->exportSkin(fileName, &error)
                    : m_viewer->exportBone(fileName, &error);
    QApplication::restoreOverrideCursor();
    if (!ok) QMessageBox::critical(this, tr("导出失败"), error);
    else statusBar()->showMessage(tr("已提交后台导出任务"), 3000);
    }

void MainWindow::toggleTheme() {
    const auto next = m_themeManager->theme() == ThemeManager::Theme::Dark
                      ? ThemeManager::Theme::Light : ThemeManager::Theme::Dark;
    m_themeManager->apply(qApp, next);
    }

void MainWindow::setDataActionsEnabled(bool enabled) {
    if (m_exportSkinAction) m_exportSkinAction->setEnabled(enabled);
    if (m_exportBoneAction) m_exportBoneAction->setEnabled(enabled);
    }
