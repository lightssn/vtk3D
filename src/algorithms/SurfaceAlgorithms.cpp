#include "ImagingAlgorithms.h"
#include <cmath>
#include <cstring>
#include <vtkCallbackCommand.h>
#include <vtkFlyingEdges3D.h>
#include <vtkImageReslice.h>
#include <vtkMatrix3x3.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyDataNormals.h>
#include <vtkQuadricDecimation.h>
#include <vtkWindowedSincPolyDataFilter.h>

namespace {
template <class Filter> void execute(Filter *filter, const Task &task) {
    vtkNew<vtkCallbackCommand> observer;
    observer->SetClientData(const_cast<Task *>(&task));
    observer->SetCallback([](vtkObject *caller, unsigned long, void *data, void *) {
        if (static_cast<Task *>(data)->cancellation->load())
            vtkAlgorithm::SafeDownCast(caller)->SetAbortExecute(1);
    });
    filter->AddObserver(vtkCommand::ProgressEvent, observer);
    task.check();
    filter->Update();
    task.check();
    if (filter->GetErrorCode())
        throw std::runtime_error("VTK processing failed");
}
template <class Image> vtkSmartPointer<vtkImageData> convert(const Image *image, int type) {
    if (!image)
        throw std::runtime_error("No image to convert");
    auto result = vtkSmartPointer<vtkImageData>::New();
    const auto size = image->GetLargestPossibleRegion().GetSize();
    result->SetDimensions(static_cast<int>(size[0]), static_cast<int>(size[1]), static_cast<int>(size[2]));
    auto first = image->GetLargestPossibleRegion().GetIndex();
    typename Image::PointType origin;
    image->TransformIndexToPhysicalPoint(first, origin);
    result->SetOrigin(origin.GetDataPointer());
    result->SetSpacing(image->GetSpacing().GetDataPointer());
    vtkNew<vtkMatrix3x3> direction;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            direction->SetElement(i, j, image->GetDirection()[i][j]);
    result->SetDirectionMatrix(direction);
    result->AllocateScalars(type, 1);
    std::memcpy(result->GetScalarPointer(), image->GetBufferPointer(),
                image->GetLargestPossibleRegion().GetNumberOfPixels() * sizeof(typename Image::PixelType));
    return result; // Owned copy: no ITK pipeline lifetime dependency.
}
} // namespace
vtkSmartPointer<vtkImageData> ImageBridge::toVtk(const VolumeData &v) {
    return convert(v.image.GetPointer(), VTK_FLOAT);
}
vtkSmartPointer<vtkImageData> ImageBridge::toVtk(const SegmentationData &v) {
    return convert(v.image.GetPointer(), VTK_UNSIGNED_SHORT);
}
vtkSmartPointer<vtkImageData> ImageBridge::forDisplay(const VolumeData &v) {
    auto input = toVtk(v);
    double bounds[6];
    input->GetBounds(bounds);
    const auto *spacing = input->GetSpacing();
    vtkNew<vtkImageReslice> reslice;
    reslice->SetInputData(input);
    reslice->SetOutputDirection(1, 0, 0, 0, 1, 0, 0, 0, 1);
    reslice->SetOutputOrigin(bounds[0], bounds[2], bounds[4]);
    reslice->SetOutputSpacing(spacing);
    reslice->SetOutputExtent(0, static_cast<int>(std::ceil((bounds[1] - bounds[0]) / spacing[0])), 0,
                             static_cast<int>(std::ceil((bounds[3] - bounds[2]) / spacing[1])), 0,
                             static_cast<int>(std::ceil((bounds[5] - bounds[4]) / spacing[2])));
    reslice->SetInterpolationModeToLinear();
    reslice->SetBackgroundLevel(-1024);
    reslice->Update();
    vtkSmartPointer<vtkImageData> result = reslice->GetOutput();
    return result;
}
SurfaceData SurfaceReconstructor::run(vtkImageData *image, const ReconstructionParameter &p,
                                      const Task &task) const {
    if (!image)
        throw std::runtime_error("No reconstruction image");
    task.report(60, "VTK surface extraction");
    vtkNew<vtkFlyingEdges3D> filter;
    filter->SetInputData(image);
    filter->SetValue(0, p.isoValue);
    execute(filter.GetPointer(), task);
    SurfaceData out{vtkSmartPointer<vtkPolyData>::New()};
    out.mesh->DeepCopy(filter->GetOutput());
    return out;
}
SurfaceData SurfaceProcessor::run(const SurfaceData &input, const SurfaceProcessingParameter &p,
                                  const Task &task) const {
    if (!input.mesh || p.smoothingIterations < 0 || p.smoothingIterations > 100 || p.reduction < 0 ||
        p.reduction >= 1)
        throw std::runtime_error("Invalid surface processing input");
    SurfaceData out = input;
    if (!out.mesh->GetNumberOfCells())
        return out;
    task.report(75, "VTK surface processing");
    if (p.smoothingIterations) {
        vtkNew<vtkWindowedSincPolyDataFilter> filter;
        filter->SetInputData(out.mesh);
        filter->SetNumberOfIterations(p.smoothingIterations);
        filter->SetPassBand(0.08);
        filter->BoundarySmoothingOff();
        filter->FeatureEdgeSmoothingOff();
        filter->NormalizeCoordinatesOn();
        execute(filter.GetPointer(), task);
        out.mesh = filter->GetOutput();
        task.check();
    }
    if (p.reduction) {
        vtkNew<vtkQuadricDecimation> filter;
        filter->SetInputData(out.mesh);
        filter->SetTargetReduction(p.reduction);
        execute(filter.GetPointer(), task);
        out.mesh = filter->GetOutput();
        task.check();
    }
    if (p.normals) {
        vtkNew<vtkPolyDataNormals> filter;
        filter->SetInputData(out.mesh);
        filter->ConsistencyOn();
        filter->AutoOrientNormalsOn();
        filter->SetFeatureAngle(60);
        execute(filter.GetPointer(), task);
        out.mesh = filter->GetOutput();
        task.check();
    }
    return out;
}
