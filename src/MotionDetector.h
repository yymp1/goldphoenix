#pragma once

#include "AppTypes.h"

#include <QRect>

struct MotionResult
{
    bool motionDetected = false;
    bool shouldTrigger = false;
    double largestArea = 0.0;
    QRect motionBox;
};

class MotionDetector
{
public:
    void setParameters(const MotionParameters& parameters);
    void reset();
    MotionResult detect(const cv::Mat& frame, const QRect& roi, qint64 timestampMs);

private:
    MotionParameters m_parameters;
    cv::Mat m_previousGray;
    bool m_previousMotion = false;
    qint64 m_lastTriggerMs = 0;
    int m_observedFrames = 0;
    int m_warmupFrames = 12;
};
