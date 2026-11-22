#include "SliceView.h"
#include "CrosshairColors.h"
#include <vtkCellData.h>
#include <vtkUnsignedCharArray.h>
#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QApplication>
#include <algorithm>
#include <cmath>
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkCellPicker.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkImageActor.h>
#include <vtkImageData.h>
#include <vtkImageViewer2.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkImageMapToColors.h>
#include <vtkImageMapper3D.h>
#include <vtkImageProperty.h>
#include <vtkLookupTable.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkMath.h>
#include <vtkBillboardTextActor3D.h>

SliceView::SliceView(const QString &title, int orientation, QWidget *parent) : QWidget(parent) {
    m_orientation = orientation;
    setObjectName("viewportCard");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);
    auto *header = new QWidget(this);
    header->setObjectName("viewportHeader");
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 7, 12, 7);
    m_label = new QLabel(title, header);
    m_label->setObjectName("viewportTitle");
    headerLayout->addWidget(m_label);
    headerLayout->addStretch();
    m_sliceLabel = new QLabel(tr("未加载"), header);
    m_sliceLabel->setObjectName("viewportMeta");
    headerLayout->addWidget(m_sliceLabel);
    layout->addWidget(header);
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
    m_crosshairData = vtkSmartPointer<vtkPolyData>::New();
    vtkNew<vtkPolyDataMapper> crosshairMapper;
    crosshairMapper->SetInputData(m_crosshairData);
    crosshairMapper->SetScalarModeToUseCellData();
    crosshairMapper->SetColorModeToDirectScalars();
    crosshairMapper->ScalarVisibilityOn();
    m_crosshair = vtkSmartPointer<vtkActor>::New();
    m_crosshair->SetMapper(crosshairMapper);
    m_crosshair->GetProperty()->SetLighting(false);
    m_crosshair->GetProperty()->SetLineWidth(1);
    m_crosshair->PickableOff();
    m_crosshair->VisibilityOff();
    m_crosshair->UseBoundsOff();
    m_viewer->GetRenderer()->AddActor(m_crosshair);
    // Crosshair topology and colors never change; only the four endpoints move.
    vtkNew<vtkPoints> cursorPoints;
    cursorPoints->SetNumberOfPoints(4);
    vtkNew<vtkCellArray> cursorLines;
    vtkNew<vtkUnsignedCharArray> axisColors;
    axisColors->SetNumberOfComponents(3);
    int line = 0;
    for (int axis = 0; axis < 3; ++axis) {
        if (axis == m_orientation) continue;
        const vtkIdType ids[] = {2 * line, 2 * line + 1};
        cursorLines->InsertNextCell(2, ids);
        axisColors->InsertNextTypedTuple(CrosshairColors[axis].data());
        ++line;
    }
    m_crosshairData->SetPoints(cursorPoints);
    m_crosshairData->SetLines(cursorLines);
    m_crosshairData->GetCellData()->SetScalars(axisColors);
    m_picker = vtkSmartPointer<vtkCellPicker>::New();
    m_picker->PickFromListOn();
    m_picker->AddPickList(m_viewer->GetImageActor());

    vtkNew<vtkLookupTable> lut;
    lut->SetNumberOfTableValues(2);
    lut->SetTableRange(0, 1);
    lut->Build();
    lut->SetTableValue(0, 0, 0, 0, 0);
    lut->SetTableValue(1, 1, 0.38, 0.25, 1);
    m_labelColors = vtkSmartPointer<vtkImageMapToColors>::New();
    m_labelColors->SetLookupTable(lut);
    m_labelColors->SetOutputFormatToRGBA();
    m_overlay = vtkSmartPointer<vtkImageActor>::New();
    m_overlay->GetMapper()->SetInputConnection(m_labelColors->GetOutputPort());
    m_overlay->InterpolateOff();
    m_overlay->PickableOff();
    m_overlay->UseBoundsOff();
    m_overlay->VisibilityOff();
    m_overlay->GetProperty()->SetOpacity(0.35);
    m_viewer->GetRenderer()->AddActor(m_overlay);

    m_measurementData = vtkSmartPointer<vtkPolyData>::New();
    vtkNew<vtkPolyDataMapper> measurementMapper;
    measurementMapper->SetInputData(m_measurementData);
    m_measurementActor = vtkSmartPointer<vtkActor>::New();
    m_measurementActor->SetMapper(measurementMapper);
    m_measurementActor->GetProperty()->SetColor(1, 0.8, 0.3);
    m_measurementActor->GetProperty()->SetLighting(false);
    m_measurementActor->GetProperty()->SetLineWidth(2);
    m_measurementActor->PickableOff();
    m_measurementActor->UseBoundsOff();
    m_measurementActor->VisibilityOff();
    m_viewer->GetRenderer()->AddActor(m_measurementActor);
    for (auto &text : m_directions) {
        text = vtkSmartPointer<vtkTextActor>::New();
        text->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
        text->GetTextProperty()->SetFontSize(14);
        text->GetTextProperty()->SetColor(0.4, 0.85, 0.8);
        text->PickableOff();
        m_viewer->GetRenderer()->AddActor2D(text);
    }
    const double positions[][2] = {{0.03, 0.5}, {0.97, 0.5}, {0.5, 0.95}, {0.5, 0.03}};
    for (int i = 0; i < 4; ++i) {
        m_directions[i]->SetPosition(positions[i][0], positions[i][1]);
        m_directions[i]->GetTextProperty()->SetJustificationToCentered();
        m_directions[i]->GetTextProperty()->SetVerticalJustificationToCentered();
    }
    setMeasurementHint(tr("定位：左键点击 / 拖动 · 滚轮切片"));
    m_renderTimer.setSingleShot(true);
    m_renderTimer.setInterval(16);
    connect(&m_renderTimer, &QTimer::timeout, this, [this] {
        if (m_hasData) {
            updateDirections(); m_viewer->Render();
        }
    });
    m_dragTimer.setSingleShot(true);
    m_dragTimer.setInterval(16);
    connect(&m_dragTimer, &QTimer::timeout, this, [this] { requestCursor(m_dragPosition); });
    m_widget->setFocusPolicy(Qt::StrongFocus);
    m_widget->installEventFilter(this);
    setDarkTheme(true);
}

