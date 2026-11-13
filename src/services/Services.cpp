#include "Services.h"
#include "algorithms/ImagingAlgorithms.h"
#include "data/Readers.h"
#include "data/Writers.h"
#include <QFileInfo>

void ProjectService::save(const ProjectData &snapshot, const QString &path) const {
    ProjectRepository().save(snapshot, path);
}
ProjectData ProjectService::open(const QString &path) const {
    return ProjectRepository().load(path);
}
QStringList ImportService::series(const QString &path) const {
    return DicomReader().series(path);
}
ProjectData ImportService::import(const QString &path, const QString &uid, const Task &task) const {
    ProjectData out;
    out.volume =
        QFileInfo(path).isDir() ? DicomReader().read(path, uid, task) : VolumeReader().read(path, task);
    return out;
}
vtkSmartPointer<vtkImageData> ProcessingService::displayImage(const VolumeData &volume) const {
    return ImageBridge::forDisplay(volume);
}
ProjectData ProcessingService::process(const ProjectData &input, float skin, float bone,
                                       const PreprocessingParameter &preprocessing,
                                       const SegmentationParameter &segmentation,
                                       const SurfaceProcessingParameter &surface, const Task &task) const {
    ProjectData out = input;
    out.processedVolume = VolumePreprocessor().run(input.volume, preprocessing, task);
    auto p = segmentation;
    p.lower = skin;
    auto skinLabels = VolumeSegmenter().run(out.processedVolume, p, task);
    p.lower = bone;
    out.segmentation = VolumeSegmenter().run(out.processedVolume, p, task);
    task.check();
    auto skinImage = ImageBridge::toVtk(skinLabels);
    auto boneImage = ImageBridge::toVtk(out.segmentation);
    out.skin = SurfaceProcessor().run(SurfaceReconstructor().run(skinImage, {}, task), surface, task);
    out.bone = SurfaceProcessor().run(SurfaceReconstructor().run(boneImage, {}, task), surface, task);
    out.processing = {{"skinThreshold", skin},
                      {"boneThreshold", bone},
                      {"medianRadius", static_cast<int>(preprocessing.medianRadius)},
                      {"spacing", preprocessing.isotropicSpacing},
                      {"largestComponent", segmentation.largestComponent},
                      {"closingRadius", static_cast<int>(segmentation.closingRadius)},
                      {"smoothingIterations", surface.smoothingIterations},
                      {"reduction", surface.reduction},
                      {"normals", surface.normals}};
    task.report(95, "Ready to display");
    return out;
}
ProjectData ProcessingService::registerVolume(const ProjectData &fixed, const VolumeData &moving,
                                              const RegistrationParameter &p, const Task &task) const {
    ProjectData out;
    out.volume = VolumeRegistrator().run(fixed.volume, moving, p, task);
    return out;
}
void ExportService::surface(const SurfaceData &data, const QString &path, const Task &task) const {
    task.report(20, "Writing surface");
    SurfaceWriter().write(data, path);
    task.check();
}
void ExportService::segmentation(const SegmentationData &data, const QString &path, const Task &task) const {
    task.report(20, "Writing labels");
    SegmentationWriter().write(data, path);
    task.check();
}
