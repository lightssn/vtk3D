#pragma once
#include "ProjectData.h"
#include "foundation/Task.h"
#include <QStringList>
class VolumeReader {
  public:
    VolumeData read(const QString &, const Task & = {}) const;
};
class DicomReader {
  public:
    QStringList series(const QString &directory) const;
    VolumeData read(const QString &directory, const QString &seriesUid = {}, const Task & = {}) const;
};
