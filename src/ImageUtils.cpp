#include "ImageUtils.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>

#include <algorithm>

#include <opencv2/imgproc.hpp>

namespace ImageUtils
{
QImage matToQImage(const cv::Mat& mat)
{
    if (mat.empty()) {
        return {};
    }

    switch (mat.type()) {
    case CV_8UC1: {
        QImage image(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_Grayscale8);
        return image.copy();
    }
    case CV_8UC3: {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        QImage image(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
        return image.copy();
    }
    case CV_8UC4: {
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        QImage image(rgba.data, rgba.cols, rgba.rows, static_cast<int>(rgba.step), QImage::Format_RGBA8888);
        return image.copy();
    }
    default:
        return {};
    }
}

QImage annotateFrameNumber(const QImage& image, qint64 frameNumber)
{
    if (image.isNull() || frameNumber < 0) {
        return image;
    }

    QImage annotated = image.convertToFormat(QImage::Format_ARGB32);

    QPainter painter(&annotated);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(std::clamp(annotated.height() / 18, 16, 36));
    painter.setFont(font);

    const QString text = QStringLiteral("F%1").arg(frameNumber);
    const QFontMetrics metrics(font);
    const QRect textRect = metrics.boundingRect(text);
    const int margin = std::clamp(annotated.height() / 40, 10, 18);
    const QRect backgroundRect(
        margin,
        annotated.height() - textRect.height() - (margin * 2),
        textRect.width() + (margin * 2),
        textRect.height() + margin);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 170));
    painter.drawRoundedRect(backgroundRect, 8, 8);

    painter.setPen(QColor(255, 255, 255));
    painter.drawText(
        QRect(
            backgroundRect.left() + margin,
            backgroundRect.top(),
            backgroundRect.width() - (margin * 2),
            backgroundRect.height()),
        Qt::AlignLeft | Qt::AlignVCenter,
        text);

    return annotated;
}

QString formatDurationMs(qint64 ms)
{
    if (ms < 0) {
        ms = 0;
    }

    const int hours = static_cast<int>(ms / 3600000);
    const int minutes = static_cast<int>((ms % 3600000) / 60000);
    const int seconds = static_cast<int>((ms % 60000) / 1000);
    const int millis = static_cast<int>(ms % 1000);

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3.%4")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'))
            .arg(millis, 3, 10, QChar('0'));
    }

    return QStringLiteral("%1:%2.%3")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(millis, 3, 10, QChar('0'));
}

QRect denormalizeRoi(const QRectF& normalized, const QSize& frameSize)
{
    if (!frameSize.isValid()) {
        return {};
    }

    QRectF bounded = normalized.normalized();
    bounded.setX(std::clamp(bounded.x(), 0.0, 1.0));
    bounded.setY(std::clamp(bounded.y(), 0.0, 1.0));
    bounded.setWidth(std::clamp(bounded.width(), 0.0, 1.0 - bounded.x()));
    bounded.setHeight(std::clamp(bounded.height(), 0.0, 1.0 - bounded.y()));

    QRect roi(
        static_cast<int>(bounded.x() * frameSize.width()),
        static_cast<int>(bounded.y() * frameSize.height()),
        static_cast<int>(bounded.width() * frameSize.width()),
        static_cast<int>(bounded.height() * frameSize.height()));

    return roi.intersected(QRect(QPoint(0, 0), frameSize));
}

QRectF normalizeRoi(const QRect& roi, const QSize& frameSize)
{
    if (!frameSize.isValid() || roi.isEmpty()) {
        return QRectF(0.25, 0.25, 0.5, 0.5);
    }

    QRect bounded = roi.intersected(QRect(QPoint(0, 0), frameSize));
    return QRectF(
        static_cast<double>(bounded.x()) / frameSize.width(),
        static_cast<double>(bounded.y()) / frameSize.height(),
        static_cast<double>(bounded.width()) / frameSize.width(),
        static_cast<double>(bounded.height()) / frameSize.height());
}
}
