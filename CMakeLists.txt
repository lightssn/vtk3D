cmake_minimum_required(VERSION 3.21)

project(vtk3D VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(VTK REQUIRED COMPONENTS
    CommonColor
    CommonCore
    CommonDataModel
    FiltersCore
    FiltersGeneral
    FiltersSources
    GUISupportQt
    ImagingCore
    ImagingColor
    ImagingGeneral
    ImagingSources
    IOImage
    IOGeometry
    IOPLY
    IOXML
    InteractionImage
    InteractionStyle
    RenderingAnnotation
    RenderingCore
    RenderingFreeType
    RenderingOpenGL2
)

# VTK's Qt widgets must use the same Qt major version as VTK itself. VTK's
# package config exports VTK_QT_VERSION when GUISupportQt is available. Never
# guess here: guessing can silently mix incompatible Qt ABIs.
if(NOT DEFINED VTK_QT_VERSION OR NOT VTK_QT_VERSION MATCHES "^[56]$")
    message(FATAL_ERROR
        "The selected VTK does not report a supported VTK_QT_VERSION (5 or 6). "
        "Install/build VTK with GUISupportQt enabled.")
endif()
message(STATUS "vtk3D: using Qt ${VTK_QT_VERSION} selected by VTK")
find_package(Qt${VTK_QT_VERSION} REQUIRED COMPONENTS Core Gui Widgets)
set(APP_VTK_LIBRARIES ${VTK_LIBRARIES})

find_package(ITK CONFIG REQUIRED COMPONENTS
    ITKCommon ITKIOImageBase ITKIOMeta ITKIONRRD ITKIONIFTI ITKIOGDCM
    ITKSmoothing ITKImageGrid ITKThresholding ITKConnectedComponents
    ITKBinaryMathematicalMorphology ITKRegistrationMethodsv4 ITKMetricsv4
    ITKOptimizersv4)
include(${ITK_USE_FILE})

add_library(Foundation STATIC src/foundation/TaskManager.cpp src/foundation/TaskManager.h)
target_include_directories(Foundation PUBLIC src)
target_link_libraries(Foundation PUBLIC Qt${VTK_QT_VERSION}::Core)

add_library(ImagingData STATIC src/data/ProjectData.cpp src/data/Readers.cpp src/data/Writers.cpp
    src/foundation/ItkTask.h)
target_link_libraries(ImagingData PUBLIC Foundation ${ITK_LIBRARIES}
    VTK::CommonDataModel VTK::IOGeometry VTK::IOPLY VTK::IOXML)

add_library(ImagingAlgorithms STATIC src/algorithms/VolumeAlgorithms.cpp src/algorithms/SurfaceAlgorithms.cpp
    src/algorithms/MeasurementGeometry.h)
target_link_libraries(ImagingAlgorithms PUBLIC ImagingData VTK::FiltersCore VTK::FiltersGeneral VTK::ImagingCore)

add_library(AppServices STATIC src/services/Services.cpp src/services/Services.h)
target_link_libraries(AppServices PUBLIC ImagingAlgorithms)

add_executable(vtk3D WIN32
    src/main.cpp
    tests/UiChecks.cpp tests/UiChecks.h
    src/ui/MainWindow.cpp src/ui/MainWindow.h
    src/ui/FourViewer.cpp src/ui/FourViewer.h
    src/ui/SliceView.cpp src/ui/SliceView.h
    src/ui/SurfaceView.cpp src/ui/SurfaceView.h
    src/ui/ParameterPanel.cpp src/ui/ParameterPanel.h
    src/ui/AnalysisBar.cpp src/ui/AnalysisBar.h
    src/ui/ThemeManager.cpp src/ui/ThemeManager.h
    resources/resources.qrc
)

target_link_libraries(vtk3D PRIVATE
    AppServices
    Qt${VTK_QT_VERSION}::Core
    Qt${VTK_QT_VERSION}::Gui
    Qt${VTK_QT_VERSION}::Widgets
    ${APP_VTK_LIBRARIES}
)

target_compile_definitions(vtk3D PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    NOMINMAX
)

vtk_module_autoinit(
    TARGETS vtk3D
    MODULES ${APP_VTK_LIBRARIES}
)

if(MSVC)
    foreach(target Foundation ImagingData ImagingAlgorithms AppServices vtk3D)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /bigobj)
        target_compile_definitions(${target} PUBLIC NOMINMAX)
    endforeach()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(PipelineTests tests/PipelineTests.cpp)
    target_link_libraries(PipelineTests PRIVATE AppServices)
    if(MSVC)
        target_compile_options(PipelineTests PRIVATE /utf-8 /bigobj)
    endif()
    vtk_module_autoinit(TARGETS PipelineTests MODULES VTK::CommonCore VTK::CommonDataModel VTK::FiltersCore)
    add_test(NAME PipelineTests COMMAND PipelineTests "${CMAKE_CURRENT_SOURCE_DIR}/data/FullHead.mhd")
endif()

include(GNUInstallDirs)
install(TARGETS vtk3D RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
