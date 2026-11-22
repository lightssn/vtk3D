#pragma once
#include "algorithms/Parameters.h"
#include <QJsonObject>
#include <QWidget>
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QLabel;
class QPushButton;
class ParameterPanel final : public QWidget {
    Q_OBJECT
  public:
    explicit ParameterPanel(QWidget *parent = nullptr);
    float skinThreshold() const;
    float boneThreshold() const;
    PreprocessingParameter preprocessing() const;
    SegmentationParameter segmentation() const;
    SurfaceProcessingParameter surfaceProcessing() const;
    RegistrationParameter registration() const;
    void setVolumeInfo(const QString &, const QString &);
    void restore(const QJsonObject &);
    void setBusy(bool);
    void setStatistics(const QJsonObject &);
  signals:
    void processRequested();
    void importRequested();
    void resetRequested();
    void themeRequested();
    void cancelRequested();
    void visibilityChanged(int layer, bool visible);
    void opacityChanged(int layer, int percent);
    void overlayChanged(bool visible, int opacity);

  private:
    QSpinBox *m_skin, *m_bone, *m_radius, *m_smoothing, *m_closing, *m_registrationIterations;
    QDoubleSpinBox *m_spacing, *m_reduction, *m_registrationStep;
    QCheckBox *m_largest;
    QLabel *m_fileLabel, *m_dimensionLabel;
    QLabel *m_statistics;
    QPushButton *m_apply, *m_open;
    bool m_busy = false;
};
