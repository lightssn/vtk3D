#include "Readers.h"
#include <QFileInfo>
#include <cmath>
#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkImageFileReader.h>
#include <itkImageSeriesReader.h>

namespace {
VolumeData validated(VolumeImage::Pointer image, const QString &source, const Task &task) {
    task.check();
    if (!image || !image->GetLargestPossibleRegion().GetNumberOfPixels())
        throw std::runtime_error("Empty volume");
    for (unsigned i = 0; i < 3; ++i)
        if (!std::isfinite(image->GetSpacing()[i]) || image->GetSpacing()[i] <= 0)
            throw std::runtime_error("Invalid image spacing");
    image->DisconnectPipeline();
    return {image, source};
}
} // namespace
VolumeData VolumeReader::read(const QString &path, const Task &task) const {
    const QFileInfo file(path);
    if (!file.isFile())
        throw std::runtime_error("Volume file not found");
    if (file.suffix().compare("raw", Qt::CaseInsensitive) == 0)
        throw std::runtime_error("RAW requires geometry and pixel type: select its MHD header instead");
    task.report(5, "ITK reading volume");
    auto reader = itk::ImageFileReader<VolumeImage>::New();
    reader->SetFileName(file.absoluteFilePath().toUtf8().constData());
    reader->Update();
    return validated(reader->GetOutput(), file.absoluteFilePath(), task);
}
QStringList DicomReader::series(const QString &directory) const {
    auto names = itk::GDCMSeriesFileNames::New();
    names->SetUseSeriesDetails(true);
    names->SetDirectory(directory.toUtf8().constData());
    QStringList result;
    for (const auto &uid : names->GetSeriesUIDs())
        result << QString::fromStdString(uid);
    return result;
}
VolumeData DicomReader::read(const QString &directory, const QString &uid, const Task &task) const {
    task.report(5, "ITK scanning DICOM series");
    auto names = itk::GDCMSeriesFileNames::New();
    names->SetUseSeriesDetails(true);
    names->SetDirectory(directory.toUtf8().constData());
    const auto ids = names->GetSeriesUIDs();
    if (ids.empty())
        throw std::runtime_error("No DICOM series found");
    if (uid.isEmpty() && ids.size() != 1)
        throw std::runtime_error("Multiple DICOM series: select a series UID");
    auto reader = itk::ImageSeriesReader<VolumeImage>::New();
    reader->SetImageIO(itk::GDCMImageIO::New());
    reader->SetFileNames(names->GetFileNames(uid.isEmpty() ? ids.front() : uid.toStdString()));
    reader->Update();
    return validated(reader->GetOutput(), directory, task);
}
