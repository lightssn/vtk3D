# vtk3D

基于 Qt 与 VTK 的 CT 多平面显示和表面重建桌面程序。第一版参考 VTK
`MedicalDemo3`，支持三个正交切片、皮肤/骨骼表面重建、STL 导出和日夜主题。

## 功能

- 轴状、冠状、矢状三个切片视图，滚轮浏览切片
- 同屏显示皮肤与骨骼三维模型
- 分别控制组织可见性、透明度和等值阈值
- 数据读取、表面重建和平滑在后台线程执行，状态栏显示阶段进度
- 左侧调试面板可设置 `vtkWindowedSincPolyDataFilter` 迭代数（0–100，默认 15）
- 将皮肤或骨骼表面导出为二进制 STL
- 日间/夜间主题，选择自动保存
- 自动加载 `data/FullHead.mhd`，也可以从菜单选择 `.mhd/.mha`

## 依赖与构建

已安装的 vcpkg 包应包含：

```powershell
vcpkg install vtk[qt,opengl]:x64-windows
```

`python` feature 对本 C++ 工程不是必需，但安装它不会影响使用。本机通过
`ports_custom/vtk` overlay 将 VTK 9.3 构建为 Qt 5 版本；另一台机器可以使用官方
Qt 6 版 VTK。工程读取 VTK 安装配置导出的 `VTK_QT_VERSION`，自动使用与 VTK ABI
一致的 Qt 5 或 Qt 6。如果 VTK 没有导出受支持的版本，配置会直接失败，而不会猜测。
不能把链接 Qt 6 的 VTK GUI 模块与 Qt 5 应用混合链接。当前机器的 vcpkg
位于 `F:/source/vcpkg`，仓库内的 `CMakePresets.json` 已按该路径配置：

```powershell
cmake --preset vcpkg-release
cmake --build --preset release
./build/Release/vtk3D.exe
```

如果 vcpkg 位于其他目录，请修改 preset 中的 `CMAKE_TOOLCHAIN_FILE`，或者运行：

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

## 示例数据
https://gitlab.kitware.com/vtk/vtk-examples/-/raw/master/src/Testing/Data/FullHead.mhd
https://gitlab.kitware.com/vtk/vtk-examples/-/raw/master/src/Testing/Data/FullHead.raw.gz
需解压raw.gz为raw
官方仓库用 gzip 保存像素文件；解压后 `FullHead.mhd` 内的 `ElementDataFile`
指向 `FullHead.raw`。程序也支持通过
“文件 → 打开体数据”从其他目录选择数据。

默认阈值 500/1150 是 `FullHead` 数据使用的偏移标量，并非未经转换的标准 HU。

## 操作

- 三个切片视图：鼠标滚轮切换切片。
- 三维视图：左键旋转、滚轮缩放、中键平移。
- STL：文件菜单中分别导出皮肤或骨骼。
- `R` 重置相机，`Ctrl+T` 切换主题。

#TODO
vcpkg install "vtk[qt,opengl,tbb]:x64-windows" --overlay-ports=F:\source\vcpkg\ports_custom
vtkFlyingEdges3D 比传统 vtkMarchingCubes 更快，并且设计上适合多线程。当前已经使用 Flying Edges。
但最终是否并行，还取决于 VTK 编译时启用的 SMP 后端：
Sequential：单线程
OpenMP：CPU 多线程
TBB：CPU 多线程，通常是更好的选择
STDThread：标准线程后端

运行后可以用以下方式确认 SMP：
#include <vtkSMPTools.h>
qDebug() << "VTK SMP backend:"
         << vtkSMPTools::GetBackend().c_str();
qDebug() << "Estimated threads:"
         << vtkSMPTools::GetEstimatedNumberOfThreads();