SliceView::~SliceView() = default;
int SliceView::currentSlice() const {
    return m_viewer->GetSlice();
}

void SliceView::setImage(vtkImageData *image) {
    if (m_hasData && m_viewer->GetInput() == image)
        return;
    m_dragTimer.stop();
    m_haveCursor = false;
    clearMeasurements();
    m_draggingCursor = false;
    m_crosshair->VisibilityOff();
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
    m_sliceLabel->setText(tr("切片 %1 / %2").arg(m_viewer->GetSlice() + 1).arg(m_viewer->GetSliceMax() + 1));
    m_widget->setToolTip(QString("%1 / %2").arg(m_viewer->GetSlice() + 1).arg(m_viewer->GetSliceMax() + 1));
}

void SliceView::resetCamera() {
    if (m_hasData) {
        m_viewer->GetRenderer()->ResetCamera();
        scheduleRender();
    }
}
void SliceView::setDarkTheme(bool dark) {
    const double b = dark ? 0.05 : 0.85;
    m_viewer->GetRenderer()->SetBackground(b, b, b);
    for (auto &text : m_directions)
        text->GetTextProperty()->SetColor(dark ? 0.4 : 0.05, dark ? 0.85 : 0.42, dark ? 0.8 : 0.4);
    if (m_hasData)
        scheduleRender();
}

bool SliceView::cursorVisible() const {
    return m_crosshair->GetVisibility() != 0;
}

