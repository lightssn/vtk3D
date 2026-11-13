#include "SliceView.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>
#include <QWheelEvent>
#include <algorithm>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkImageActor.h>
#include <vtkImageData.h>
#include <vtkImageViewer2.h>
#include <vtkNew.h>
#include <vtkRenderer.h>
SliceView::SliceView(const QString &title, int orientation, QWidget *parent) : QWidget(parent) {
    setObjectName("viewportCard");
    auto *layout = new QVBoxLayout(this);
    m_label = new QLabel(title, this);
    m_label->setObjectName("viewportTitle");
    layout->addWidget(m_label);
    m_widget = new QVTKOpenGLNativeWidget(this);
    layout->addWidget(m_widget, 1);
    m_widget->setMinimumSize(240, 200);
    vtkNew<vtkGenericOpenGLRenderWindow> window;
    m_widget->setRenderWindow(window);
    m_viewer = vtkSmartPointer<vtkImageViewer2>::New();
    m_viewer->SetRenderWindow(window);
    m_viewer->SetupInteractor(window->GetInteractor());
    m_viewer->SetSliceOrientation(orientation);
    vtkNew<vtkImageData> placeholder;
    placeholder->SetDimensions(1, 1, 1);
    placeholder->AllocateScalars(VTK_FLOAT, 1);
    placeholder->SetScalarComponentFromFloat(0, 0, 0, 0, 0);
    m_viewer->SetInputData(placeholder);
    m_viewer->GetImageActor()->SetVisibility(false);
    m_widget->installEventFilter(this);
    setDarkTheme(true);
}
SliceView::~SliceView() = default;
int SliceView::currentSlice() const {
    return m_viewer->GetSlice();
}
void SliceView::setImage(vtkImageData *image) {
    m_viewer->SetInputData(image);
    m_viewer->GetImageActor()->SetVisibility(true);
    m_hasData = true;
    m_viewer->SetColorWindow(2000);
    m_viewer->SetColorLevel(1000);
    m_viewer->SetSlice((m_viewer->GetSliceMin() + m_viewer->GetSliceMax()) / 2);
    updateLabel();
    resetCamera();
}
void SliceView::updateLabel() {
    m_widget->setToolTip(QString("%1 / %2").arg(m_viewer->GetSlice() + 1).arg(m_viewer->GetSliceMax() + 1));
}
void SliceView::resetCamera() {
    if (m_hasData) {
        m_viewer->GetRenderer()->ResetCamera();
        m_viewer->Render();
    }
}
void SliceView::setDarkTheme(bool dark) {
    const double b = dark ? 0.05 : 0.85;
    m_viewer->GetRenderer()->SetBackground(b, b, b);
    if (m_hasData)
        m_viewer->Render();
}
bool SliceView::eventFilter(QObject *object, QEvent *event) {
    if (object == m_widget && event->type() == QEvent::Wheel && m_hasData) {
        auto *wheel = static_cast<QWheelEvent *>(event);
        m_viewer->SetSlice(std::clamp(m_viewer->GetSlice() + (wheel->angleDelta().y() > 0 ? 1 : -1),
                                      m_viewer->GetSliceMin(), m_viewer->GetSliceMax()));
        updateLabel();
        m_viewer->Render();
        return true;
    }
    return QWidget::eventFilter(object, event);
}
