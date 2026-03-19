#pragma once

#include <QDateTime>
#include <QImage>
#include <QMetaType>
#include <QRectF>
#include <QString>
#include <QVector>

#include <opencv2/core.hpp>

enum class InputSourceType
{
    Camera = 0,
    VideoFile = 1
};

enum class PipelineStatus
{
    Idle,
    MotionDetected,
    Capturing,
    Completed
};

struct FramePacket
{
    cv::Mat frame;
    qint64 frameNumber = -1;
    qint64 timestampMs = 0;
    qint64 sequence = 0;

    bool isValid() const
    {
        return !frame.empty();
    }
};

struct MotionParameters
{
    int threshold = 16;
    int minArea = 3040;
    int cooldownMs = 550;
};

struct BurstParameters
{
    int burstCount = 12;
    int capturesPerAction = 1;
};

struct VideoState
{
    qint64 currentFrame = 0;
    qint64 totalFrames = 0;
    qint64 currentMs = 0;
    qint64 totalMs = 0;
    double fps = 0.0;
    bool playing = false;
    bool loop = false;
};

struct BurstResult
{
    QVector<QImage> images;
    QVector<qint64> frameNumbers;
    QString captureId;
    QDateTime timestamp;
    QString reason;
};

struct AppSettings
{
    InputSourceType inputSourceType = InputSourceType::VideoFile;
    int cameraIndex = 0;
    QString lastVideoPath;
    QRectF roiNormalized = QRectF(0.1698, 0.6574, 0.6198, 0.3426);
    MotionParameters motion;
    BurstParameters burst;
    int ringBufferSize = 8;
    QString outputDir;
    bool loopPlayback = false;
    int peopleCount = 0;
};

Q_DECLARE_METATYPE(VideoState)
Q_DECLARE_METATYPE(BurstResult)