void SliceView::setCursor(int x, int y, int z, bool visible) {
    if (!m_hasData)
        return;
    auto *image = m_viewer->GetInput();
    const int *extent = image->GetExtent();
    const int requested[] = {x, y, z};
    int selected[3];
    for (int axis = 0; axis < 3; ++axis)
        selected[axis] = std::clamp(requested[axis], extent[2 * axis], extent[2 * axis + 1]);
    if (m_haveCursor && std::equal(selected, selected + 3, m_cursor) && cursorVisible() == visible)
        return;
    const bool sliceChanged = !m_haveCursor || selected[m_orientation] != currentSlice();
    if (sliceChanged) endPointDrag(true);
    std::copy(selected, selected + 3, m_cursor);
    m_haveCursor = true;
    if (sliceChanged) m_pending.clear();
    m_viewer->SetSlice(m_cursor[m_orientation]);
    auto *points = m_crosshairData->GetPoints();
    int line = 0;
    for (int axis = 0; axis < 3; ++axis) {
        if (axis == m_orientation)
            continue;
        for (int end = 0; end < 2; ++end) {
            int index[] = {m_cursor[0], m_cursor[1], m_cursor[2]};
            index[axis] = extent[2 * axis + end];
            double world[3];
            image->TransformIndexToPhysicalPoint(index, world);
            points->SetPoint(2 * line + end, world);
        }
        ++line;
    }
    points->Modified();
    // Lift the lines slightly toward the camera to avoid depth fighting with the image.
    double direction[3];
    m_viewer->GetRenderer()->GetActiveCamera()->GetDirectionOfProjection(direction);
    const double *spacing = image->GetSpacing();
    const double offset = 0.01 * std::min({std::abs(spacing[0]), std::abs(spacing[1]), std::abs(spacing[2])});
    m_crosshair->SetPosition(-offset * direction[0], -offset * direction[1], -offset * direction[2]);
    m_overlay->SetPosition(-offset * 0.5 * direction[0], -offset * 0.5 * direction[1], -offset * 0.5 * direction[2]);
    m_measurementActor->SetPosition(-offset * 2 * direction[0], -offset * 2 * direction[1], -offset * 2 * direction[2]);
    if (m_hasLabels)
        m_overlay->SetDisplayExtent(m_viewer->GetImageActor()->GetDisplayExtent());
    m_crosshair->SetVisibility(visible);
    updateLabel();
    if (sliceChanged) updateMeasurements();
    scheduleRender();
}

void SliceView::requestCursor(const QPointF &position) {
    MeasurementGeometry::Point world;
    if (!pick(position, world)) return;
    double index[3];
    m_viewer->GetInput()->TransformPhysicalPointToContinuousIndex(world.data(), index);
    int selected[] = {int(std::lround(index[0])), int(std::lround(index[1])), int(std::lround(index[2]))};
    selected[m_orientation] = currentSlice();
    if (cursorVisible() && std::equal(selected, selected + 3, m_cursor)) return;
    emit cursorRequested(selected[0], selected[1], selected[2], true);
}

bool SliceView::pick(const QPointF &position, MeasurementGeometry::Point &world) {
    const double scale = m_widget->devicePixelRatioF();
    if (!m_picker->Pick(position.x() * scale, (m_widget->height() - position.y()) * scale - 1, 0,
                      m_viewer->GetRenderer()))
        return false;
    double index[3];
    auto *image = m_viewer->GetInput();
    image->TransformPhysicalPointToContinuousIndex(m_picker->GetPickPosition(), index);
    index[m_orientation] = currentSlice();
    image->TransformContinuousIndexToPhysicalPoint(index, world.data());
    return true;
}

