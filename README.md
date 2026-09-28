# vtk3D
基于qt+vtk的ct切片读取重建程序
	输入mhd数据
	轴状、冠状、矢状三切片视图
	皮肤/骨骼三维模型，可导出为stl
	后台执行读取数据、表面重建、平滑
	可设置平滑迭代数（0–100，默认 15）

#依赖
	vcpkg install vtk[qt,opengl] (vcpkg的vtk默认依赖vcpkg的qt6，qt5或源码qt需在vcpkg创vtk的自定义port后，加--overlay-ports=.\port_custom)

#示例数据
https://gitlab.kitware.com/vtk/vtk-examples/-/raw/master/src/Testing/Data/FullHead.mhd
https://gitlab.kitware.com/vtk/vtk-examples/-/raw/master/src/Testing/Data/FullHead.raw.gz
需解压raw.gz为raw
默认阈值500/1150是此数据偏移标量，非标准HU

#读取
itk影像格式MetaImage：
	头文件.mhd、同路径同名像素文件.raw
	.mhd参数：
		BinaryDataByteOrderMSB = False 小端序
		TransformMatrix = -1 0 0 0 1 0 0 0 -1 图像轴3x3矩阵，xz翻转，y不变，决定体素坐标映射到物理空间方向
		Offset = 0 0 0 图像原点物理坐标偏移
		CenterOfRotation = 0 0 0 旋转中心坐标
		AnatomicalOrientation = LAS 左前上解剖方向
		ElementSpacing = 0.9375 0.9375 1.5 体素xyz物理间距，一般为毫米
		DimSize = 256 256 94 xyz三方向体素数
		ElementType = MET_SHORT 每体素为16位int
		ElementDataFile = FullHead.raw 像素数据文件名
	.raw存储：xyz三方向总计的体素值
	用vtkMetaImageReader读取
	vtk在找不到同名.raw时会找.raw.gz
	
实际临床ct数据：
		每切片一个DICOM，含像素数据 检查 序列 患者坐标 层间距 重建参数元数据
		用vtkDICOMImageReader读取
	
#类
	主窗口类 QMainWindow MainWindow
		构建界面 buildUi()
	四视图类 QWidget MedicalViewer
		buildUi()
			初始化渲染窗口 new QVTKOpenGLNativeWidget(...)
		切片面板结构体 SlicePane
			保存标题 方向 控件 vtk窗口
		表面实例结构体 SurfacePipeline
			保存vtkPolyData，mapper，actor
		导入 loadMetaImage()
			QThread [
				vtkMetaImageReader->SetFileName(...)
				体素网格 vtkImageData
				提取等值面 vtkFlyingEdges3D contour
				平滑 vtkWindowedSincPolyDataFilter smoother
				计算法线 vtkPolyDataNormals normals
				模型网格 vtkPolyData
				连接管线 smoother->SetInputConnection(contour->GetOutputPort());
					依次连接，最后normals->Update()执行时，vtk沿管线向上，更新等值面-平滑-法线-网格
				]
			三个切片视图查看同一体数据不同方向
			三维视图显示两份网格
			actor 控制可见性、颜色、透明度
			mapper 把网格数据交给渲染器绘制
		调整阈值/平滑迭代数 requestReconstruction()
			从已保存m_image重新提取网格，不读文件
			若已有重建线程，会标记，结束后再处理
		applyReconstruction()
			vtkPolyDataMapper接收皮肤/骨骼vtkPolyData m_skin->mapper->SetInputData(skin);
		导出 exportSurface()
			vtkPolyData → vtkSTLWriter
			
		setSkinVisible()
			m_skin->actor->SetVisibility(visible);
		setSkinOpacity()
			m_skin->actor->GetProperty()->SetOpacity(...)
		renderAll()
			Render();
			
		主题 setDarkTheme()
		
	主题类 ThemeManager
		QSettings读/存主题
	滚轮事件类 QObject SliceEventFilter
		调整切片索引
	
#生命周期
	main()
		QApplication
			MainWindow
				ThemeManager
				MedicalViewer
					SlicePane
					SurfacePipeline
	
#管线
	vtkMetaImageReader读raw得vtkImageData
	vtkFlyingEdges3D按皮肤/骨骼阈值提取等值面
	sinc平滑+法线处理得vtkPolyData
	映射到actor显示
	vtkWindowedSincPolyDataFilter
	vtkPolyDataNormals
	mapper
	
	
	标量场
	Flying Edges
	Marching Cubes
	
	vtk智能指针 vtkSmartPointer，非线程安全
	vtkNew
	vtkPolyData
	
#基础题
	vtkImageData与vtkPolyData区别：
		三维影像体素网格/带坐标三维网格
		前者按阈值提取等值面得后者
		
	vtk管线机制：
		SetInputConnection依次连接每个vtkNew
		最后数据请求或Update()触发更新
		
	mapper/actor/renderer/render window分工：
		vtkPolyDataMapper 映射器，接收数据，准备绘制
		vtkActor 场景中对象，带颜色、透明度、材质
		vtkRenderer 场景相机、灯光、背景，渲染图像
		QVTKOpenGLNativeWidget 渲染窗口
		
	提取等值面：将符合阈值的三维体素连接为表面
	OpenGL渲染线程：此项目是主线程，离屏渲染才用子线程渲染
	
	预防和排查内存泄露
		qt父子对象、智能指针、任务回收、限制并发
		用vs内存快照分析
	平滑为何可能收缩或抹掉特征
	法线如何影响光照：法线是面方向向量，朝向光源亮，背向光源暗
	
#进阶题
	Flying Edges 相对经典 Marching Cubes 的并行和性能特点
	阈值选取对表面拓扑和细节的影响
	大体数据的内存和耗时瓶颈
	后台计算与 GUI/OpenGL 线程的边界

#TODO
	reader 错误检查、数据范围与维度检查
	文件损坏检查
	值得检查的是运行中关闭窗口：析构函数会对 QThread::create() 创建的线程调用 quit()、wait()；这里的计算函数没有运行线程事件循环，quit() 不会中断计算，wait() 会等它做完。线程对象依赖 finished → deleteLater() 回收，而 Qt 说明主事件循环停止后提交的延迟删除不会执行
	改皮肤阈值只重建皮肤，改骨骼阈值只重建骨骼，并加入防抖，让m_rebuildPending 合并排队请求里旧请求不执行
	透明度、显隐变化只重绘三维窗口
	体数据读完就先显示切片，表面继续在后台生成
	大网格交互时简化
	
#渲染
	vtk可通过webgpu用vulkan后端，需构建时带RenderingWebGPU，vtk9.4起支持vtkWebGPURenderWindow