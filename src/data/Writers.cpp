#include "Writers.h"
#include "Readers.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <vtkNew.h>
#include <vtkOBJWriter.h>
#include <vtkPLYWriter.h>
#include <vtkSTLWriter.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkXMLPolyDataWriter.h>

void SegmentationWriter::write(const SegmentationData &data, const QString &path) const {
    if (!data.image)
        throw std::runtime_error("No segmentation");
    const auto name = path.toLower();
    if (!name.endsWith(".mha") && !name.endsWith(".nii.gz") && !name.endsWith(".nrrd"))
        throw std::runtime_error("Unsupported label output format");
    auto writer = itk::ImageFileWriter<LabelImage>::New();
    writer->SetInput(data.image);
    writer->SetFileName(path.toUtf8().constData());
    writer->UseCompressionOn();
    writer->Update();
}
void SurfaceWriter::write(const SurfaceData &data, const QString &path) const {
    if (!data.mesh || !data.mesh->GetNumberOfCells())
        throw std::runtime_error("No surface to export");
    const auto ext = QFileInfo(path).suffix().toLower();
    auto write = [&](auto writer) {
        writer->SetFileName(path.toUtf8().constData());
        writer->SetInputData(data.mesh);
        writer->Write();
        if (writer->GetErrorCode() || !QFileInfo(path).isFile() || QFileInfo(path).size() == 0)
            throw std::runtime_error("Surface write failed");
    };
    if (ext == "stl") {
        auto w = vtkSmartPointer<vtkSTLWriter>::New();
        w->SetFileTypeToBinary();
        write(w);
    } else if (ext == "vtp")
        write(vtkSmartPointer<vtkXMLPolyDataWriter>::New());
    else if (ext == "obj")
        write(vtkSmartPointer<vtkOBJWriter>::New());
    else if (ext == "ply")
        write(vtkSmartPointer<vtkPLYWriter>::New());
    else
        throw std::runtime_error("Unsupported surface output format");
}
void ProjectRepository::save(const ProjectData &project, const QString &path) const {
    if (!project.volume.image)
        throw std::runtime_error("No project volume");
    const QFileInfo info(path);
    const QString folder = info.completeBaseName() + "-" + QUuid::createUuid().toString(QUuid::Id128);
    QDir parent = info.absoluteDir();
    if (!parent.mkpath(folder))
        throw std::runtime_error("Cannot create project assets");
    QDir assets(parent.filePath(folder));
    QJsonObject json;
    json["version"] = 1;
    json["assets"] = folder;
    auto writer = itk::ImageFileWriter<VolumeImage>::New();
    writer->SetInput(project.volume.image);
    writer->SetFileName(assets.filePath("volume.mha").toUtf8().constData());
    writer->UseCompressionOn();
    writer->Update();
    json["source"] = project.volume.source;
    json["patientCoordinates"] = project.volume.patientCoordinates;
    json["processing"] = project.processing;
    json["statistics"] = project.statistics;
    QJsonArray labelInfo;
    for (const auto &label : project.segmentation.labels)
        labelInfo.append(QJsonObject{{"value", label.value},
                                     {"name", label.name},
                                     {"color", QJsonArray{label.color[0], label.color[1], label.color[2]}}});
    json["labelInfo"] = labelInfo;
    if (project.segmentation.image) {
        SegmentationWriter().write(project.segmentation, assets.filePath("labels.mha"));
        json["labels"] = true;
    }
    for (auto pair : {std::make_pair("skin", project.skin), std::make_pair("bone", project.bone)})
        if (pair.second.mesh && pair.second.mesh->GetNumberOfCells()) {
            SurfaceWriter().write(pair.second, assets.filePath(QString(pair.first) + ".vtp"));
            json[pair.first] = true;
        }
    QSaveFile file(path);
    const auto bytes = QJsonDocument(json).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        throw std::runtime_error("Project manifest write failed");
}
ProjectData ProjectRepository::load(const QString &path) const {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error("Cannot open project");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    const auto json = document.object();
    if (error.error != QJsonParseError::NoError || json["version"].toInt() != 1)
        throw std::runtime_error("Invalid project manifest");
    const QDir parent = QFileInfo(path).absoluteDir();
    const QString assetPath = json["assets"].toString();
    if (assetPath.isEmpty() || assetPath.contains('/') || assetPath.contains('\\') || assetPath == "..")
        throw std::runtime_error("Invalid assets path");
    QDir assets(parent.filePath(assetPath));
    ProjectData out;
    out.volume = VolumeReader().read(assets.filePath("volume.mha"));
    out.volume.source = json["source"].toString();
    out.volume.patientCoordinates = json["patientCoordinates"].toBool();
    out.processing = json["processing"].toObject();
    out.statistics = json["statistics"].toObject();
    if (json["labels"].toBool()) {
        auto reader = itk::ImageFileReader<LabelImage>::New();
        reader->SetFileName(assets.filePath("labels.mha").toUtf8().constData());
        reader->Update();
        out.segmentation.image = reader->GetOutput();
        out.segmentation.image->DisconnectPipeline();
    }
    for (const auto &value : json["labelInfo"].toArray()) {
        const auto object = value.toObject();
        const auto color = object["color"].toArray();
        if (color.size() != 3)
            throw std::runtime_error("Invalid label color");
        out.segmentation.labels.push_back({static_cast<unsigned short>(object["value"].toInt()),
                                           object["name"].toString(),
                                           {color[0].toDouble(), color[1].toDouble(), color[2].toDouble()}});
    }
    for (auto pair : {std::make_pair("skin", &out.skin), std::make_pair("bone", &out.bone)})
        if (json[pair.first].toBool()) {
            vtkNew<vtkXMLPolyDataReader> reader;
            reader->SetFileName(assets.filePath(QString(pair.first) + ".vtp").toUtf8().constData());
            reader->Update();
            if (reader->GetErrorCode())
                throw std::runtime_error("Project mesh read failed");
            pair.second->mesh = reader->GetOutput();
        }
    return out;
}
