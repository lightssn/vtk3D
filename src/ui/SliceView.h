#pragma once
#include <QWidget>
#include <vtkSmartPointer.h>
#include "algorithms/MeasurementGeometry.h"
#include <QTimer>
class vtkImageData;
class vtkImageViewer2;
class vtkActor;
class vtkPolyData;
class QVTKOpenGLNativeWidget;
class QLabel;
class vtkImageMapToColors;
class vtkImageActor;
class vtkTextActor;
class vtkCellPicker;
class vtkBillboardTextActor3D;
class SliceView final : public QWidget {
    Q_OBJECT
  public:
    SliceView(const QString &title, int orientation, QWidget *parent = nullptr);
    ~SliceView() override;
    void setImage(vtkImageData *);
    void setDarkTheme(bool);
    void resetCamera();
    int currentSlice() const;
    void setCursor(int x, int y, int z, bool visible = true);
    bool cursorVisible() const;
    void setLabels(vtkImageData *);
    void setOverlay(bool visible, int opacity);
    void setTool(int tool);
    void clearMeasurements();
    void undoMeasurement();
    int measurementCount() const { return int(m_measurements.size()); }
    void setPatientCoordinates(bool enabled);
    QString measurementHint() const { return m_measurementHint; }
  signals:
    void cursorRequested(int x, int y, int z, bool activate = true);
    void measurementChanged(const QString &text);
    void measurementHintChanged(const QString &text);

  protected:
    bool eventFilter(QObject *, QEvent *) override;

  private:
    vtkSmartPointer<vtkImageViewer2> m_viewer;
    QVTKOpenGLNativeWidget *m_widget;
    QLabel *m_label;
    QLabel *m_sliceLabel;
    bool m_hasData = false;
    bool m_draggingCursor = false;
    int m_orientation;
    int m_cursor[3] = {};
    vtkSmartPointer<vtkActor> m_crosshair;
    vtkSmartPointer<vtkPolyData> m_crosshairData;
    vtkSmartPointer<vtkImageActor> m_overlay;
    vtkSmartPointer<vtkImageMapToColors> m_labelColors;
    vtkSmartPointer<vtkCellPicker> m_picker;
    vtkSmartPointer<vtkActor> m_measurementActor;
    vtkSmartPointer<vtkPolyData> m_measurementData;
    vtkSmartPointer<vtkTextActor> m_directions[4];
    QString m_measurementHint;
    void setMeasurementHint(const QString &);
    struct Measurement {
        int slice;
        int tool;
        std::vector<MeasurementGeometry::Point> points;
        QString text;
        vtkSmartPointer<vtkBillboardTextActor3D> label;
    };
    std::vector<Measurement> m_measurements;
    std::vector<MeasurementGeometry::Point> m_pending;
    int m_tool = 0;
    bool m_overlayVisible = false, m_hasLabels = false, m_patientCoordinates = false;
    QTimer m_renderTimer, m_dragTimer;
    QPointF m_dragPosition;
    bool m_haveCursor = false;
    bool m_measurementPress = false;
    bool m_rightPressed = false, m_rightMoved = false;
    QPointF m_rightStart;
    double m_rightScale = 1;
    int m_dragMeasurement = -2; // -2：未拖动，-1：未完成的测量，其余为测量下标。
    int m_dragPoint = -1;
    MeasurementGeometry::Point m_dragOriginal{};
    void scheduleRender();
    bool pick(const QPointF &, MeasurementGeometry::Point &);
    void addMeasurementPoint(const QPointF &);
    void finishMeasurement();
    bool beginPointDrag(const QPointF &);
    void moveMeasurementPoint(const QPointF &);
    void endPointDrag(bool cancel = false);
    bool updateMeasurement(Measurement &);
    void updateMeasurements();
    void updateDirections();
    void requestCursor(const QPointF &position);
    void updateLabel();
};
