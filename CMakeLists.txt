cmake_minimum_required(VERSION 3.16)

project(PokeBurstCaptureMVP VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)
find_package(OpenCV 4 REQUIRED)

set(PROJECT_SOURCES
    src/AppTypes.h
    src/RingBuffer.h
    src/RingBuffer.cpp
    src/ImageUtils.h
    src/ImageUtils.cpp
    src/SettingsManager.h
    src/SettingsManager.cpp
    src/FrameSourceBase.h
    src/FrameSourceBase.cpp
    src/CameraFrameSource.h
    src/CameraFrameSource.cpp
    src/VideoFileFrameSource.h
    src/VideoFileFrameSource.cpp
    src/MotionDetector.h
    src/MotionDetector.cpp
    src/BurstCaptureManager.h
    src/BurstCaptureManager.cpp
    src/ProcessingWorker.h
    src/ProcessingWorker.cpp
    src/PreviewWidget.h
    src/PreviewWidget.cpp
    src/FrameCellWidget.h
    src/FrameCellWidget.cpp
    src/BurstGridWidget.h
    src/BurstGridWidget.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/main.cpp
)

if (QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(PokeBurstCaptureMVP
        WIN32
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(PokeBurstCaptureMVP
        WIN32
        ${PROJECT_SOURCES}
    )
endif()

target_include_directories(PokeBurstCaptureMVP PRIVATE src ${OpenCV_INCLUDE_DIRS})

target_link_libraries(PokeBurstCaptureMVP
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        ${OpenCV_LIBS}
)

if (MSVC)
    target_compile_options(PokeBurstCaptureMVP PRIVATE /utf-8 /W4)
else()
    target_compile_options(PokeBurstCaptureMVP PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_compile_definitions(PokeBurstCaptureMVP PRIVATE
    QT_DEPRECATED_WARNINGS
    _CRT_SECURE_NO_WARNINGS
)

if (QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(PokeBurstCaptureMVP)
endif()

add_executable(FitDefaultsTool
    tools/FitDefaultsTool.cpp
)

target_link_libraries(FitDefaultsTool
    PRIVATE
        ${OpenCV_LIBS}
)

target_include_directories(FitDefaultsTool PRIVATE ${OpenCV_INCLUDE_DIRS})

if (MSVC)
    target_compile_options(FitDefaultsTool PRIVATE /utf-8 /W4)
else()
    target_compile_options(FitDefaultsTool PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(SimulateActionCaptureTool
    src/AppTypes.h
    src/ImageUtils.h
    src/ImageUtils.cpp
    src/MotionDetector.h
    src/MotionDetector.cpp
    src/BurstCaptureManager.h
    src/BurstCaptureManager.cpp
    tools/SimulateActionCaptureTool.cpp
)

target_include_directories(SimulateActionCaptureTool PRIVATE src ${OpenCV_INCLUDE_DIRS})

target_link_libraries(SimulateActionCaptureTool
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        ${OpenCV_LIBS}
)

if (MSVC)
    target_compile_options(SimulateActionCaptureTool PRIVATE /utf-8 /W4)
else()
    target_compile_options(SimulateActionCaptureTool PRIVATE -Wall -Wextra -Wpedantic)
endif()
