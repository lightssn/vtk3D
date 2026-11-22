#pragma once
#include <QWidget>
#include <vtkSmartPointer.h>
#include <QTimer>
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
    void setCursor(const double position[3], double size, bool visible);
    bool cursorVisible() const;

  private:
    vtkSmartPointer<vtkActor> m_actors[2];
    vtkSmartPointer<vtkActor> m_cursor;
    vtkSmartPointer<vtkPolyData> m_cursorData;
    vtkSmartPointer<vtkRenderer> m_cursorRenderer;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_window;
    QTimer m_renderTimer;
    void scheduleRender();
};
