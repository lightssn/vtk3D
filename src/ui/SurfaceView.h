#pragma once
#include <QWidget>
#include <vtkSmartPointer.h>
class vtkActor;
class vtkRenderer;
class vtkGenericOpenGLRenderWindow;
class vtkPolyData;
class SurfaceView final : public QWidget {
    Q_OBJECT
  public:
    explicit SurfaceView(QWidget *parent = nullptr);
    ~SurfaceView() override;
    void setSurfaces(vtkPolyData *skin, vtkPolyData *bone);
    void setLayerVisible(int layer, bool visible);
    void setOpacity(int layer, int percent);
    void setDarkTheme(bool);
    void resetCamera();

  private:
    vtkSmartPointer<vtkActor> m_actors[2];
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_window;
};
