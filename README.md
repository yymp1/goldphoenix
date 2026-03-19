# 发道具卡高速抓拍工具 MVP

一个基于 `C++17 + Qt Widgets + OpenCV 4.x` 的 Windows 桌面 MVP，重点优先打通：

- 本地视频加载与播放
- ROI 运动检测
- 自动按动作抓关键帧
- 右侧缩略图展示与大图预览
- 手动触发、保存结果、参数持久化

当前版本同时保留了普通 USB/UVC 摄像头接入能力，但优先围绕 `test3.MOV` 的视频测试链路实现。

## 依赖

- Windows 10 / 11
- CMake 3.16+
- C++17 编译器
  - 推荐 MSVC 2019/2022 x64
- Qt 5.15.2 或 Qt 6.x
  - 组件：`Core`、`Gui`、`Widgets`
- OpenCV 4.x
  - 需要包含 `videoio`、`imgproc`、`imgcodecs`、`highgui` 等常规模块

## 当前已验证环境

本机已实际安装并验证通过以下组合：

- `MSYS2`：`D:\msys64`
- `UCRT64 GCC`：15.2.0
- `Qt`：6.10.1
- `OpenCV`：4.13.0
- `CMake`：4.2.3
- `Ninja`：1.13.2

工程已在该环境下成功编译，并实际启动过 `PokeBurstCaptureMVP.exe`。

## 工程结构

```text
.
├─ CMakeLists.txt
├─ README.md
├─ test3.MOV
├─ 软件演示视频.mp4
├─ sample/
└─ src/
   ├─ AppTypes.h
   ├─ MainWindow.h / MainWindow.cpp
   ├─ FrameSourceBase.h / FrameSourceBase.cpp
   ├─ CameraFrameSource.h / CameraFrameSource.cpp
   ├─ VideoFileFrameSource.h / VideoFileFrameSource.cpp
   ├─ RingBuffer.h / RingBuffer.cpp
   ├─ MotionDetector.h / MotionDetector.cpp
   ├─ BurstCaptureManager.h / BurstCaptureManager.cpp
   ├─ ProcessingWorker.h / ProcessingWorker.cpp
   ├─ PreviewWidget.h / PreviewWidget.cpp
   ├─ SettingsManager.h / SettingsManager.cpp
   ├─ ImageUtils.h / ImageUtils.cpp
   └─ main.cpp
```

## 构建步骤

### 方式 0：直接使用当前机器已装好的 MSYS2 环境

如果你就在这台机器上继续用，最省事的方式是直接运行：

```powershell
cd D:\poke
.\build_mvp_ucrt64.cmd
.\run_mvp_ucrt64.cmd
```

当前构建输出目录：

```text
D:\poke\build-msys2-2
```

### 方式一：使用 Qt 5.15.2 + MSVC

先打开 `x64 Native Tools Command Prompt for VS` 或 Qt 自带的开发环境，再执行：

```powershell
cd D:\poke
mkdir build
cd build
cmake .. -G "Ninja" `
  -DCMAKE_PREFIX_PATH="C:/Qt/5.15.2/msvc2019_64" `
  -DOpenCV_DIR="C:/opencv/build"
cmake --build . --config Release
```

如果你的 OpenCV CMake 配置目录在 `build/x64/vc16/lib` 或其他位置，请把 `OpenCV_DIR` 改成实际路径。

### 方式二：Qt 6

```powershell
cd D:\poke
mkdir build
cd build
cmake .. -G "Ninja" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.6.0/msvc2019_64" `
  -DOpenCV_DIR="C:/opencv/build"
cmake --build . --config Release
```

### 如果未使用 Ninja

也可以改成 Visual Studio 生成器：

```powershell
cmake .. -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/5.15.2/msvc2019_64" `
  -DOpenCV_DIR="C:/opencv/build"
