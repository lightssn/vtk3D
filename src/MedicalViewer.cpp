#include "MedicalViewer.h"

#include <QEvent>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>
#include <QWheelEvent>
#include <QThread>
#include <QPointer>

#include <vtkActor.h>
#include <vtkFlyingEdges3D.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkImageData.h>
#include <vtkImageProperty.h>
#include <vtkImageViewer2.h>
#include <vtkMetaImageReader.h>
#include <vtkNew.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyDataNormals.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSTLWriter.h>
#include <vtkWindowedSincPolyDataFilter.h>

#include <algorithm>
#include <exception>
#include <functional>
#include <stdexcept>

namespace {
class SliceEventFilter final : public QObject {
    public:
        explicit SliceEventFilter(std::function<void(int)> callback, QObject* parent)
            : QObject(parent), m_callback(std::move(callback)) {}

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override {
            Q_UNUSED(watched)
            if (event->type() == QEvent::Wheel) {
                const auto* wheel = static_cast<QWheelEvent*>(event);
                m_callback(wheel->angleDelta().y() >= 0 ? 1 : -1);
                return true;
                }
            return false;
            }

    private:
        std::function<void(int)> m_callback;
    };
}

struct MedicalViewer::SlicePane {
    QString title;
    int orientation = vtkImageViewer2::SLICE_ORIENTATION_XY;
    QWidget* frame = nullptr;
    QLabel* titleLabel = nullptr;
    QLabel* sliceLabel = nullptr;
    QVTKOpenGLNativeWidget* widget = nullptr;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> window;
    vtkSmartPointer<vtkImageViewer2> viewer;
    };

struct MedicalViewer::SurfacePipeline {
    double threshold = 0.0;
    vtkSmartPointer<vtkPolyData> data;
    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    };

MedicalViewer::MedicalViewer(QWidget* parent)
    : QWidget(parent),
      m_skin(std::make_unique<SurfacePipeline>()),
      m_bone(std::make_unique<SurfacePipeline>()) {
    buildUi();
    initializeRenderers();
    }

MedicalViewer::~MedicalViewer() {
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait();
        }
    }

void MedicalViewer::buildUi() {
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(8);

    const std::array<QString, 3> titles = {
        tr("轴状面 · Axial"), tr("冠状面 · Coronal"), tr("矢状面 · Sagittal")
        };
    const std::array<int, 3> orientations = {
        vtkImageViewer2::SLICE_ORIENTATION_XY,
        vtkImageViewer2::SLICE_ORIENTATION_XZ,
        vtkImageViewer2::SLICE_ORIENTATION_YZ
        };

    for (std::size_t i = 0; i < m_slices.size(); ++i) {
        auto pane = std::make_unique<SlicePane>();
        pane->title = titles[i];
        pane->orientation = orientations[i];
        pane->frame = new QWidget(this);
        pane->frame->setObjectName(QStringLiteral("viewportCard"));
        auto* layout = new QVBoxLayout(pane->frame);
        layout->setContentsMargins(1, 1, 1, 1);
        layout->setSpacing(0);

        auto* header = new QWidget(pane->frame);
        header->setObjectName(QStringLiteral("viewportHeader"));
        auto* headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(12, 7, 12, 7);
        pane->titleLabel = new QLabel(titles[i], header);
        pane->titleLabel->setObjectName(QStringLiteral("viewportTitle"));
        pane->sliceLabel = new QLabel(tr("未加载"), header);
        pane->sliceLabel->setObjectName(QStringLiteral("viewportMeta"));
        headerLayout->addWidget(pane->titleLabel);
        headerLayout->addStretch();
        headerLayout->addWidget(pane->sliceLabel);

        pane->widget = new QVTKOpenGLNativeWidget(pane->frame);
        pane->widget->setMinimumSize(280, 220);
        layout->addWidget(header);
        layout->addWidget(pane->widget, 1);
        grid->addWidget(pane->frame, static_cast<int>(i / 2), static_cast<int>(i % 2));
        m_slices[i] = std::move(pane);
        }

    auto* volumeFrame = new QWidget(this);
    volumeFrame->setObjectName(QStringLiteral("viewportCard"));
    auto* volumeLayout = new QVBoxLayout(volumeFrame);
    volumeLayout->setContentsMargins(1, 1, 1, 1);
    volumeLayout->setSpacing(0);
    auto* volumeHeader = new QWidget(volumeFrame);
    volumeHeader->setObjectName(QStringLiteral("viewportHeader"));
    auto* volumeHeaderLayout = new QHBoxLayout(volumeHeader);
    volumeHeaderLayout->setContentsMargins(12, 7, 12, 7);
    auto* volumeTitle = new QLabel(tr("三维重建 · 3D Reconstruction"), volumeHeader);
    volumeTitle->setObjectName(QStringLiteral("viewportTitle"));
    auto* interactionHint = new QLabel(tr("左键旋转  ·  滚轮缩放  ·  中键平移"), volumeHeader);
    interactionHint->setObjectName(QStringLiteral("viewportMeta"));
    volumeHeaderLayout->addWidget(volumeTitle);
    volumeHeaderLayout->addStretch();
    volumeHeaderLayout->addWidget(interactionHint);
    m_volumeWidget = new QVTKOpenGLNativeWidget(volumeFrame);
    m_volumeWidget->setMinimumSize(280, 220);
    volumeLayout->addWidget(volumeHeader);
    volumeLayout->addWidget(m_volumeWidget, 1);
    grid->addWidget(volumeFrame, 1, 1);

    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    }

