#include "FourViewer.h"
#include "SliceView.h"
#include "SurfaceView.h"
#include <QGridLayout>
#include <QScopedValueRollback>
#include <algorithm>
#include <vtkImageData.h>
#include <QVBoxLayout>
#include <QApplication>
FourViewer::FourViewer(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    auto *grid = new QGridLayout;
    layout->addLayout(grid, 1);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(8);
    const QString titles[] = {tr("轴状面 · Axial"), tr("冠状面 · Coronal"), tr("矢状面 · Sagittal")};
    const int orientations[] = {2, 1, 0};
    for (int i = 0; i < 3; ++i) {
        m_slices[i] = new SliceView(titles[i], orientations[i], this);
        grid->addWidget(m_slices[i], i / 2, i % 2);
        connect(m_slices[i], &SliceView::cursorRequested, this, &FourViewer::updateCursor);
        connect(m_slices[i], &SliceView::measurementHintChanged, this, [this, i](const QString &text) {
            if (m_activeSlice == i) emit measurementHintChanged(text);
        });
    }
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        for (int i = 0; i < 3; ++i) if (now && m_slices[i]->isAncestorOf(now)) {
            m_activeSlice = i;
            emit measurementHintChanged(m_slices[i]->measurementHint());
        }
    });
    m_surface = new SurfaceView(this);
    grid->addWidget(m_surface, 1, 1);
    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
}
FourViewer::~FourViewer() = default;
void FourViewer::setData(vtkImageData *image, vtkPolyData *skin, vtkPolyData *bone,
                         const VolumeData &source, vtkImageData *labels) {
    if (!image) return;
    const bool changedImage = m_image != image;
    m_image = image;
    m_source = source;
    if (changedImage) { m_cursorVisible = false; m_haveCursor = false; }
    for (auto *view : m_slices) {
        view->setImage(image);
        view->setLabels(labels);
        view->setPatientCoordinates(source.patientCoordinates);
    }
    m_surface->setSurfaces(skin, bone);
    const int *e = image->GetExtent();
    if (changedImage) updateCursor((e[0] + e[1]) / 2, (e[2] + e[3]) / 2, (e[4] + e[5]) / 2, false);
    else { m_haveCursor = false; updateCursor(m_cursor[0], m_cursor[1], m_cursor[2], false); }
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
void FourViewer::setTool(int tool) {
    for (auto *view : m_slices) view->setTool(tool);
}
void FourViewer::undoMeasurement() { m_slices[m_activeSlice]->undoMeasurement(); }
void FourViewer::clearMeasurements() {
    for (auto *view : m_slices) view->clearMeasurements();
}
void FourViewer::setOverlay(bool visible, int opacity) {
    for (auto *view : m_slices) view->setOverlay(visible, opacity);
}

void FourViewer::updateCursor(int x, int y, int z, bool activate) {
    if (m_updatingCursor)
        return;
    if (!m_image) return;
    QScopedValueRollback<bool> guard(m_updatingCursor, true);
    const int *extent = m_image->GetExtent();
    const int index[] = {std::clamp(x, extent[0], extent[1]), std::clamp(y, extent[2], extent[3]),
                         std::clamp(z, extent[4], extent[5])};
    const bool visible = m_cursorVisible || activate;
    if (m_haveCursor && std::equal(index, index + 3, m_cursor) && visible == m_cursorVisible) return;
    std::copy(index, index + 3, m_cursor);
    m_haveCursor = true;
    m_cursorVisible = m_cursorVisible || activate;
    // Programmatic updates never emit cursorRequested: only user input starts a broadcast.
    for (auto *view : m_slices)
        view->setCursor(index[0], index[1], index[2], m_cursorVisible);
    if (m_image) {
        double world[3];
        m_image->TransformIndexToPhysicalPoint(index, world);
        const double *bounds = m_image->GetBounds();
        const double size =
            0.08 * std::max({bounds[1] - bounds[0], bounds[3] - bounds[2], bounds[5] - bounds[4]});
        m_surface->setCursor(world, size, m_cursorVisible);
        QString value = tr("原图范围外");
        if (m_source.image) {
            VolumeImage::PointType point;
            for (int axis = 0; axis < 3; ++axis) point[axis] = world[axis];
            VolumeImage::IndexType sourceIndex;
            if (m_source.image->TransformPhysicalPointToIndex(point, sourceIndex))
                value = tr("原图 IJK %1, %2, %3\n灰度 %4")
                    .arg(sourceIndex[0]).arg(sourceIndex[1]).arg(sourceIndex[2])
                    .arg(m_source.image->GetPixel(sourceIndex), 0, 'f', 1);
        }
        emit coordinatesChanged(tr("显示 IJK  %1, %2, %3\nXYZ  %4, %5, %6 mm\n%7\n%8")
            .arg(index[0]).arg(index[1]).arg(index[2])
            .arg(world[0], 0, 'f', 2).arg(world[1], 0, 'f', 2).arg(world[2], 0, 'f', 2)
            .arg(value, m_source.patientCoordinates ? tr("方向 LPS") : tr("方向为物理轴；单位按输入几何")));
    }
}