cmake --build . --config Release
```

## 运行方法

编译完成后运行：

```powershell
.\Release\PokeBurstCaptureMVP.exe
```

或 Ninja 默认输出：

```powershell
.\PokeBurstCaptureMVP.exe
```

首次启动后会在程序目录下自动使用或创建：

- `config.ini`
- `captures/`

## 视频测试模式说明

这是当前 MVP 的主路径，建议先用仓库根目录下的 `test3.MOV` 验证。

1. 启动程序后，选择 `视频测试模式`
2. 点击 `上传视频`，选择 `test3.MOV`
3. 点击 `开始预览`
4. 在左侧预览区拖拽设置 ROI
5. 点击 `播放视频`
6. 当道具卡快速经过 ROI 且运动面积达到阈值时，会自动触发一轮抓拍
7. 抓拍结果会依次填充到右侧 12 个缩略图
8. 点击任意缩略图，可以在下方大图区域查看
9. 点击 `保存本轮抓拍`，程序会在 `captures/capture_时间戳/` 下写入图片

视频模式已支持：

- 播放 / 暂停 / 停止
- 拖动进度条跳转
- 下一帧单步调试
- 循环播放开关
- 帧号 / 总帧数 / 当前时间 / 总时长显示
- 暂停后手动触发抓拍
- 播放结束后保留当前抓拍结果

## 摄像头模式说明

MVP 中的摄像头模式同样走统一的 `FrameSourceBase + RingBuffer + ProcessingWorker` 流程。

1. 选择 `摄像头模式`
2. 点击 `刷新摄像头`
3. 从下拉框里选中目标设备
4. 点击 `开始预览`
5. 在预览区设置 ROI
6. 让目标道具卡快速经过 ROI，程序会按与视频模式相同的逻辑触发抓拍

当前枚举方式是按索引探测 `0..5` 号设备，属于 MVP 方案。

## 已实现功能

- Qt Widgets 主界面
- 统一输入源抽象 `FrameSourceBase`
- `CameraFrameSource` 摄像头接入
- `VideoFileFrameSource` 本地视频播放、暂停、停止、seek、下一帧、循环
- 固定容量线程安全环形缓冲区，最新帧优先
- 处理线程与 UI 分离
- ROI 可视化与鼠标拖拽设置
- ROI 参数持久化
- ROI 内传统 OpenCV 运动检测
  - 灰度化
  - 高斯模糊
  - 帧差
  - 阈值化
  - 膨胀
  - 轮廓面积判定
- 冷却时间 + 上升沿触发，避免重复误触
- 自动触发 12 张连续抓拍
- 手动触发复用同一抓拍结果链路
- 右侧缩略图显示与大图预览
- 一键保存本轮抓拍到独立目录
- 配置持久化到 `config.ini`

## 默认参数

- 结果上限：12
- 环形缓冲区大小：8
- 默认 ROI：`0.1698, 0.6574, 0.6198, 0.3426`（按 `x, y, w, h` 归一化）
- 运动阈值：16
- 最小轮廓面积：3040
- 冷却时间：550ms
- 单动作抓拍数：1
- 默认保存目录：应用程序目录下的 `captures/`
- 视频循环播放：默认关闭

以上默认值已基于当前仓库中的 `test3.MOV` 和 `sample/1.JPG ~ 10.JPG` 做过一轮拟合，默认策略改为“每次发卡动作抓 1 张关键帧”，优先贴近“卡片刚脱手且露出一部分”的时机。

## 已知限制

- 当前运动检测是基于 ROI 帧差，不做卡片类型识别
- 摄像头枚举采用索引探测，不显示真实设备名称
- 没有单独做视频播放速度控制，默认原速
- 没有多轮历史结果浏览器
- 大图预览当前是单图查看，不支持缩放与拖拽
- ROI 目前支持拖拽重设，不支持框选后直接移动
- 依赖 OpenCV 对本地视频编解码支持，部分 `.mov` 文件能否打开取决于本机 OpenCV/FFmpeg 构建方式

## 后续扩展建议

- 在 `ProcessingWorker` 后面增加模板匹配或卡片识别模块
- 在 `BurstCaptureManager` 完成后追加自动裁切卡片区域
- 保存抓拍结果时增加 CSV / Excel 导出
- 增加多轮抓拍历史浏览与参数对比
- 在摄像头链路中补充分辨率、曝光和帧率设置项
- 将 ROI / 参数方案做成多个可切换 profile

## 说明

本工程按“先做能跑的 MVP，再保留扩展点”的原则实现，没有引入 QML、工业相机 SDK、深度学习模型或云服务。