void MedicalViewer::initializeRenderers() {
    for (auto& pane : m_slices) {
        pane->window = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
        pane->widget->setRenderWindow(pane->window);
        pane->viewer = vtkSmartPointer<vtkImageViewer2>::New();
        pane->viewer->SetRenderWindow(pane->window);
        pane->viewer->SetupInteractor(pane->window->GetInteractor());
        pane->viewer->SetSliceOrientation(pane->orientation);
        pane->viewer->GetRenderer()->SetBackground(0.035, 0.055, 0.075);
        connectSliceInteraction(*pane);
        }

    m_volumeWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    m_volumeWidget->setRenderWindow(m_volumeWindow);
    m_volumeRenderer = vtkSmartPointer<vtkRenderer>::New();
    m_volumeRenderer->SetBackground(0.035, 0.055, 0.075);
    m_volumeRenderer->SetBackground2(0.09, 0.13, 0.17);
    m_volumeRenderer->GradientBackgroundOn();
    m_volumeWindow->AddRenderer(m_volumeRenderer);

    auto configureSurface = [this](SurfacePipeline& surface, double threshold,
    double r, double g, double b, double opacity) {
        surface.threshold = threshold;
        surface.mapper->ScalarVisibilityOff();
        surface.actor->SetMapper(surface.mapper);
        surface.actor->GetProperty()->SetColor(r, g, b);
        surface.actor->GetProperty()->SetOpacity(opacity);
        surface.actor->GetProperty()->SetInterpolationToPBR();
        surface.actor->GetProperty()->SetMetallic(0.0);
        surface.actor->GetProperty()->SetRoughness(0.48);
        m_volumeRenderer->AddActor(surface.actor);
        };
    configureSurface(*m_skin, 500.0, 0.94, 0.68, 0.56, 0.34);
    configureSurface(*m_bone, 1150.0, 0.92, 0.90, 0.78, 1.0);
    }

void MedicalViewer::connectSliceInteraction(SlicePane& pane) {
    auto callback = [this, &pane](int delta) {
        if (!m_hasData) return;
        const int next = std::clamp(pane.viewer->GetSlice() + delta,
                                    pane.viewer->GetSliceMin(), pane.viewer->GetSliceMax());
        pane.viewer->SetSlice(next);
        updateSliceLabel(pane);
        pane.viewer->Render();
        };
    pane.widget->installEventFilter(new SliceEventFilter(std::move(callback), pane.widget));
    }