bool SliceView::eventFilter(QObject *object, QEvent *event) {
    if (object == m_widget && m_hasData) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton && mouse->modifiers() == Qt::NoModifier) {
                m_widget->setFocus();
                // 优先命中已有端点，避免拖动端点时误加点或移动十字线。
                if (beginPointDrag(mouse->position())) {
                    m_measurementPress = true;
                    return true;
                }
                if (m_tool != 0) {
                    m_measurementPress = true;
                    addMeasurementPoint(mouse->position());
                    beginPointDrag(mouse->position());
                    return true;
                }
                m_draggingCursor = true;
                requestCursor(mouse->position());
                return true;
            }
            if (mouse->button() == Qt::RightButton && mouse->modifiers() == Qt::NoModifier) {
                endPointDrag();
                m_rightPressed = true;
                m_rightMoved = false;
                m_rightStart = mouse->position();
                m_rightScale = m_viewer->GetRenderer()->GetActiveCamera()->GetParallelScale();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_rightPressed) {
            const auto delta = static_cast<QMouseEvent *>(event)->position() - m_rightStart;
            m_rightMoved = m_rightMoved || delta.manhattanLength() >= QApplication::startDragDistance();
            if (m_rightMoved) {
                auto *renderer = m_viewer->GetRenderer();
                renderer->GetActiveCamera()->SetParallelScale(std::clamp(
                    m_rightScale * std::exp(std::clamp(delta.y() * 0.01, -10.0, 10.0)), 1e-8, 1e12));
                renderer->ResetCameraClippingRange();
                scheduleRender();
            }
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_rightPressed &&
                   static_cast<QMouseEvent *>(event)->button() == Qt::RightButton) {
            m_rightPressed = false;
            if (!m_rightMoved && m_tool != 0) finishMeasurement();
            return true;

        } else if (event->type() == QEvent::MouseMove && m_dragPoint >= 0) {
            moveMeasurementPoint(static_cast<QMouseEvent *>(event)->position());
            return true;
        } else if (event->type() == QEvent::MouseMove && m_draggingCursor) {
            m_dragPosition = static_cast<QMouseEvent *>(event)->position();
            if (!m_dragTimer.isActive()) m_dragTimer.start();
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_draggingCursor) {
            if (static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
                m_draggingCursor = false;
                m_dragTimer.stop();
                requestCursor(static_cast<QMouseEvent *>(event)->position());
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease && m_measurementPress) {
            const auto button = static_cast<QMouseEvent *>(event)->button();
            if (button == Qt::LeftButton || button == Qt::RightButton) {
                if (m_dragPoint >= 0) {
                    moveMeasurementPoint(static_cast<QMouseEvent *>(event)->position());
                    endPointDrag();
                }
                m_measurementPress = false;
                return true;
            }
        } else if (event->type() == QEvent::KeyPress) {
            const int key = static_cast<QKeyEvent *>(event)->key();
            if (key == Qt::Key_Escape) {
                if (m_dragPoint >= 0) endPointDrag(true);
                else m_pending.clear();
                updateMeasurements(); return true;
            }
            if (key == Qt::Key_Backspace) { undoMeasurement(); return true; }
            if (key == Qt::Key_Return || key == Qt::Key_Enter) {
                endPointDrag(); finishMeasurement(); return true;
            }
        } else if (event->type() == QEvent::FocusOut) {
            endPointDrag(true);
            m_rightPressed = false;
        } else if (event->type() == QEvent::Wheel) {
            endPointDrag(true);
            m_dragTimer.stop();
            auto *wheel = static_cast<QWheelEvent *>(event);
            const int delta = wheel->angleDelta().y();
            if (delta == 0)
                return true;
            int selected[] = {m_cursor[0], m_cursor[1], m_cursor[2]};
            selected[m_orientation] = std::clamp(currentSlice() + (delta > 0 ? 1 : -1),
                                                 m_viewer->GetSliceMin(), m_viewer->GetSliceMax());
            emit cursorRequested(selected[0], selected[1], selected[2], false);
            return true;
        }
    }
    return QWidget::eventFilter(object, event);
}

