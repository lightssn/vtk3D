#pragma once

#include <QWidget>
#include <array>
#include <memory>

#include <vtkSmartPointer.h>

class QLabel;
class QVTKOpenGLNativeWidget;
class vtkActor;
class vtkFlyingEdges3D;
class vtkGenericOpenGLRenderWindow;
class vtkImageData;
class vtkImageViewer2;
class vtkMetaImageReader;
class vtkPolyData;
class vtkRenderer;
class QThread;

class MedicalViewer final : public QWidget
{
    Q_OBJECT

public:
    explicit MedicalViewer(QWidget* parent = nullptr);
    ~MedicalViewer() override;

    bool loadMetaImage(const QString& fileName, QString* errorMessage = nullptr);
    bool hasData() const noexcept;
    QString loadedFile() const;
    bool exportSkin(const QString& fileName, QString* errorMessage = nullptr);
    bool exportBone(const QString& fileName, QString* errorMessage = nullptr);

public slots:
    void setSkinVisible(bool visible);
    void setBoneVisible(bool visible);
    void setSkinOpacity(int percent);
    void setBoneOpacity(int percent);
    void setSkinThreshold(int value);
    void setBoneThreshold(int value);
    void setSmoothingIterations(int iterations);
    void setDarkTheme(bool dark);
    void resetCameras();

signals:
    void dataLoaded(const QString& fileName, const QString& dimensions);
    void sliceChanged(const QString& orientation, int current, int maximum);
    void operationStarted(const QString& text);
    void operationProgress(int percent, const QString& text);
    void operationFinished(const QString& text);
    void operationFailed(const QString& text);

private:
    struct SlicePane;
    struct SurfacePipeline;

    void buildUi();
    void initializeRenderers();
    void connectSliceInteraction(SlicePane& pane);
    void updateSliceLabel(SlicePane& pane);
    void requestReconstruction();
    void startReconstruction(vtkSmartPointer<vtkImageData> image, const QString& fileName,
                             bool isNewData);
    void applyReconstruction(vtkSmartPointer<vtkImageData> image,
                             vtkSmartPointer<vtkPolyData> skin,
                             vtkSmartPointer<vtkPolyData> bone,
                             const QString& fileName, bool isNewData);
    bool exportSurface(SurfacePipeline& surface, const QString& fileName,
                       QString* errorMessage);
    void renderAll();

    std::array<std::unique_ptr<SlicePane>, 3> m_slices;
    std::unique_ptr<SurfacePipeline> m_skin;
    std::unique_ptr<SurfacePipeline> m_bone;
    QVTKOpenGLNativeWidget* m_volumeWidget = nullptr;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_volumeWindow;
    vtkSmartPointer<vtkRenderer> m_volumeRenderer;
    vtkSmartPointer<vtkImageData> m_image;
    QThread* m_workerThread = nullptr;
    QString m_loadedFile;
    bool m_hasData = false;
    bool m_dark = true;
    bool m_rebuildPending = false;
    int m_smoothingIterations = 15;
};