bool MedicalViewer::loadMetaImage(const QString& fileName, QString* errorMessage) {
    const QFileInfo info(fileName);
    if (!info.isFile()) {
        if (errorMessage) *errorMessage = tr("数据文件不存在。");
        return false;
        }
    if (m_workerThread && m_workerThread->isRunning()) {
        if (errorMessage) *errorMessage = tr("已有后台任务正在运行，请稍候。");
        return false;
        }

    const QString absolutePath = info.absoluteFilePath();
    emit operationStarted(tr("正在读取体数据…"));
    QPointer<MedicalViewer> self(this);
    const double skinThreshold = m_skin->threshold;
    const double boneThreshold = m_bone->threshold;
    const int iterations = m_smoothingIterations;
    m_workerThread = QThread::create([self, absolutePath, skinThreshold, boneThreshold, iterations] {
        auto progress = [self](int value, const QString& text) {
            if (!self) return;
            QMetaObject::invokeMethod(self, [self, value, text] {
                if (self) emit self->operationProgress(value, text);
                }, Qt::QueuedConnection);
            };
        try {
            progress(5, QObject::tr("读取 MetaImage 数据"));
            vtkNew<vtkMetaImageReader> reader;
            reader->SetFileName(absolutePath.toUtf8().constData());
            reader->Update();
            vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
            image->DeepCopy(reader->GetOutput());
            if (image->GetNumberOfPoints() == 0) throw std::runtime_error("No volume data");

            auto build = [&](double threshold, int base, const QString& label) {
                progress(base, QObject::tr("提取%1等值面").arg(label));
                vtkNew<vtkFlyingEdges3D> contour;
                contour->SetInputData(image);
                contour->SetValue(0, threshold);
                vtkNew<vtkWindowedSincPolyDataFilter> smoother;
                smoother->SetInputConnection(contour->GetOutputPort());
                smoother->SetNumberOfIterations(iterations);
                smoother->SetPassBand(0.08);
                smoother->BoundarySmoothingOff();
                smoother->FeatureEdgeSmoothingOff();
                smoother->NormalizeCoordinatesOn();
                progress(base + 12, QObject::tr("平滑%1表面（%2 次）").arg(label).arg(iterations));
                vtkNew<vtkPolyDataNormals> normals;
                normals->SetInputConnection(smoother->GetOutputPort());
                normals->SetFeatureAngle(60.0);
                normals->ConsistencyOn();
                normals->AutoOrientNormalsOn();
                normals->Update();
                vtkSmartPointer<vtkPolyData> result = vtkSmartPointer<vtkPolyData>::New();
                result->DeepCopy(normals->GetOutput());
                return result;
                };
            auto skin = build(skinThreshold, 20, QObject::tr("皮肤"));
            auto bone = build(boneThreshold, 58, QObject::tr("骨骼"));
            progress(95, QObject::tr("准备显示结果"));
            if (self) QMetaObject::invokeMethod(self, [self, image, skin, bone, absolutePath] {
                if (self) self->applyReconstruction(image, skin, bone, absolutePath, true);
                }, Qt::QueuedConnection);
            }
        catch (const std::exception& exception) {
            const QString message = QObject::tr("后台处理失败：%1").arg(QString::fromUtf8(exception.what()));
            if (self) QMetaObject::invokeMethod(self, [self, message] {
                if (self) emit self->operationFailed(message);
                }, Qt::QueuedConnection);
            }
        });
    connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);
    connect(m_workerThread, &QThread::finished, this, [this] {
        m_workerThread = nullptr;
        if (m_rebuildPending) {
            m_rebuildPending = false;
            requestReconstruction();
            }
        });
    m_workerThread->start();
    return true;
    }

void MedicalViewer::requestReconstruction() {
    if (!m_hasData || !m_image) return;
    if (m_workerThread && m_workerThread->isRunning()) {
        m_rebuildPending = true;
        return;
        }
    startReconstruction(m_image, m_loadedFile, false);
    }

