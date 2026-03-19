#pragma once

#include <QImage>
#include <QRect>
#include <QRectF>
#include <QString>

#include <opencv2/core.hpp>

namespace ImageUtils
{
QImage matToQImage(const cv::Mat& mat);
QImage annotateFrameNumber(const QImage& image, qint64 frameNumber);
QString formatDurationMs(qint64 ms);
QRect denormalizeRoi(const QRectF& normalized, const QSize& frameSize);
QRectF normalizeRoi(const QRect& roi, const QSize& frameSize);
}
