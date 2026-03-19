# 超级无敌金凤凰1.0

一个基于 `C++17 + Qt Widgets + OpenCV 4.x` 的 Windows 桌面工具，用于检测“道具卡”快速经过指定区域的动作，并自动抓取关键帧。

当前版本优先打通本地视频测试链路，同时保留 USB/UVC 摄像头接入能力。核心目标不是“识别卡片类型”，而是稳定完成：

- ROI 运动检测
- 动作触发
- 关键帧抓拍
- 网格展示与局部放大查看
- 本轮结果保存

## 当前状态

这不是演示代码仓库，而是一版可运行的 MVP 工程。

当前已经完成：

- 视频文件加载、播放、暂停、停止、拖动进度、逐帧调试
- USB/UVC 摄像头接入
- 统一的 `FrameSourceBase + RingBuffer + ProcessingWorker` 处理链
- ROI 帧差运动检测
- 冷却时间和上升沿触发
- 单动作关键帧抓拍
- 最多 20 格关键帧滚动网格
- Alt 局部放大查看
- 人数分组边框着色
- 参数持久化与结果保存

## 适用场景

- 本地视频回放调参
- 没有工业相机 SDK 的轻量验证
- 发卡动作节奏分析
- 关键帧采集链路联调
- 后续接入模板匹配、卡片裁切、批量分析前的基础版本

## 技术栈

- C++17
- Qt Widgets
- OpenCV 4.x
- CMake
- Windows 10 / 11

优先兼容：

- Qt 5.15.2

当前本机实际验证过的环境：

- MSYS2 UCRT64
- GCC 15.2
- Qt 6.10.1
- OpenCV 4.13.0
- CMake 4.2
- Ninja

## 功能概览

### 1. 双输入源

- 摄像头模式
- 本地视频测试模式

两者共用同一套帧缓存、检测和抓拍流程。

### 2. ROI 运动检测

当前版本使用传统 OpenCV 方法：

- 灰度化
- 高斯模糊
- 帧差
- 阈值化
- 膨胀
- 轮廓面积判定

算法目标是检测“卡片是否快速经过指定通道”，不是识别手势，也不是识别卡片类别。

### 3. 动作级抓拍

当前默认策略是：

- 一次完整发卡视为一次动作
- 每次动作默认抓 1 张关键帧
- 关键帧时机优先贴近“卡片刚脱手且露出一部分”

### 4. 关键帧工作台

- 最多支持 20 张关键帧
- 4 列滚动网格
- 点击格子查看大图
- Alt + 鼠标局部放大
- 分组边框着色

## 界面说明

### 左侧

- 关键帧主工作区
- 4 列滚动网格
- 选中关键帧查看

### 右侧

- 小型视频 / 摄像头预览
- ROI 叠加
- 状态信息
- 输入源控制
- 参数区

### 顶部

- 紧凑工具栏
- 输入源切换
- 打开视频
- 开始 / 停止
- 常用操作入口

## 默认参数

当前仓库内的默认值已经基于 `test3.MOV` 和 `sample/1.JPG ~ 10.JPG` 做过一轮拟合：

- ROI：`0.1698, 0.6574, 0.6198, 0.3426`
- 运动阈值：`16`
- 最小运动面积：`3040`
- 冷却时间：`550 ms`
- 环形缓冲区大小：`8`
- 结果上限：`12`
- 单动作抓拍数：`1`
- 视频循环播放：默认关闭

这组参数更偏向“卡片经过下方通道时触发”，而不是框住手部本身。

## 为什么 ROI 常放在下方

这个项目当前不是在“识别手”，而是在“看一条过卡通道上有没有明显运动”。

很多场景里，ROI 放在下方反而更稳，原因很简单：

- 卡片脱手后会稳定经过这块区域
- 下方背景更干净
- 手部区域的干扰动作更多
- 更容易形成清晰的上升沿触发

所以调参时，通常应该先找“卡片经过的区域”，再去调阈值和面积。

## 工程结构