void MedicalViewer::startReconstruction(vtkSmartPointer<vtkImageData> image,
                                        const QString& fileName, bool isNewData) {
    emit operationStarted(tr("正在后台重建表面…"));
    QPointer<MedicalViewer> self(this);
    const double skinThreshold = m_skin->threshold;
    const double boneThreshold = m_bone->threshold;
    const int iterations = m_smoothingIterations;
    m_workerThread = QThread::create([self, image, fileName, isNewData,
    skinThreshold, boneThreshold, iterations] {
        auto build = [self, image, iterations](double threshold, int base, const QString& label) {
            if (self) QMetaObject::invokeMethod(self, [self, base, label] {
                if (self) emit self->operationProgress(base, QObject::tr("正在重建%1").arg(label));
                }, Qt::QueuedConnection);
            vtkNew<vtkFlyingEdges3D> contour;
            contour->SetInputData(image);
            contour->SetValue(0, threshold);
            vtkNew<vtkWindowedSincPolyDataFilter> smoother;
            smoother->SetInputConnection(contour->GetOutputPort());
            smoother->SetNumberOfIterations(iterations);
            smoother->SetPassBand(0.08);
            smoother->NormalizeCoordinatesOn();
            vtkNew<vtkPolyDataNormals> normals;
            normals->SetInputConnection(smoother->GetOutputPort());
            normals->AutoOrientNormalsOn();
            normals->ConsistencyOn();
            normals->Update();
            vtkSmartPointer<vtkPolyData> result = vtkSmartPointer<vtkPolyData>::New();
            result->DeepCopy(normals->GetOutput());
            return result;
            };
        auto skin = build(skinThreshold, 15, QObject::tr("皮肤表面"));
        auto bone = build(boneThreshold, 55, QObject::tr("骨骼表面"));
        if (self) QMetaObject::invokeMethod(self, [self, image, skin, bone, fileName, isNewData] {
            if (self) self->applyReconstruction(image, skin, bone, fileName, isNewData);
            }, Qt::QueuedConnection);
        });
    connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);
    connect(m_workerThread, &QThread::finished, this, [this] {
        m_workerThread = nullptr;
        if (m_rebuildPending) {
            m_rebuildPending = false;
            requestReconstruction();
            }
        });
    m_workerThread->start();
    }

void MedicalViewer::applyReconstruction(vtkSmartPointer<vtkImageData> image,
                                        vtkSmartPointer<vtkPolyData> skin,
                                        vtkSmartPointer<vtkPolyData> bone,
                                        const QString& fileName, bool isNewData) {
    m_image = image;
    m_skin->data = skin;
    m_bone->data = bone;
    m_skin->mapper->SetInputData(skin);
    m_bone->mapper->SetInputData(bone);
    if (isNewData) {
        for (auto& pane : m_slices) {
            pane->viewer->SetInputData(image);
            pane->viewer->SetColorWindow(2000.0);
            pane->viewer->SetColorLevel(1000.0);
            pane->viewer->SetSlice((pane->viewer->GetSliceMin() + pane->viewer->GetSliceMax()) / 2);
            }
        m_hasData = true;
        m_loadedFile = fileName;
        for (auto& pane : m_slices) updateSliceLabel(*pane);
        int dimensions[3] {};
        image->GetDimensions(dimensions);
        emit dataLoaded(fileName, QStringLiteral("%1 × %2 × %3")
                        .arg(dimensions[0]).arg(dimensions[1]).arg(dimensions[2]));
        resetCameras();
        }
    else {
        renderAll();
        }
    emit operationProgress(100, tr("处理完成"));
    emit operationFinished(tr("后台重建完成"));
    }

bool MedicalViewer::hasData() const noexcept {
    return m_hasData;
    }
QString MedicalViewer::loadedFile() const {
    return m_loadedFile;
    }

void MedicalViewer::updateSliceLabel(SlicePane& pane) {
    const int current = pane.viewer->GetSlice() - pane.viewer->GetSliceMin() + 1;
    const int maximum = pane.viewer->GetSliceMax() - pane.viewer->GetSliceMin() + 1;
    pane.sliceLabel->setText(tr("切片 %1 / %2").arg(current).arg(maximum));
    emit sliceChanged(pane.title, current, maximum);
    }

void MedicalViewer::setSkinVisible(bool visible) {
    m_skin->actor->SetVisibility(visible);
    renderAll();
    }

void MedicalViewer::setBoneVisible(bool visible) {
    m_bone->actor->SetVisibility(visible);
    renderAll();
    }

void MedicalViewer::setSkinOpacity(int percent) {
    m_skin->actor->GetProperty()->SetOpacity(percent / 100.0);
    renderAll();
    }

