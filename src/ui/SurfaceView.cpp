#include "SurfaceView.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkActor.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkNew.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
SurfaceView::SurfaceView(QWidget *parent) : QWidget(parent) {
    setObjectName("viewportCard");
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("三维重建 · 左键旋转 / 滚轮缩放"), this));
    auto *widget = new QVTKOpenGLNativeWidget(this);
    layout->addWidget(widget, 1);
    widget->setMinimumSize(240, 200);
    m_window = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    widget->setRenderWindow(m_window);
    m_renderer = vtkSmartPointer<vtkRenderer>::New();
    m_window->AddRenderer(m_renderer);
    for (int i = 0; i < 2; ++i) {
        m_actors[i] = vtkSmartPointer<vtkActor>::New();
        vtkNew<vtkPolyDataMapper> mapper;
        mapper->ScalarVisibilityOff();
        m_actors[i]->SetMapper(mapper);
        m_actors[i]->GetProperty()->SetColor(i ? 0.92 : 0.94, i ? 0.9 : 0.68, i ? 0.78 : 0.56);
        m_actors[i]->GetProperty()->SetOpacity(i ? 1 : 0.34);
        m_renderer->AddActor(m_actors[i]);
    }
    setDarkTheme(true);
}
SurfaceView::~SurfaceView() = default;
void SurfaceView::setSurfaces(vtkPolyData *skin, vtkPolyData *bone) {
    vtkPolyDataMapper::SafeDownCast(m_actors[0]->GetMapper())->SetInputData(skin);
    vtkPolyDataMapper::SafeDownCast(m_actors[1]->GetMapper())->SetInputData(bone);
    resetCamera();
}
void SurfaceView::setLayerVisible(int layer, bool visible) {
    m_actors[layer]->SetVisibility(visible);
    m_window->Render();
}
void SurfaceView::setOpacity(int layer, int percent) {
    m_actors[layer]->GetProperty()->SetOpacity(percent / 100.0);
    m_window->Render();
}
void SurfaceView::setDarkTheme(bool dark) {
    m_renderer->SetBackground(dark ? 0.05 : 0.85, dark ? 0.07 : 0.87, dark ? 0.09 : 0.9);
}
void SurfaceView::resetCamera() {
    m_renderer->ResetCamera();
    m_renderer->ResetCameraClippingRange();
    m_window->Render();
}
