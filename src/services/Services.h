#pragma once
#include "algorithms/Parameters.h"
#include "data/ProjectData.h"
#include "foundation/Task.h"
#include <QObject>
#include <vtkImageData.h>

class ProjectService final : public QObject {
    Q_OBJECT
  public:
    explicit ProjectService(QObject *parent = nullptr) : QObject(parent) {}
    const ProjectData &data() const {
        return m_data;
    }
    void replace(ProjectData data) {
        m_data = std::move(data);
        emit changed();
    }
    void save(const ProjectData &, const QString &) const;
    ProjectData open(const QString &) const;
  signals:
    void changed();

  private:
    ProjectData m_data;
};
class ImportService {
  public:
    QStringList series(const QString &) const;
    ProjectData import(const QString &, const QString &uid = {}, const Task & = {}) const;
};
class ProcessingService {
  public:
    vtkSmartPointer<vtkImageData> displayImage(const VolumeData &, const Task & = {}) const;
    ProjectData process(const ProjectData &, float skinThreshold, float boneThreshold,
                        const PreprocessingParameter &, const SegmentationParameter &,
                        const SurfaceProcessingParameter &, const Task & = {}) const;
    ProjectData registerVolume(const ProjectData &fixed, const VolumeData &moving,
                               const RegistrationParameter &, const Task & = {}) const;
};
class ExportService {
  public:
    void surface(const SurfaceData &, const QString &, const Task & = {}) const;
    void segmentation(const SegmentationData &, const QString &, const Task & = {}) const;
};
