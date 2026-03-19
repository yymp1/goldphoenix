#include "PreviewWidget.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

PreviewWidget::PreviewWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(640, 360);
    setMouseTracking(true);
}

void PreviewWidget::setImage(const QImage& image)
{
    m_image = image;
    update();
}

void PreviewWidget::setRoi(const QRect& roi)
{
    m_roi = roi;
    update();
}

QRect PreviewWidget::roi() const
{
    return m_roi;
}

QSize PreviewWidget::imageSize() const
{
    return m_image.size();
}

QRect PreviewWidget::targetRect() const
{
    if (m_image.isNull()) {
        return rect();
    }

    QSize scaled = m_image.size();
    scaled.scale(size(), Qt::KeepAspectRatio);
    const int x = (width() - scaled.width()) / 2;
    const int y = (height() - scaled.height()) / 2;
    return QRect(x, y, scaled.width(), scaled.height());
}

QPoint PreviewWidget::widgetToImage(const QPoint& widgetPos) const
{
    if (m_image.isNull()) {
        return {};
    }

    const QRect target = targetRect();
    const int clampedX = std::clamp(widgetPos.x(), target.left(), target.right());
    const int clampedY = std::clamp(widgetPos.y(), target.top(), target.bottom());

    const double xRatio = static_cast<double>(clampedX - target.left()) / std::max(1, target.width());
    const double yRatio = static_cast<double>(clampedY - target.top()) / std::max(1, target.height());

    return QPoint(
        std::clamp(static_cast<int>(xRatio * m_image.width()), 0, m_image.width() - 1),
        std::clamp(static_cast<int>(yRatio * m_image.height()), 0, m_image.height() - 1));
}

QRect PreviewWidget::imageToWidget(const QRect& imageRect) const
{
    if (m_image.isNull() || imageRect.isEmpty()) {
        return {};
    }

    const QRect target = targetRect();
    const double scaleX = static_cast<double>(target.width()) / m_image.width();
    const double scaleY = static_cast<double>(target.height()) / m_image.height();

    return QRect(
        target.left() + static_cast<int>(imageRect.left() * scaleX),
        target.top() + static_cast<int>(imageRect.top() * scaleY),
        static_cast<int>(imageRect.width() * scaleX),
        static_cast<int>(imageRect.height() * scaleY));
}

void PreviewWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), QColor(248, 249, 252));

    if (!m_image.isNull()) {
        painter.drawImage(targetRect(), m_image);
    } else {
        painter.setPen(QColor(148, 156, 169));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("预览区"));
    }

    painter.setRenderHint(QPainter::Antialiasing, true);

    if (!m_roi.isEmpty()) {
        painter.setPen(QPen(QColor(0, 220, 120), 2));
        painter.drawRect(imageToWidget(m_roi));
    }

    if (m_dragging && !m_dragCurrent.isEmpty()) {
        painter.setPen(QPen(QColor(255, 190, 0), 2, Qt::DashLine));
        painter.drawRect(imageToWidget(m_dragCurrent));
    }
}

void PreviewWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || m_image.isNull()) {
        QWidget::mousePressEvent(event);
        return;
    }

    m_dragging = true;
    m_dragStartImage = widgetToImage(event->pos());
    m_dragCurrent = QRect(m_dragStartImage, m_dragStartImage).normalized();
    update();
}

void PreviewWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_dragging || m_image.isNull()) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QPoint current = widgetToImage(event->pos());
    m_dragCurrent = QRect(m_dragStartImage, current).normalized();
    update();
}

void PreviewWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_dragging || event->button() != Qt::LeftButton || m_image.isNull()) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    m_dragging = false;
    const QPoint endPoint = widgetToImage(event->pos());
    QRect newRoi = QRect(m_dragStartImage, endPoint).normalized();
    newRoi = newRoi.intersected(QRect(QPoint(0, 0), m_image.size()));

    if (newRoi.width() >= 10 && newRoi.height() >= 10) {
        m_roi = newRoi;
        emit roiChanged(m_roi);
    }

    m_dragCurrent = {};
    update();
}
