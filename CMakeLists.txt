cmake_minimum_required(VERSION 3.21)

project(vtk3D VERSION 0.1.0 LANGUAGES CXX)

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
    ImagingGeneral
    ImagingSources
    IOImage
    IOGeometry
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

add_executable(vtk3D WIN32
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/MedicalViewer.cpp
    src/MedicalViewer.h
    src/ThemeManager.cpp
    src/ThemeManager.h
    resources/resources.qrc
)

target_link_libraries(vtk3D PRIVATE
    Qt${VTK_QT_VERSION}::Core
    Qt${VTK_QT_VERSION}::Gui
    Qt${VTK_QT_VERSION}::Widgets
    ${VTK_LIBRARIES}
)

target_compile_definitions(vtk3D PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    NOMINMAX
)

vtk_module_autoinit(
    TARGETS vtk3D
    MODULES ${VTK_LIBRARIES}
)

if(MSVC)
    target_compile_options(vtk3D PRIVATE /W4 /permissive- /utf-8)
endif()

include(GNUInstallDirs)
install(TARGETS vtk3D RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
