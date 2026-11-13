#pragma once
#include "ProjectData.h"
class SegmentationWriter {
  public:
    void write(const SegmentationData &, const QString &) const;
};
class SurfaceWriter {
  public:
    void write(const SurfaceData &, const QString &) const;
};
class ProjectRepository {
  public:
    void save(const ProjectData &, const QString &) const;
    ProjectData load(const QString &) const;
};
