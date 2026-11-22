#include "Services.h"
#include "algorithms/ImagingAlgorithms.h"
#include "data/Readers.h"
#include "data/Writers.h"
#include <QFileInfo>
#include <QElapsedTimer>
#include <itkImageRegionConstIterator.h>
#include <vtkMassProperties.h>
#include <vtkNew.h>

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
vtkSmartPointer<vtkImageData> ProcessingService::displayImage(const VolumeData &volume, const Task &task) const {
    return ImageBridge::forDisplay(volume, task);
}
ProjectData ProcessingService::process(const ProjectData &input, float skin, float bone,
                                       const PreprocessingParameter &preprocessing,
                                       const SegmentationParameter &segmentation,
                                       const SurfaceProcessingParameter &surface, const Task &task) const {
    ProjectData out = input;
    out.statistics = {};
    QElapsedTimer total, stage;
    total.start();
    stage.start();
    out.processedVolume = VolumePreprocessor().run(input.volume, preprocessing, task);
    out.statistics["preprocessMs"] = double(stage.restart());
    auto p = segmentation;
    p.lower = skin;
    auto skinLabels = VolumeSegmenter().run(out.processedVolume, p, task);
    p.lower = bone;
    out.segmentation = VolumeSegmenter().run(out.processedVolume, p, task);
    out.statistics["segmentationMs"] = double(stage.restart());
    task.check();
    auto skinImage = ImageBridge::toVtk(skinLabels);
    auto boneImage = ImageBridge::toVtk(out.segmentation);
    auto skinSurface = SurfaceReconstructor().run(skinImage, {}, task);
    auto boneSurface = SurfaceReconstructor().run(boneImage, {}, task);
    out.statistics["reconstructionMs"] = double(stage.restart());
    out.statistics["skinInputTriangles"] = double(skinSurface.mesh->GetNumberOfPolys());
    out.statistics["boneInputTriangles"] = double(boneSurface.mesh->GetNumberOfPolys());
    out.skin = SurfaceProcessor().run(skinSurface, surface, task);
    out.bone = SurfaceProcessor().run(boneSurface, surface, task);
    out.statistics["surfaceMs"] = double(stage.restart());
    for (const auto &entry : {std::make_pair(QString("skin"), out.skin.mesh),
                              std::make_pair(QString("bone"), out.bone.mesh)}) {
        out.statistics[entry.first + "Points"] = double(entry.second->GetNumberOfPoints());
        out.statistics[entry.first + "Triangles"] = double(entry.second->GetNumberOfPolys());
        if (entry.second->GetNumberOfPolys()) {
            vtkNew<vtkMassProperties> mass;
            mass->SetInputData(entry.second);
            mass->Update();
            out.statistics[entry.first + "AreaMm2"] = mass->GetSurfaceArea();
        }
        task.check();
    }
    // Label volume is independent of smoothing/decimation of the displayed mesh.
    size_t foreground = 0, visited = 0;
    itk::ImageRegionConstIterator<LabelImage> it(out.segmentation.image,
                                               out.segmentation.image->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if ((++visited & 65535) == 0)
            task.check();
        foreground += it.Get() != 0;
    }
    const auto spacing = out.segmentation.image->GetSpacing();
    const auto d = out.segmentation.image->GetDirection();
    const double determinant = d[0][0] * (d[1][1]*d[2][2] - d[1][2]*d[2][1]) -
        d[0][1] * (d[1][0]*d[2][2] - d[1][2]*d[2][0]) +
        d[0][2] * (d[1][0]*d[2][1] - d[1][1]*d[2][0]);
    out.statistics["foregroundVoxels"] = double(foreground);
    out.statistics["labelVolumeMm3"] = double(foreground) * spacing[0] * spacing[1] * spacing[2] * std::abs(determinant);
    out.statistics["totalMs"] = double(total.elapsed());
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
