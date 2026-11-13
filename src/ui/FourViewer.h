#pragma once
#include <QWidget>
#include <vtkSmartPointer.h>
class SliceView;
class SurfaceView;
class vtkImageData;
class vtkPolyData;
class FourViewer final : public QWidget {
    Q_OBJECT
  public:
    explicit FourViewer(QWidget *parent = nullptr);
    ~FourViewer() override;
    void setData(vtkImageData *, vtkPolyData *skin, vtkPolyData *bone);
  public slots:
    void setDarkTheme(bool);
    void resetCameras();
    void setSkinVisible(bool);
    void setBoneVisible(bool);
    void setSkinOpacity(int);
    void setBoneOpacity(int);

  private:
    SliceView *m_slices[3];
    SurfaceView *m_surface;
};
