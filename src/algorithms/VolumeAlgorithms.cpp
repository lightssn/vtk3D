#include "ImagingAlgorithms.h"
#include <cmath>
#include <itkBinaryBallStructuringElement.h>
#include <itkBinaryMorphologicalClosingImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkCenteredTransformInitializer.h>
#include <itkCommand.h>
#include <itkConnectedComponentImageFilter.h>
#include <itkIdentityTransform.h>
#include <itkImageRegistrationMethodv4.h>
#include <itkMeanSquaresImageToImageMetricv4.h>
#include <itkMedianImageFilter.h>
#include <itkRegularStepGradientDescentOptimizerv4.h>
#include <itkRelabelComponentImageFilter.h>
#include <itkResampleImageFilter.h>
#include <itkTranslationTransform.h>

namespace {
class CancellationObserver : public itk::Command {
  public:
    using Self = CancellationObserver;
    using Pointer = itk::SmartPointer<Self>;
    itkNewMacro(Self);
    Task task;
    void Execute(itk::Object *, const itk::EventObject &) override {
        task.check();
    }
    void Execute(const itk::Object *, const itk::EventObject &) override {
        task.check();
    }
};
template <class T> void update(T *process, const Task &task) {
    auto observer = CancellationObserver::New();
    observer->task = task;
    process->AddObserver(itk::ProgressEvent(), observer);
    process->AddObserver(itk::IterationEvent(), observer);
    task.check();
    process->Update();
    task.check();
}
} // namespace

VolumeData VolumePreprocessor::run(const VolumeData &input, const PreprocessingParameter &p,
                                   const Task &task) const {
    if (!input.image)
        throw std::runtime_error("No input volume");
    VolumeData out = input;
    if (p.medianRadius > 8)
        throw std::runtime_error("Median radius must be <= 8");
    if (p.medianRadius) {
        task.report(15, "ITK median denoising");
        using Filter = itk::MedianImageFilter<VolumeImage, VolumeImage>;
        auto filter = Filter::New();
        Filter::InputSizeType radius;
        radius.Fill(p.medianRadius);
        filter->SetRadius(radius);
        filter->SetInput(input.image);
        update(filter.GetPointer(), task);
        out.image = filter->GetOutput();
        out.image->DisconnectPipeline();
    }
    if (p.isotropicSpacing < 0 || !std::isfinite(p.isotropicSpacing))
        throw std::runtime_error("Invalid spacing");
    if (p.isotropicSpacing > 0) {
        task.report(25, "ITK resampling");
        using Filter = itk::ResampleImageFilter<VolumeImage, VolumeImage>;
        auto filter = Filter::New();
        filter->SetInput(out.image);
        auto spacing = out.image->GetSpacing();
        spacing.Fill(p.isotropicSpacing);
        auto size = out.image->GetLargestPossibleRegion().GetSize();
        for (unsigned i = 0; i < 3; ++i)
            size[i] = static_cast<itk::SizeValueType>(
                          std::ceil((size[i] - 1) * out.image->GetSpacing()[i] / spacing[i])) +
                      1;
        filter->SetSize(size);
        filter->SetOutputSpacing(spacing);
        filter->SetOutputOrigin(out.image->GetOrigin());
        filter->SetOutputDirection(out.image->GetDirection());
        filter->SetTransform(itk::IdentityTransform<double, 3>::New());
        update(filter.GetPointer(), task);
        out.image = filter->GetOutput();
        out.image->DisconnectPipeline();
    }
    return out;
}

