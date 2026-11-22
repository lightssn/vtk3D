#include "SurfaceView.h"
#include "CrosshairColors.h"
#include <vtkCellData.h>
#include <vtkUnsignedCharArray.h>
#include <QLabel>
#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
SurfaceView::SurfaceView(QWidget *parent) : QWidget(parent) {
    setObjectName("viewportCard");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);
    auto *header = new QWidget(this);
    header->setObjectName("viewportHeader");
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 7, 12, 7);
    auto *title = new QLabel(tr("三维重建 · 3D Reconstruction"), header);
    title->setObjectName("viewportTitle");
    auto *hint = new QLabel(tr("左键旋转 · 滚轮缩放 · 中键平移"), header);
    hint->setObjectName("viewportMeta");
    headerLayout->addWidget(title);
    headerLayout->addStretch();
    headerLayout->addWidget(hint);
    layout->addWidget(header);
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
    // A transparent overlay keeps the locator visible even inside an opaque bone surface.
    m_window->SetNumberOfLayers(2);
    m_cursorRenderer = vtkSmartPointer<vtkRenderer>::New();
    m_cursorRenderer->SetLayer(1);
    m_cursorRenderer->InteractiveOff();
    m_cursorRenderer->SetActiveCamera(m_renderer->GetActiveCamera());
    m_window->AddRenderer(m_cursorRenderer);
    m_cursorData = vtkSmartPointer<vtkPolyData>::New();
    vtkNew<vtkPolyDataMapper> cursorMapper;
    cursorMapper->SetInputData(m_cursorData);
    cursorMapper->SetScalarModeToUseCellData();
    cursorMapper->SetColorModeToDirectScalars();
    cursorMapper->ScalarVisibilityOn();
    m_cursor = vtkSmartPointer<vtkActor>::New();
    m_cursor->SetMapper(cursorMapper);
    m_cursor->GetProperty()->SetLighting(false);
    m_cursor->GetProperty()->SetLineWidth(2);
    m_cursor->PickableOff();
    m_cursor->UseBoundsOff();
    m_cursor->VisibilityOff();
    m_cursorRenderer->AddActor(m_cursor);
    vtkNew<vtkPoints> points;
    points->SetNumberOfPoints(6);
    vtkNew<vtkCellArray> lines;
    vtkNew<vtkUnsignedCharArray> colors;
    colors->SetNumberOfComponents(3);
    for (int axis = 0; axis < 3; ++axis) {
        const vtkIdType ids[] = {2 * axis, 2 * axis + 1};
        lines->InsertNextCell(2, ids);
        colors->InsertNextTypedTuple(CrosshairColors[axis].data());
    }
    m_cursorData->SetPoints(points);
    m_cursorData->SetLines(lines);
    m_cursorData->GetCellData()->SetScalars(colors);
    m_renderTimer.setSingleShot(true);
    m_renderTimer.setInterval(16);
    connect(&m_renderTimer, &QTimer::timeout, this, [this] { m_window->Render(); });
    setDarkTheme(true);
}
SurfaceView::~SurfaceView() = default;
void SurfaceView::setSurfaces(vtkPolyData *skin, vtkPolyData *bone) {
    m_cursor->VisibilityOff();
    vtkPolyDataMapper::SafeDownCast(m_actors[0]->GetMapper())->SetInputData(skin);
    vtkPolyDataMapper::SafeDownCast(m_actors[1]->GetMapper())->SetInputData(bone);
    if (skin || bone) resetCamera();
    else scheduleRender();
}
void SurfaceView::setLayerVisible(int layer, bool visible) {
    m_actors[layer]->SetVisibility(visible);
    scheduleRender();
}
void SurfaceView::setOpacity(int layer, int percent) {
    m_actors[layer]->GetProperty()->SetOpacity(percent / 100.0);
    scheduleRender();
}
void SurfaceView::setDarkTheme(bool dark) {
    m_renderer->SetBackground(dark ? 0.05 : 0.85, dark ? 0.07 : 0.87, dark ? 0.09 : 0.9);
    scheduleRender();
}
void SurfaceView::resetCamera() {
    m_renderer->ResetCamera();
    m_renderer->ResetCameraClippingRange();
    scheduleRender();
}

bool SurfaceView::cursorVisible() const {
    return m_cursor->GetVisibility() != 0;
}
void SurfaceView::setCursor(const double position[3], double size, bool visible) {
    auto *points = m_cursorData->GetPoints();
    for (int axis = 0; axis < 3; ++axis) {
        for (int end = 0; end < 2; ++end) {
            double point[] = {position[0], position[1], position[2]};
            point[axis] += (end ? 1 : -1) * size;
            points->SetPoint(2 * axis + end, point);
        }
    }
    points->Modified();
    m_cursor->SetVisibility(visible);
    scheduleRender();
}
void SurfaceView::scheduleRender() {
    if (!m_renderTimer.isActive()) m_renderTimer.start();
}
