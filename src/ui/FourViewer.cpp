#include "FourViewer.h"
#include "SliceView.h"
#include "SurfaceView.h"
#include <QGridLayout>
FourViewer::FourViewer(QWidget *parent) : QWidget(parent) {
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    const QString titles[] = {tr("轴状面"), tr("冠状面"), tr("矢状面")};
    const int orientations[] = {2, 1, 0};
    for (int i = 0; i < 3; ++i) {
        m_slices[i] = new SliceView(titles[i], orientations[i], this);
        grid->addWidget(m_slices[i], i / 2, i % 2);
    }
    m_surface = new SurfaceView(this);
    grid->addWidget(m_surface, 1, 1);
    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
}
FourViewer::~FourViewer() = default;
void FourViewer::setData(vtkImageData *image, vtkPolyData *skin, vtkPolyData *bone) {
    for (auto *view : m_slices)
        view->setImage(image);
    m_surface->setSurfaces(skin, bone);
}
void FourViewer::setDarkTheme(bool dark) {
    for (auto *view : m_slices)
        view->setDarkTheme(dark);
    m_surface->setDarkTheme(dark);
}
void FourViewer::resetCameras() {
    for (auto *view : m_slices)
        view->resetCamera();
    m_surface->resetCamera();
}
void FourViewer::setSkinVisible(bool v) {
    m_surface->setLayerVisible(0, v);
}
void FourViewer::setBoneVisible(bool v) {
    m_surface->setLayerVisible(1, v);
}
void FourViewer::setSkinOpacity(int v) {
    m_surface->setOpacity(0, v);
}
void FourViewer::setBoneOpacity(int v) {
    m_surface->setOpacity(1, v);
}
