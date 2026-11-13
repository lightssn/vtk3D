#include "algorithms/ImagingAlgorithms.h"
#include "data/Readers.h"
#include "data/Writers.h"
#include "foundation/TaskManager.h"
#include "services/Services.h"
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <cmath>
#include <iostream>
#include <itkGDCMImageIO.h>
#include <itkImageFileWriter.h>
#include <itkImageRegionIterator.h>
#include <itkImageSeriesWriter.h>
#include <itkMetaDataObject.h>
#include <vtkMatrix3x3.h>

void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void mustFail(F f) {
    bool failed = false;
    try {
        f();
    } catch (...) {
        failed = true;
    }
    require(failed, "Expected error was not reported");
}
VolumeData phantom(int shift = 0) {
    auto image = VolumeImage::New();
    VolumeImage::SizeType size;
    size.Fill(32);
    VolumeImage::RegionType region;
    region.SetSize(size);
    image->SetRegions(region);
    image->Allocate();
    itk::ImageRegionIterator<VolumeImage> it(image, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        const auto i = it.GetIndex();
        const double x = i[0] - 15.0 - shift, y = i[1] - 16.0, z = i[2] - 14.0;
        it.Set(static_cast<float>(2000 * std::exp(-(x * x / 30 + y * y / 45 + z * z / 22))));
    }
    return {image, "phantom"};
}
double mse(const VolumeData &a, const VolumeData &b) {
    double sum = 0;
    size_t count = 0;
    itk::ImageRegionConstIterator<VolumeImage> it(a.image, a.image->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        const auto index = it.GetIndex();
        bool interior = true;
        for (unsigned axis = 0; axis < 3; ++axis)
            if (index[axis] < 4 || index[axis] >= 28)
                interior = false;
        if (!interior)
            continue;
        const double d = it.Get() - b.image->GetPixel(index);
        sum += d * d;
        ++count;
    }
    return sum / count; // Compare overlap, excluding resampling padding.
}
void queueTests() {
    TaskManager manager;
    QEventLoop loop;
    int work = 0, complete = 0, failures = 0;
    QThread *worker = nullptr;
    QObject::connect(&manager, &TaskManager::failed, &loop, [&](const QString &) { ++failures; });
    QObject::connect(&manager, &TaskManager::idle, &loop, &QEventLoop::quit);
    bool timeout = false;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
        timeout = true;
        loop.quit();
    });
    timer.start(10000);
    manager.submit(
        "one",
        [&](const Task &) {
            require(QThread::currentThread() != QCoreApplication::instance()->thread(),
                    "Task ran on UI thread");
            worker = QThread::currentThread();
            ++work;
        },
        [&] {
            require(QThread::currentThread() == QCoreApplication::instance()->thread(),
                    "Completion ran off UI thread");
            ++complete;
        });
    manager.submit("failure", [](const Task &) { throw std::runtime_error("expected"); });
    manager.submit(
        "two",
        [&](const Task &) {
            require(QThread::currentThread() == worker, "Worker thread was not reused");
            ++work;
        },
        [&] { ++complete; });
    loop.exec();
    require(!timeout && work == 2 && complete == 2 && failures == 1, "Task queue failed");
    bool completed = false;
    std::atomic_bool entered{false};
    manager.submit(
        "cancel",
        [&](const Task &task) {
            entered.store(true);
            while (true) {
                task.check();
                QThread::msleep(1);
            }
        },
        [&] { completed = true; });
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (entered.load())
            manager.cancel();
    });
    poll.start(1);
    timer.start(10000);
    loop.exec();
    require(!timeout && !completed, "Cancelled task committed result");
}
void dicomTests(const QString &directory) {
    using Image3 = itk::Image<short, 3>;
    using Image2 = itk::Image<short, 2>;
    auto image = Image3::New();
    Image3::SizeType size{{8, 8, 4}};
    Image3::RegionType region;
    region.SetSize(size);
    image->SetRegions(region);
    image->Allocate();
    image->FillBuffer(42);
    using Writer = itk::ImageSeriesWriter<Image3, Image2>;
    auto writer = Writer::New();
    writer->SetInput(image);
    auto io = itk::GDCMImageIO::New();
    io->KeepOriginalUIDOn();
    writer->SetImageIO(io);
    std::vector<itk::MetaDataDictionary> dictionaries(4);
    Writer::DictionaryArrayType pointers;
    std::vector<std::string> files;
    for (int z = 0; z < 4; ++z) {
        auto &dictionary = dictionaries[z];
        auto tag = [&](const char *key, const std::string &value) {
            itk::EncapsulateMetaData<std::string>(dictionary, key, value);
        };
        tag("0008|0060", "CT");
        tag("0008|0016", "1.2.840.10008.5.1.4.1.1.2");
        tag("0020|000d", "1.2.826.0.1.3680043.10.543.1");
        tag("0020|000e", "1.2.826.0.1.3680043.10.543.2");
        tag("0020|0052", "1.2.826.0.1.3680043.10.543.3");
        tag("0020|0037", "1\\0\\0\\0\\1\\0");
        tag("0020|0032", "0\\0\\" + std::to_string(z));
        tag("0020|0013", std::to_string(z + 1));
        pointers.push_back(&dictionary);
        files.push_back(QDir(directory).filePath(QString("slice-%1.dcm").arg(3 - z)).toStdString());
    }
    writer->SetFileNames(files);
    writer->SetMetaDataDictionaryArray(&pointers);
    writer->Update();
    const auto ids = DicomReader().series(directory);
    require(ids.size() == 1, "DICOM series enumeration failed");
    const auto read = DicomReader().read(directory, ids.front());
    require(read.geometry().dimensions[2] == 4, "DICOM series depth incorrect");
    require(read.image->GetBufferPointer()[0] == 42, "DICOM scalar decoding incorrect");
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir dir;
        require(dir.isValid(), "Temporary directory unavailable");
        queueTests();
        QDir().mkpath(dir.filePath("dicom"));
        dicomTests(dir.filePath("dicom"));
        auto input = phantom();
        auto noisy = phantom();
        VolumeImage::IndexType index{{15, 16, 14}};
        noisy.image->SetPixel(index, 100000);
        auto filtered = VolumePreprocessor().run(noisy, {1, 0});
        require(filtered.image->GetPixel(index) < 100000, "Median did not remove impulse");
        auto sampled = VolumePreprocessor().run(input, {0, 2});
        require(sampled.image->GetSpacing()[0] == 2, "Resampling spacing wrong");
        auto fixed = phantom(), moving = phantom(2);
        auto registered = VolumeRegistrator().run(fixed, moving, {80, 1});
        std::cout << "registration MSE: " << mse(fixed, moving) << " -> " << mse(fixed, registered) << '\n';
        require(mse(fixed, registered) < mse(fixed, moving) * 0.1,
                "Registration did not correct known translation");
        SegmentationParameter seg;
        seg.lower = 500;
        seg.largestComponent = true;
        seg.closingRadius = 1;
        auto labels = VolumeSegmenter().run(input, seg);
        require(labels.image->GetPixel(index) == 1, "Target voxel not segmented");
        auto vtkImage = ImageBridge::toVtk(labels);
        auto surface = SurfaceReconstructor().run(vtkImage, {});
        require(surface.mesh->GetNumberOfCells() > 0, "Empty surface");
        auto processed = SurfaceProcessor().run(surface, {5, 0.5, true});
        require(processed.mesh->GetNumberOfCells() < surface.mesh->GetNumberOfCells(),
                "Decimation did not reduce cells");
        for (const QString ext : {"stl", "vtp", "obj", "ply"})
            SurfaceWriter().write(processed, dir.filePath("surface." + ext));
        for (const QString ext : {"mha", "nii.gz", "nrrd"}) {
            const auto path = dir.filePath("labels." + ext);
            SegmentationWriter().write(labels, path);
            auto read = VolumeReader().read(path);
            require(read.image->GetPixel(index) == 1, "Label round trip failed");
        }
        VolumeImage::PointType origin;
        origin[0] = 12;
        origin[1] = -3;
        origin[2] = 7;
        input.image->SetOrigin(origin);
        auto direction = input.image->GetDirection();
        direction[0][0] = -1;
        direction[1][1] = -1;
        input.image->SetDirection(direction);
        auto bridge = ImageBridge::toVtk(input);
        require(bridge->GetOrigin()[0] == 12 && bridge->GetDirectionMatrix()->GetElement(0, 0) == -1,
                "Bridge lost geometry");
        bridge->SetScalarComponentFromDouble(0, 0, 0, 0, 999);
        require(input.image->GetBufferPointer()[0] != 999, "Bridge aliases ITK memory");
        ProjectData project;
        project.volume = input;
        project.segmentation = labels;
        project.bone = processed;
        project.processing = {{"medianRadius", 1}, {"boneThreshold", 500}};
        const auto manifest = dir.filePath("project.json");
        ProjectRepository().save(project, manifest);
        auto restored = ProjectRepository().load(manifest);
        require(restored.volume.geometry().origin[0] == 12 &&
                    restored.bone.mesh->GetNumberOfCells() == processed.mesh->GetNumberOfCells(),
                "Project restore failed");
        require(restored.segmentation.labels.size() == 1 &&
                    restored.segmentation.labels.front().name == "Target" &&
                    restored.processing["boneThreshold"].toInt() == 500,
                "Project metadata lost");
        mustFail([] { VolumeReader().read("missing.mhd"); });
        mustFail([&] { SurfaceWriter().write({}, dir.filePath("empty.stl")); });
        Task cancelled;
        cancelled.cancellation->store(true);
        mustFail([&] { VolumePreprocessor().run(input, {1, 0}, cancelled); });
        if (argc > 1) {
            auto real = ImportService().import(QString::fromLocal8Bit(argv[1]));
            auto geometry = real.volume.geometry();
            require(geometry.dimensions[0] == 256 && geometry.dimensions[2] == 94,
                    "FullHead dimensions unexpected");
            auto registeredReal = ProcessingService().registerVolume(real, real.volume, {2, 1});
            auto result = ProcessingService().process(registeredReal, 500, 1150, {1, 0}, {}, {5, 0.3, true});
            require(result.volume.image == registeredReal.volume.image,
                    "Processing replaced original volume");
            auto display = ImageBridge::forDisplay(result.volume);
            require(display->GetDirectionMatrix()->GetElement(0, 0) == 1 &&
                        display->GetScalarRange()[1] > 1150,
                    "Canonical display image invalid");
            require(result.skin.mesh->GetNumberOfCells() > 0 && result.bone.mesh->GetNumberOfCells() > 0,
                    "FullHead processing failed");
            ExportService().surface(result.bone, dir.filePath("FullHead.stl"));
            std::cout << "FullHead cells: skin=" << result.skin.mesh->GetNumberOfCells()
                      << " bone=" << result.bone.mesh->GetNumberOfCells() << '\n';
        }
        std::cout << "All pipeline tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
