#pragma once
#include <QJsonObject>
#include <QString>
#include <array>
#include <itkImage.h>
#include <itkTransform.h>
#include <vector>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

using VolumeImage = itk::Image<float, 3>;
using LabelImage = itk::Image<unsigned short, 3>;
struct VolumeGeometrySt {
    std::array<double, 3> origin{}, spacing{};
    std::array<double, 9> direction{};
    std::array<size_t, 3> dimensions{};
};
struct LabelInfoSt {
    unsigned short value = 1;
    QString name;
    std::array<double, 3> color{1, 0, 0};
};
struct VolumeData {
    VolumeImage::Pointer image;
    QString source;
    VolumeGeometrySt geometry() const;
};
struct SegmentationData {
    LabelImage::Pointer image;
    std::vector<LabelInfoSt> labels;
};
struct SurfaceData {
    vtkSmartPointer<vtkPolyData> mesh;
};
struct ProjectData {
    VolumeData volume;
    VolumeData processedVolume;
    SegmentationData segmentation;
    SurfaceData skin, bone;
    QJsonObject processing;
};