void MedicalViewer::setBoneOpacity(int percent) {
    m_bone->actor->GetProperty()->SetOpacity(percent / 100.0);
    renderAll();
    }

void MedicalViewer::setSkinThreshold(int value) {
    m_skin->threshold = value;
    requestReconstruction();
    }

void MedicalViewer::setBoneThreshold(int value) {
    m_bone->threshold = value;
    requestReconstruction();
    }

void MedicalViewer::setSmoothingIterations(int iterations) {
    m_smoothingIterations = std::clamp(iterations, 0, 100);
    requestReconstruction();
    }

void MedicalViewer::setDarkTheme(bool dark) {
    m_dark = dark;
    const std::array<double, 3> background = dark
        ? std::array<double, 3> {0.035, 0.055, 0.075}
        :
        std::array<double, 3> {0.82, 0.86, 0.89};
    for (auto& pane : m_slices)
        pane->viewer->GetRenderer()->SetBackground(background.data());
    m_volumeRenderer->SetBackground(background.data());
    if (dark) m_volumeRenderer->SetBackground2(0.09, 0.13, 0.17);
    else m_volumeRenderer->SetBackground2(0.96, 0.97, 0.98);
    renderAll();
    }

void MedicalViewer::resetCameras() {
    if (!m_hasData) return;
    for (auto& pane : m_slices) {
        pane->viewer->GetRenderer()->ResetCamera();
        pane->viewer->Render();
        }
    m_volumeRenderer->ResetCamera();
    m_volumeRenderer->ResetCameraClippingRange();
    m_volumeWindow->Render();
    }

bool MedicalViewer::exportSkin(const QString& fileName, QString* errorMessage) {
    return exportSurface(*m_skin, fileName, errorMessage);
    }

bool MedicalViewer::exportBone(const QString& fileName, QString* errorMessage) {
    return exportSurface(*m_bone, fileName, errorMessage);
    }

bool MedicalViewer::exportSurface(SurfacePipeline& surface, const QString& fileName,
                                  QString* errorMessage) {
    if (!m_hasData) {
        if (errorMessage) *errorMessage = tr("请先加载体数据。");
        return false;
        }
    vtkPolyData* output = surface.data;
    if (!output || output->GetNumberOfCells() == 0) {
        if (errorMessage) *errorMessage = tr("当前阈值没有生成可导出的表面。");
        return false;
        }
    if (m_workerThread && m_workerThread->isRunning()) {
        if (errorMessage) *errorMessage = tr("后台正在处理，请完成后再导出。");
        return false;
        }
    const QString target = QFileInfo(fileName).absoluteFilePath();
    vtkSmartPointer<vtkPolyData> data = surface.data;
    QPointer<MedicalViewer> self(this);
    emit operationStarted(tr("正在后台写入 STL…"));
    m_workerThread = QThread::create([self, data, target] {
        if (self) QMetaObject::invokeMethod(self, [self] {
            if (self) emit self->operationProgress(35, QObject::tr("正在编码三角网格"));
            }, Qt::QueuedConnection);
        vtkNew<vtkSTLWriter> writer;
        writer->SetFileName(target.toUtf8().constData());
        writer->SetInputData(data);
        writer->SetFileTypeToBinary();
        writer->Write();
        const bool ok = writer->GetErrorCode() == 0;
        if (self) QMetaObject::invokeMethod(self, [self, ok, target] {
            if (!self) return;
            if (ok) {
                emit self->operationProgress(100, QObject::tr("STL 写入完成"));
                emit self->operationFinished(QObject::tr("已导出：%1").arg(target));
                }
            else {
                emit self->operationFailed(QObject::tr("STL 文件写入失败：%1").arg(target));
                }
            }, Qt::QueuedConnection);
        });
    connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);
    connect(m_workerThread, &QThread::finished, this, [this] { m_workerThread = nullptr; });
    m_workerThread->start();
    return true;
    }

void MedicalViewer::renderAll() {
    if (!m_hasData) return;
    for (auto& pane : m_slices) pane->viewer->Render();
    m_volumeWindow->Render();
    }
