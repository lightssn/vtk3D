# vtk3D
基于qt+vtk的ct切片读取重建程序
	输入mhd数据
	轴状、冠状、矢状三切片视图
	皮肤/骨骼三维模型，可导出为stl
	后台执行读取数据、表面重建、平滑
	可设置平滑迭代数（0–100，默认 15）

#依赖
	vcpkg install itk[vtk] --overlay-ports=C:\code\vtk3D\vcpkg-overlays\ports
	vcpkg install vtk[qt,opengl] (vcpkg的itk+vtk默认依赖vcpkg的qt6，qt5或源码qt需手动在vcpkg创vtk的自定义port，然后加--overlay-ports=.\port_custom)
	
#目录
	data/data.txt 数据说明



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
		ElementType = MET_SHORT 每体素为16位int 2字节
		ElementDataFile = FullHead.raw 像素数据文件名
	.raw存储：xyz三方向总计的体素值
	用vtkMetaImageReader读取
	vtk在找不到同名.raw时会找.raw.gz
	
实际临床ct数据：
		每切片一个DICOM，含像素数据 检查 序列 患者坐标 层间距 重建参数元数据
		用vtkDICOMImageReader读取

#交互层
	主窗口 MainWindow
	四视图 FourViewer
	切片视图 SliceView
	三维视图 SurfaceView
	分割/重建/显示参数 ParameterPanel
	主题 ThemeManager
		
	主窗口类 QMainWindow MainWindow
		构建界面 buildUi()
	四视图类 QWidget MedicalViewer
		buildUi()
			初始化渲染窗口 new QVTKOpenGLNativeWidget(...)
		切片面板结构体 SlicePane
			保存标题 方向 控件 vtk窗口
		表面实例结构体 SurfacePipeline
			保存 vtkPolyData，mapper，actor
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
				] 通过值捕获传递智能指针
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
		
#服务层
	项目管理 ProjectService
	导入 ImportService
	处理 ProcessingService
	导出 ExportService

#数据层	
	项目 ProjectData
	体数据 VolumeData
	分割标签 SegmentationData
	表面网格 SurfaceData
	
	读取单影像 VolumeReader .mhd .mha .nrrd .nifti
	读取DICOM序列 DicomReader .raw
	
	标签转影像 SegmentationWriter
		.mha .nii.gz .nrrd
	STL导出器 SurfaceWriter .stl .vtp .obj .ply
	项目备份 ProjectRepository
	几何体 VolumeGeometrySt
	标签体 LabelInfoSt
	
#算法层
	ITK
	去噪 重采样 VolumePreprocessor PreprocessingParameter
	分割 连通域筛选 形态学处理 VolumeSegmenter SegmentationParameter
	配准 VolumeRegistrator RegistrationParameter
	
	ITK转VTK ImageBridge
	
	VTK
	提取等值面 SurfaceReconstructor ReconstructionParameter
	网格平滑 简化 法线 SurfaceProcessor SurfaceProcessingParameter

#基础层
	任务 Task
	任务调度 TaskManager
	日志 Logger
	配置 Settings
	
#问题
	十字线联动 防递归
	
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
	
|  |  |
|---|---|
| 基础阅片 | 多检查、多序列、多屏、平铺/层叠、自定义挂片布局 |
| 图像操作 | 窗宽窗位、缩放、平移、旋转、翻转、负片 |
| 测量标注 | 长度、角度、心胸比、面积、周长、CT 值、文字与箭头 |
| 序列联动 | 关联滚动、同步二维操作、交叉定位线 |
| 数据操作 | DICOM 标签查看、图像导出、发送至 DICOM 节点 |
|---|---|
| 影像浏览 | 窗宽窗位、斜切 MPR、厚层投影、曲面重建 |
| 三维显示 | 体绘制、传递函数、裁剪、剖切 |
| 分割编辑 | 画笔、擦除、区域生长、多标签、人工修正 |
| 测量分析 | 角度、轮廓、统计、时间序列比较 |
| 配准融合 | 刚体、仿射、形变、多影像叠加 |
| 临床工作流 | 患者与检查管理、PACS 通信、报告 |
| 专科工具 | 血管、骨科、肿瘤、放疗等特定流程 |
| 产品工程 | 撤销重做、自动恢复、审计、权限和质量验证 |
	
#渲染
	vtk可通过webgpu用vulkan后端，需构建时带RenderingWebGPU，vtk9.4起支持vtkWebGPURenderWindow