```text
.
├─ CMakeLists.txt
├─ README.md
├─ LICENSE
├─ build_mvp_ucrt64.cmd
├─ run_mvp_ucrt64.cmd
├─ sample/
├─ src/
│  ├─ AppTypes.h
│  ├─ MainWindow.h / MainWindow.cpp
│  ├─ FrameSourceBase.h / FrameSourceBase.cpp
│  ├─ CameraFrameSource.h / CameraFrameSource.cpp
│  ├─ VideoFileFrameSource.h / VideoFileFrameSource.cpp
│  ├─ RingBuffer.h / RingBuffer.cpp
│  ├─ MotionDetector.h / MotionDetector.cpp
│  ├─ BurstCaptureManager.h / BurstCaptureManager.cpp
│  ├─ ProcessingWorker.h / ProcessingWorker.cpp
│  ├─ PreviewWidget.h / PreviewWidget.cpp
│  ├─ BurstGridWidget.h / BurstGridWidget.cpp
│  ├─ FrameCellWidget.h / FrameCellWidget.cpp
│  ├─ SettingsManager.h / SettingsManager.cpp
│  ├─ ImageUtils.h / ImageUtils.cpp
│  └─ main.cpp
└─ tools/
   ├─ FitDefaultsTool.cpp
   └─ SimulateActionCaptureTool.cpp
```

## 构建

### 方式 1：当前机器直接构建

如果你就在这台已经配置好的机器上：

```powershell
cd D:\poke
.\build_mvp_ucrt64.cmd
.\run_mvp_ucrt64.cmd
```

当前输出目录：

```text
D:\poke\build-msys2-2
```

### 方式 2：手动 CMake 构建

```powershell
cd D:\poke
mkdir build
cd build
cmake .. -G "Ninja" `
  -DCMAKE_PREFIX_PATH="C:/Qt/5.15.2/msvc2019_64" `
  -DOpenCV_DIR="C:/opencv/build"
cmake --build . --config Release
```

Qt 6 也可用，只要把 `CMAKE_PREFIX_PATH` 指向对应版本。

## 运行

编译完成后运行：

```powershell
.\PokeBurstCaptureMVP.exe
```

首次启动后，程序会在运行目录生成：

- `config.ini`
- `captures/`

## 视频测试模式

当前 MVP 的主链路是视频测试模式，推荐先用它验证。

基本流程：

1. 打开程序
2. 选择视频模式
3. 加载本地视频
4. 开始预览
5. 调整 ROI
6. 播放视频
7. 观察自动触发和关键帧结果
8. 点击缩略图查看大图
9. 保存本轮抓拍结果

已支持：

- 播放 / 暂停 / 停止
- 拖动进度条
- 下一帧
- 循环播放开关
- 当前帧 / 总帧数 / 当前时间 / 总时长显示
- 暂停后手动触发
- 播放结束后保留当前抓拍结果

## 摄像头模式

摄像头模式已经接入，但这版仍优先围绕视频测试优化。

支持：

- 枚举 0..5 号摄像头
- 打开指定摄像头
- 实时预览
- 共享同一套 ROI 检测和抓拍流程

## GitHub 使用说明

当前仓库已经适合直接放到 GitHub。

建议上传内容：

- 源码
- README
- LICENSE
- sample 参考图
- 构建脚本

不建议上传：

- 本地构建目录
- 本地便携工具
- 大体积原始测试视频
- 本地生成的 captures

## 已知限制

- 当前只做传统运动检测，不做卡片分类识别
- 摄像头枚举仍是 MVP 级别的索引探测
- 没有播放速度控制
- 没有历史轮次浏览器
- 大图预览目前不支持单独缩放拖拽
- ROI 当前以拖拽重设为主
- 视频解码能力依赖本机 OpenCV / FFmpeg 构建方式

## 后续扩展方向

- 模板匹配识别卡片类型
- 自动裁切卡片区域
- CSV / Excel 导出
- 多轮历史结果浏览
- 参数 profile
- 批量回放和参数对比

## 许可证

本项目当前使用 [MIT License](LICENSE)。
