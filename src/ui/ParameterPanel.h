#pragma once
#include "algorithms/Parameters.h"
#include <QJsonObject>
#include <QWidget>
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
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
    void restore(const QJsonObject &);
  signals:
    void processRequested();
    void visibilityChanged(int layer, bool visible);
    void opacityChanged(int layer, int percent);

  private:
    QSpinBox *m_skin, *m_bone, *m_radius, *m_smoothing, *m_closing, *m_registrationIterations;
    QDoubleSpinBox *m_spacing, *m_reduction, *m_registrationStep;
    QCheckBox *m_largest;
};