SegmentationData VolumeSegmenter::run(const VolumeData &input, const SegmentationParameter &p,
                                      const Task &task) const {
    if (!input.image || p.lower > p.upper)
        throw std::runtime_error("Invalid segmentation input/range");
    task.report(35, "ITK threshold segmentation");
    using Threshold = itk::BinaryThresholdImageFilter<VolumeImage, LabelImage>;
    auto threshold = Threshold::New();
    threshold->SetInput(input.image);
    threshold->SetLowerThreshold(p.lower);
    threshold->SetUpperThreshold(p.upper);
    threshold->SetInsideValue(1);
    threshold->SetOutsideValue(0);
    update(threshold.GetPointer(), task);
    SegmentationData result;
    result.image = threshold->GetOutput();
    result.image->DisconnectPipeline();
    if (p.largestComponent) {
        using Components = itk::ConnectedComponentImageFilter<LabelImage, itk::Image<unsigned int, 3>>;
        using Relabel =
            itk::RelabelComponentImageFilter<itk::Image<unsigned int, 3>, itk::Image<unsigned int, 3>>;
        auto components = Components::New();
        components->SetInput(result.image);
        auto relabel = Relabel::New();
        relabel->SetInput(components->GetOutput());
        using Select = itk::BinaryThresholdImageFilter<itk::Image<unsigned int, 3>, LabelImage>;
        auto select = Select::New();
        select->SetInput(relabel->GetOutput());
        select->SetLowerThreshold(1);
        select->SetUpperThreshold(1);
        select->SetInsideValue(1);
        select->SetOutsideValue(0);
        update(select.GetPointer(), task);
        result.image = select->GetOutput();
        result.image->DisconnectPipeline();
    }
    if (p.closingRadius > 8)
        throw std::runtime_error("Closing radius must be <= 8");
    if (p.closingRadius) {
        using Kernel = itk::BinaryBallStructuringElement<unsigned short, 3>;
        Kernel kernel;
        kernel.SetRadius(p.closingRadius);
        kernel.CreateStructuringElement();
        using Closing = itk::BinaryMorphologicalClosingImageFilter<LabelImage, LabelImage, Kernel>;
        auto closing = Closing::New();
        closing->SetInput(result.image);
        closing->SetKernel(kernel);
        closing->SetForegroundValue(1);
        update(closing.GetPointer(), task);
        result.image = closing->GetOutput();
        result.image->DisconnectPipeline();
    }
    result.labels.push_back({1, "Target", {0.92, 0.9, 0.78}});
    return result;
}

VolumeData VolumeRegistrator::run(const VolumeData &fixed, const VolumeData &moving,
                                  const RegistrationParameter &p, const Task &task) const {
    if (!fixed.image || !moving.image || !p.iterations || p.learningRate <= 0)
        throw std::runtime_error("Invalid registration input");
    task.report(20, "ITK translation registration");
    using Transform = itk::TranslationTransform<double, 3>;
    using Registration = itk::ImageRegistrationMethodv4<VolumeImage, VolumeImage, Transform>;
    auto transform = Transform::New();
    transform->SetIdentity();
    auto optimizer = itk::RegularStepGradientDescentOptimizerv4<double>::New();
    optimizer->SetLearningRate(p.learningRate);
    optimizer->SetMinimumStepLength(0.001);
    optimizer->SetNumberOfIterations(p.iterations);
    auto registration = Registration::New();
    registration->SetFixedImage(fixed.image);
    registration->SetMovingImage(moving.image);
    registration->SetMetric(itk::MeanSquaresImageToImageMetricv4<VolumeImage, VolumeImage>::New());
    registration->SetOptimizer(optimizer);
    registration->SetInitialTransform(transform);
    registration->InPlaceOn();
    registration->SetNumberOfLevels(1);
    Registration::ShrinkFactorsArrayType shrink;
    shrink.SetSize(1);
    shrink[0] = 1;
    Registration::SmoothingSigmasArrayType sigma;
    sigma.SetSize(1);
    sigma[0] = 0;
    registration->SetShrinkFactorsPerLevel(shrink);
    registration->SetSmoothingSigmasPerLevel(sigma);
    auto observer = CancellationObserver::New();
    observer->task = task;
    optimizer->AddObserver(itk::IterationEvent(), observer);
    update(registration.GetPointer(), task);
    auto resample = itk::ResampleImageFilter<VolumeImage, VolumeImage>::New();
    resample->SetInput(moving.image);
    resample->SetReferenceImage(fixed.image);
    resample->UseReferenceImageOn();
    resample->SetTransform(transform);
    resample->SetDefaultPixelValue(-1024);
    update(resample.GetPointer(), task);
    VolumeData out{resample->GetOutput(), moving.source};
    out.image->DisconnectPipeline();
    return out;
}
