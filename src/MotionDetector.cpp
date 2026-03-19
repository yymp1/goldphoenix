#include "MotionDetector.h"

#include <opencv2/imgproc.hpp>

void MotionDetector::setParameters(const MotionParameters& parameters)
{
    m_parameters = parameters;
}

void MotionDetector::reset()
{
    m_previousGray.release();
    m_previousMotion = false;
    m_lastTriggerMs = 0;
    m_observedFrames = 0;
}

MotionResult MotionDetector::detect(const cv::Mat& frame, const QRect& roi, qint64 timestampMs)
{
    MotionResult result;
    if (frame.empty()) {
        return result;
    }

    QRect boundedRoi = roi;
    if (boundedRoi.isEmpty()) {
        boundedRoi = QRect(0, 0, frame.cols, frame.rows);
    }

    boundedRoi = boundedRoi.intersected(QRect(0, 0, frame.cols, frame.rows));
    if (boundedRoi.isEmpty()) {
        return result;
    }

    const cv::Rect cvRoi(boundedRoi.x(), boundedRoi.y(), boundedRoi.width(), boundedRoi.height());
    cv::Mat roiFrame = frame(cvRoi);

    cv::Mat gray;
    cv::cvtColor(roiFrame, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);

    if (m_previousGray.empty() || m_previousGray.size() != gray.size()) {
        m_previousGray = gray.clone();
        m_previousMotion = false;
        m_observedFrames = 1;
        return result;
    }

    cv::Mat diff;
    cv::absdiff(gray, m_previousGray, diff);

    cv::Mat thresholded;
    cv::threshold(diff, thresholded, m_parameters.threshold, 255, cv::THRESH_BINARY);
    cv::dilate(thresholded, thresholded, cv::Mat(), cv::Point(-1, -1), 2);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresholded, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    double largestArea = 0.0;
    cv::Rect largestBox;
    for (const auto& contour : contours) {
        const double area = cv::contourArea(contour);
        if (area > largestArea) {
            largestArea = area;
            largestBox = cv::boundingRect(contour);
        }
    }

    result.largestArea = largestArea;
    result.motionDetected = largestArea >= static_cast<double>(m_parameters.minArea);
    if (largestArea > 0.0) {
        result.motionBox = QRect(
            boundedRoi.x() + largestBox.x,
            boundedRoi.y() + largestBox.y,
            largestBox.width,
            largestBox.height);
    }

    ++m_observedFrames;

    if (result.motionDetected
        && !m_previousMotion
        && m_observedFrames > m_warmupFrames
        && (timestampMs - m_lastTriggerMs) >= m_parameters.cooldownMs) {
        result.shouldTrigger = true;
        m_lastTriggerMs = timestampMs;
    }

    m_previousMotion = result.motionDetected;
    m_previousGray = gray.clone();
    return result;
}
