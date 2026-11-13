#pragma once
#include "Parameters.h"
#include "data/ProjectData.h"
#include "foundation/Task.h"
#include <vtkImageData.h>

class VolumePreprocessor {
  public:
    VolumeData run(const VolumeData &, const PreprocessingParameter &, const Task & = {}) const;
};
class VolumeSegmenter {
  public:
    SegmentationData run(const VolumeData &, const SegmentationParameter &, const Task & = {}) const;
};
class VolumeRegistrator {
  public:
    VolumeData run(const VolumeData &fixed, const VolumeData &moving, const RegistrationParameter &,
                   const Task & = {}) const;
};
namespace ImageBridge {
vtkSmartPointer<vtkImageData> toVtk(const VolumeData &);
vtkSmartPointer<vtkImageData> toVtk(const SegmentationData &);
vtkSmartPointer<vtkImageData> forDisplay(const VolumeData &);
} // namespace ImageBridge
class SurfaceReconstructor {
  public:
    SurfaceData run(vtkImageData *, const ReconstructionParameter &, const Task & = {}) const;
};
class SurfaceProcessor {
  public:
    SurfaceData run(const SurfaceData &, const SurfaceProcessingParameter &, const Task & = {}) const;
};