void SliceView::scheduleRender() {
    if (m_hasData && !m_renderTimer.isActive()) m_renderTimer.start();
}
void SliceView::setLabels(vtkImageData *labels) {
    m_hasLabels = labels != nullptr;
    m_labelColors->SetInputData(labels);
    if (labels) m_overlay->SetDisplayExtent(m_viewer->GetImageActor()->GetDisplayExtent());
    m_overlay->SetVisibility(m_hasLabels && m_overlayVisible);
    scheduleRender();
}
void SliceView::setOverlay(bool visible, int opacity) {
    m_overlayVisible = visible;
    m_overlay->SetVisibility(m_hasLabels && visible);
    m_overlay->GetProperty()->SetOpacity(std::clamp(opacity, 0, 100) / 100.0);
    scheduleRender();
}
void SliceView::setTool(int tool) {
    endPointDrag(true);
    m_tool = tool;
    m_draggingCursor = false;
    m_dragTimer.stop();
    m_pending.clear();
    updateMeasurements();
}
void SliceView::clearMeasurements() {
    endPointDrag(true);
    m_pending.clear();
    for (const auto &m : m_measurements) m_viewer->GetRenderer()->RemoveActor(m.label);
    m_measurements.clear();
    if (m_measurementData) updateMeasurements();
}
void SliceView::undoMeasurement() {
    endPointDrag(true);
    if (!m_pending.empty()) m_pending.pop_back();
    else {
        for (auto it = m_measurements.end(); it != m_measurements.begin();) {
            --it;
            if (it->slice == currentSlice()) {
                m_viewer->GetRenderer()->RemoveActor(it->label);
                m_measurements.erase(it); break;
            }
        }
    }
    updateMeasurements();
}
bool SliceView::beginPointDrag(const QPointF &position) {
    double best = 10 * 10; // 使用逻辑像素命中，适配高 DPI。
    int selectedMeasurement = -2, selectedPoint = -1;
    const double scale = m_widget->devicePixelRatioF();
    auto *renderer = m_viewer->GetRenderer();
    auto find = [&](const std::vector<MeasurementGeometry::Point> &points, int measurement) {
        for (int i = 0; i < int(points.size()); ++i) {
            renderer->SetWorldPoint(points[i][0], points[i][1], points[i][2], 1);
            renderer->WorldToDisplay();
            const auto *display = renderer->GetDisplayPoint();
            const QPointF screen(display[0] / scale, m_widget->height() - (display[1] + 1) / scale);
            const auto delta = screen - position;
            const double squared = delta.x() * delta.x() + delta.y() * delta.y();
            if (squared < best) {
                best = squared;
                selectedMeasurement = measurement;
                selectedPoint = i;
            }
        }
    };
    find(m_pending, -1);
    for (int i = int(m_measurements.size()) - 1; i >= 0; --i)
        if (m_measurements[i].slice == currentSlice()) find(m_measurements[i].points, i);
    if (selectedPoint < 0) return false;
    m_dragMeasurement = selectedMeasurement;
    m_dragPoint = selectedPoint;
    m_dragOriginal = selectedMeasurement < 0 ? m_pending[selectedPoint] :
        m_measurements[selectedMeasurement].points[selectedPoint];
    m_widget->setCursor(Qt::ClosedHandCursor);
    return true;
}
bool SliceView::updateMeasurement(Measurement &m) {
    try {
        const double value = m.tool == 1 ? MeasurementGeometry::distance(m.points[0], m.points[1]) :
                                          MeasurementGeometry::area(m.points, m_orientation);
        if (value < 1e-6) throw std::runtime_error("退化测量");
        m.text = m.tool == 1 ? tr("距离 %1 mm").arg(value, 0, 'f', 2) :
                               tr("面积 %1 mm²").arg(value, 0, 'f', 2);
        const QString caption = QString::number(value, 'f', 2) + (m.tool == 1 ? " mm" : " mm^2");
        m.label->SetInput(caption.toUtf8().constData());
        m.label->SetPosition(m.points.back().data());
        return true;
    } catch (const std::exception &) {
        m.text = tr("轮廓无效：边交叉或测量退化");
        m.label->SetInput("Invalid");
        return false;
    }
}
void SliceView::moveMeasurementPoint(const QPointF &position) {
    MeasurementGeometry::Point world;
    if (m_dragPoint < 0 || !pick(position, world)) return;
    if (m_dragMeasurement < 0) m_pending[m_dragPoint] = world;
    else {
        auto &measurement = m_measurements[m_dragMeasurement];
        measurement.points[m_dragPoint] = world;
        updateMeasurement(measurement);
        emit measurementChanged(measurement.text);
    }
    updateMeasurements();
}
void SliceView::endPointDrag(bool cancel) {
    if (m_dragPoint < 0) return;
    bool reverted = false;
    if (m_dragMeasurement < 0) {
        if (cancel) m_pending[m_dragPoint] = m_dragOriginal;
    } else {
        auto &measurement = m_measurements[m_dragMeasurement];
        if (cancel || !updateMeasurement(measurement)) {
            measurement.points[m_dragPoint] = m_dragOriginal;
            updateMeasurement(measurement);
            reverted = !cancel;
        }
        emit measurementChanged(measurement.text);
    }
    m_dragPoint = -1;
    m_dragMeasurement = -2;
    m_measurementPress = false;
    m_widget->unsetCursor();
    updateMeasurements();
    if (reverted) setMeasurementHint(tr("拖动位置无效，已恢复原位置"));
}
void SliceView::addMeasurementPoint(const QPointF &position) {
    MeasurementGeometry::Point world;
    if (!pick(position, world)) return;
    if (!m_pending.empty() && MeasurementGeometry::distance(m_pending.back(), world) < 1e-6) return;
    m_pending.push_back(world);
    if (m_tool == 1 && m_pending.size() == 2) finishMeasurement();
    else updateMeasurements();
}
void SliceView::finishMeasurement() {
    if (m_tool == 0) return;
    if ((m_tool == 1 && m_pending.size() != 2) || (m_tool == 2 && m_pending.size() < 3)) return;
    try {
        auto label = vtkSmartPointer<vtkBillboardTextActor3D>::New();
        Measurement measurement{currentSlice(), m_tool, m_pending, {}, label};
        if (!updateMeasurement(measurement)) throw std::runtime_error("轮廓无效");
        label->GetTextProperty()->SetFontSize(14);
        label->GetTextProperty()->SetColor(1, 0.85, 0.35);
        label->GetTextProperty()->SetBackgroundColor(0.05, 0.07, 0.08);
        label->GetTextProperty()->SetBackgroundOpacity(0.7);
        label->GetTextProperty()->SetShadow(true);
        label->SetDisplayOffset(8, 8);
        label->PickableOff();
        label->UseBoundsOff();
        m_viewer->GetRenderer()->AddActor(label);
        m_measurements.push_back(std::move(measurement));
        m_pending.clear();
        updateMeasurements();
        emit measurementChanged(m_measurements.back().text);
    } catch (const std::exception &) {
        setMeasurementHint(tr("轮廓无效：不能交叉或退化；退格撤销，Esc 取消"));
    }
}
void SliceView::updateMeasurements() {
    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> lines, vertices;
    auto append = [&](const std::vector<MeasurementGeometry::Point> &path, bool closed) {
        const vtkIdType start = points->GetNumberOfPoints();
        for (const auto &p : path) {
            const vtkIdType id = points->InsertNextPoint(p.data());
            vertices->InsertNextCell(1, &id);
        }
        for (vtkIdType i = 1; i < vtkIdType(path.size()); ++i) {
            const vtkIdType ids[] = {start + i - 1, start + i};
            lines->InsertNextCell(2, ids);
        }
        if (closed && path.size() > 2) {
            const vtkIdType ids[] = {points->GetNumberOfPoints() - 1, start};
            lines->InsertNextCell(2, ids);
        }
    };
    QStringList results;
    for (const auto &m : m_measurements) {
        m.label->SetVisibility(m.slice == currentSlice());
        if (m.slice != currentSlice()) continue;
        append(m.points, m.tool == 2);
        results << m.text;
    }
    append(m_pending, false);
    m_measurementData->SetPoints(points);
    m_measurementData->SetLines(lines);
    m_measurementData->SetVerts(vertices);
    m_measurementActor->GetProperty()->SetPointSize(6);
    m_measurementActor->SetVisibility(points->GetNumberOfPoints() != 0);
    QString hint = m_tool == 0 ? tr("定位：左键拖动 · 滚轮切片 · 右键拖动缩放") : m_tool == 1 ?
        tr("测距：依次点击两点 · Esc 取消") : tr("面积：依次点选 · 右键 / Enter 闭合 · Esc 取消");
    if (!results.empty()) hint = results.last() + tr(" · 本层 %1 项").arg(results.size());
    if (!m_pending.empty()) hint = tr("已选 %1 点 · ").arg(m_pending.size()) + hint;
    setMeasurementHint(hint);
    m_widget->setToolTip(results.isEmpty() ? hint : results.join('\n'));
    scheduleRender();
}
void SliceView::setMeasurementHint(const QString &text) {
    m_measurementHint = text;
    emit measurementHintChanged(text);
}
void SliceView::setPatientCoordinates(bool enabled) {
    m_patientCoordinates = enabled;
    scheduleRender();
}
void SliceView::updateDirections() {
    auto *camera = m_viewer->GetRenderer()->GetActiveCamera();
    double forward[3], up[3], right[3];
    camera->GetDirectionOfProjection(forward);
    camera->GetViewUp(up);
    vtkMath::Cross(forward, up, right);
    vtkMath::Normalize(right);
    vtkMath::Cross(right, forward, up);
    auto name = [this](const double *vector, double sign) {
        int axis = 0;
        for (int i = 1; i < 3; ++i) if (std::abs(vector[i]) > std::abs(vector[axis])) axis = i;
        const bool positive = vector[axis] * sign > 0;
        const char *physical[] = {"X", "Y", "Z"};
        const char *lpsPositive[] = {"L", "P", "S"}, *lpsNegative[] = {"R", "A", "I"};
        return m_patientCoordinates ? QString(positive ? lpsPositive[axis] : lpsNegative[axis]) :
            QString(positive ? "+" : "-") + physical[axis];
    };
    const QString labels[] = {name(right, -1), name(right, 1), name(up, 1), name(up, -1)};
    for (int i = 0; i < 4; ++i) m_directions[i]->SetInput(labels[i].toUtf8().constData());
}
