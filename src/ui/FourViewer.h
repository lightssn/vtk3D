#pragma once
#include <QWidget>
#include <vtkSmartPointer.h>
#include "data/ProjectData.h"
class SliceView;
class SurfaceView;
class vtkImageData;
class vtkPolyData;
class QLabel;
class FourViewer final : public QWidget {
    Q_OBJECT
  public:
    explicit FourViewer(QWidget *parent = nullptr);
    ~FourViewer() override;
    void setData(vtkImageData *, vtkPolyData *skin, vtkPolyData *bone,
                 const VolumeData &, vtkImageData *labels = nullptr);
  public slots:
    void setDarkTheme(bool);
    void resetCameras();
    void setSkinVisible(bool);
    void setBoneVisible(bool);
    void setSkinOpacity(int);
    void setBoneOpacity(int);
    void setTool(int);
    void undoMeasurement();
    void clearMeasurements();
    void setOverlay(bool visible, int opacity);
  signals:
    void coordinatesChanged(const QString &);
    void measurementHintChanged(const QString &);

  private:
    SliceView *m_slices[3];
    SurfaceView *m_surface;
    bool m_updatingCursor = false;
    bool m_cursorVisible = false;
    vtkSmartPointer<vtkImageData> m_image;
    VolumeData m_source;
    int m_cursor[3] = {};
    bool m_haveCursor = false;
    int m_activeSlice = 0;
    void updateCursor(int x, int y, int z, bool activate);
};
