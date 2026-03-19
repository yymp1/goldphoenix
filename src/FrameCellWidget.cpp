#include "FrameCellWidget.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QCursor>
#include <QEnterEvent>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QWheelEvent>

#include <algorithm>

namespace
{
QColor defaultBorderColor()
{
    return QColor(185, 192, 203);
}

QColor hoverOverlayColor()
{
    return QColor(61, 102, 170, 18);
}
}

FrameCellWidget::FrameCellWidget(int slotIndex, QWidget* parent)
    : QWidget(parent)
    , m_slotIndex(slotIndex)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    qApp->installEventFilter(this);
}

FrameCellWidget::~FrameCellWidget()
{
    if (m_wheelRelaySource) {
        m_wheelRelaySource->removeEventFilter(this);
    }
    qApp->removeEventFilter(this);
}

int FrameCellWidget::slotIndex() const
{
    return m_slotIndex;
}

void FrameCellWidget::setSlotIndex(int index)
{
    if (m_slotIndex == index) {
        return;
    }

    m_slotIndex = index;
    update();
}

void FrameCellWidget::setImage(const QImage& image)
{
    m_image = image;
    resetZoomView();
    update();
}

void FrameCellWidget::clearImage()
{
    m_image = {};
    m_frameNumber = -1;
    resetZoomView();
    update();
}

bool FrameCellWidget::hasImage() const
{
    return !m_image.isNull();
}

void FrameCellWidget::setFrameNumber(qint64 frameNumber)
{
    if (m_frameNumber == frameNumber) {
        return;
    }

    m_frameNumber = frameNumber;
    update();
}

qint64 FrameCellWidget::frameNumber() const
{
    return m_frameNumber;
}

void FrameCellWidget::setAccentColor(const QColor& color)
{
    m_accentColor = color;
    update();
}

void FrameCellWidget::clearAccentColor()
{
    if (!m_accentColor.isValid()) {
        return;
    }

    m_accentColor = {};
    update();
}

void FrameCellWidget::setSelected(bool selected)
{
    if (m_selected == selected) {
        return;
    }

    m_selected = selected;
    update();
}

bool FrameCellWidget::isSelected() const
{
    return m_selected;
}

QSize FrameCellWidget::sizeHint() const
{
    return { 260, 172 };
}

QSize FrameCellWidget::minimumSizeHint() const
{
    return { 180, 126 };
}

bool FrameCellWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        updateAltState(QApplication::keyboardModifiers().testFlag(Qt::AltModifier));
    } else if ((watched == qApp || watched == m_wheelRelaySource) && event->type() == QEvent::Wheel) {
        if (handleZoomWheel(static_cast<QWheelEvent*>(event), true)) {
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void FrameCellWidget::updateAltState(bool altPressed)
{
    if (m_altPressed == altPressed) {
        return;
    }

    m_altPressed = altPressed;
    if (m_altPressed && m_hovered && hasImage()) {
        activateZoomView();
        updateFocusFromMouse();
    }

    if (hasImage() && (m_hovered || m_zoomViewActive)) {
        update();
    }
}

void FrameCellWidget::ensureWheelRelayInstalled()
{
    if (m_wheelRelaySource) {
        return;
    }

    QWidget* cursor = parentWidget();
    while (cursor) {
        if (auto* scrollArea = qobject_cast<QAbstractScrollArea*>(cursor)) {
            m_wheelRelaySource = scrollArea->viewport();
            break;
        }
        cursor = cursor->parentWidget();
    }

    if (m_wheelRelaySource) {
        m_wheelRelaySource->installEventFilter(this);
    }
}

void FrameCellWidget::activateZoomView()
{
    if (!hasImage()) {
        return;
    }

    if (!m_zoomViewActive) {
        m_zoomViewActive = true;
        if (m_zoomFactor < kMinZoomFactor || m_zoomFactor > kMaxZoomFactor) {
            m_zoomFactor = kDefaultZoomFactor;
        }
        if (m_focusPoint.x() < 0.0 || m_focusPoint.y() < 0.0) {
            m_focusPoint = QPointF(m_image.width() * 0.5, m_image.height() * 0.5);
        }
    }
}

void FrameCellWidget::resetZoomView()
{
    m_zoomViewActive = false;
    m_zoomFactor = kDefaultZoomFactor;

    if (m_image.isNull()) {
        m_focusPoint = QPointF(-1.0, -1.0);
    } else {
        m_focusPoint = QPointF(m_image.width() * 0.5, m_image.height() * 0.5);
    }
}

void FrameCellWidget::updateFocusFromMouse()
{
    if (!hasImage()) {
        return;
    }

    const QRectF targetRect = contentRectF();
    const QRectF baseSourceRect = aspectFillSourceRect(targetRect.size());
    if (targetRect.isEmpty() || baseSourceRect.isEmpty()) {
        return;
    }

    const QPointF mouse = clampedMousePos(targetRect);
    const qreal xRatio = (mouse.x() - targetRect.left()) / std::max<qreal>(1.0, targetRect.width());
    const qreal yRatio = (mouse.y() - targetRect.top()) / std::max<qreal>(1.0, targetRect.height());

    m_focusPoint = QPointF(
        baseSourceRect.left() + (baseSourceRect.width() * xRatio),
        baseSourceRect.top() + (baseSourceRect.height() * yRatio));
}

bool FrameCellWidget::handleZoomWheel(QWheelEvent* event, bool fromGlobalFilter)
{
    const bool altPressed = QApplication::keyboardModifiers().testFlag(Qt::AltModifier);
    updateAltState(altPressed);

    QPointF localPos;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    localPos = fromGlobalFilter ? mapFromGlobal(QCursor::pos()) : event->position();
#else
    localPos = fromGlobalFilter ? mapFromGlobal(QCursor::pos()) : event->pos();
#endif

    if (!shouldHandleZoomInput(localPos, altPressed)) {
        return false;
    }

    m_hovered = true;
    m_mousePos = localPos;
    activateZoomView();
    updateFocusFromMouse();

    const QPoint angleDelta = event->angleDelta();
    if (angleDelta.y() == 0) {
        event->accept();
        return true;
    }

    const qreal direction = angleDelta.y() > 0 ? 1.0 : -1.0;
    m_zoomFactor = std::clamp(m_zoomFactor + (direction * kZoomStep), kMinZoomFactor, kMaxZoomFactor);
    update();
    event->accept();
    return true;
}

bool FrameCellWidget::shouldHandleZoomInput(const QPointF& localPos, bool altPressed) const
{
    if (!hasImage() || !rect().contains(localPos.toPoint())) {
        return false;
    }

    // On Windows inside QScrollArea, Alt + wheel can be delivered without a stable
    // modifier state once the zoom view is already active. Allow the active cell to
    // continue owning wheel zoom so it can actually exceed the default 2.5x.
    return altPressed || m_zoomViewActive;
}

QRectF FrameCellWidget::contentRectF() const
{
    return QRectF(rect()).adjusted(8.0, 8.0, -8.0, -8.0);
}

QRectF FrameCellWidget::aspectFillSourceRect(const QSizeF& targetSize) const
{
    if (m_image.isNull() || targetSize.width() <= 1.0 || targetSize.height() <= 1.0) {
        return {};
    }

    const QSizeF imageSize = m_image.size();
    const qreal imageAspect = imageSize.width() / imageSize.height();
    const qreal targetAspect = targetSize.width() / targetSize.height();

    if (imageAspect > targetAspect) {
        const qreal croppedWidth = imageSize.height() * targetAspect;
        const qreal left = (imageSize.width() - croppedWidth) * 0.5;
        return QRectF(left, 0.0, croppedWidth, imageSize.height());
    }

    const qreal croppedHeight = imageSize.width() / targetAspect;
    const qreal top = (imageSize.height() - croppedHeight) * 0.5;
    return QRectF(0.0, top, imageSize.width(), croppedHeight);
}

QPointF FrameCellWidget::clampedMousePos(const QRectF& targetRect) const
{
    return QPointF(
        std::clamp(m_mousePos.x(), targetRect.left(), targetRect.right()),
        std::clamp(m_mousePos.y(), targetRect.top(), targetRect.bottom()));
}

QPointF FrameCellWidget::currentFocusPoint(const QRectF& baseSourceRect) const
{
    if (m_focusPoint.x() < 0.0 || m_focusPoint.y() < 0.0) {
        return baseSourceRect.center();
    }

    return QPointF(
        std::clamp(m_focusPoint.x(), 0.0, std::max<qreal>(0.0, m_image.width() - 1.0)),
        std::clamp(m_focusPoint.y(), 0.0, std::max<qreal>(0.0, m_image.height() - 1.0)));
}

QRectF FrameCellWidget::zoomSourceRect(const QRectF& baseSourceRect) const
{
    if (baseSourceRect.isEmpty()) {
        return baseSourceRect;
    }

    const QPointF focus = currentFocusPoint(baseSourceRect);

    const QSizeF zoomSize(
        std::max<qreal>(24.0, baseSourceRect.width() / m_zoomFactor),
        std::max<qreal>(24.0, baseSourceRect.height() / m_zoomFactor));

    const qreal maxLeft = std::max<qreal>(0.0, m_image.width() - zoomSize.width());
    const qreal maxTop = std::max<qreal>(0.0, m_image.height() - zoomSize.height());
    const qreal left = std::clamp(focus.x() - (zoomSize.width() * 0.5), 0.0, maxLeft);
    const qreal top = std::clamp(focus.y() - (zoomSize.height() * 0.5), 0.0, maxTop);

    return QRectF(left, top, zoomSize.width(), zoomSize.height());
}

bool FrameCellWidget::isZoomViewActive() const
{
    return hasImage() && m_zoomViewActive;
}

void FrameCellWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF outerRect = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
    const QRectF contentRect = contentRectF();
    const QColor panelColor = m_hovered ? QColor(248, 250, 255) : QColor(255, 255, 255);

    painter.setPen(Qt::NoPen);
    painter.setBrush(panelColor);
    painter.drawRoundedRect(outerRect, 12.0, 12.0);

    if (hasImage()) {
        painter.save();
        QPainterPath clipPath;
        clipPath.addRoundedRect(contentRect, 9.0, 9.0);
        painter.setClipPath(clipPath);
        painter.setClipRect(contentRect);

        const QRectF baseSourceRect = aspectFillSourceRect(contentRect.size());
        const QRectF sourceRect = isZoomViewActive()
            ? zoomSourceRect(baseSourceRect)
            : baseSourceRect;

        painter.drawImage(contentRect, m_image, sourceRect);

        if (m_hovered && !isZoomViewActive()) {
            painter.fillRect(contentRect, hoverOverlayColor());
        }

        painter.restore();
    } else {
        painter.setPen(QColor(145, 151, 161));
        QFont placeholderFont = painter.font();
        placeholderFont.setBold(true);
        placeholderFont.setPixelSize(26);
        painter.setFont(placeholderFont);
        painter.drawText(contentRect, Qt::AlignCenter, QString::number(m_slotIndex + 1));
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 220));
    painter.drawRoundedRect(QRectF(14.0, 12.0, 44.0, 24.0), 6.0, 6.0);
    painter.setPen(QColor(74, 82, 94));
    painter.setFont(QFont(QStringLiteral("Segoe UI"), 9, QFont::DemiBold));
    painter.drawText(QRectF(14.0, 12.0, 44.0, 24.0), Qt::AlignCenter, QString::number(m_slotIndex + 1));

    if (isZoomViewActive()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 228));
        painter.drawRoundedRect(QRectF(width() - 106.0, 12.0, 92.0, 24.0), 6.0, 6.0);
        painter.setPen(QColor(56, 66, 82));
        painter.drawText(QRectF(width() - 106.0, 12.0, 92.0, 24.0), Qt::AlignCenter, QStringLiteral("%1x").arg(m_zoomFactor, 0, 'f', 2));
    }

    QColor borderColor = m_accentColor.isValid() ? m_accentColor : defaultBorderColor();
    if (m_hovered) {
        borderColor = borderColor.lighter(m_accentColor.isValid() ? 106 : 112);
    }

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(borderColor, m_accentColor.isValid() ? 3.8 : 1.6));
    painter.drawRoundedRect(outerRect, 12.0, 12.0);

    if (m_selected) {
        painter.setPen(QPen(QColor(58, 95, 150, 190), 2.0));
        painter.drawRoundedRect(outerRect.adjusted(5.0, 5.0, -5.0, -5.0), 9.0, 9.0);
    }
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void FrameCellWidget::enterEvent(QEnterEvent* event)
#else
void FrameCellWidget::enterEvent(QEvent* event)
#endif
{
    m_hovered = true;
    ensureWheelRelayInstalled();
    updateAltState(QApplication::keyboardModifiers().testFlag(Qt::AltModifier));
    if (!rect().isEmpty()) {
        m_mousePos = rect().center();
    }
    if (m_altPressed && hasImage()) {
        activateZoomView();
        updateFocusFromMouse();
    }
    update();
    QWidget::enterEvent(event);
}

void FrameCellWidget::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void FrameCellWidget::mouseMoveEvent(QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_mousePos = event->position();
#else
    m_mousePos = event->localPos();
#endif
    updateAltState(QApplication::keyboardModifiers().testFlag(Qt::AltModifier));
    if (hasImage() && m_altPressed) {
        activateZoomView();
        updateFocusFromMouse();
    }
    if (hasImage()) {
        update();
    }

    QWidget::mouseMoveEvent(event);
}

void FrameCellWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->pos()) && hasImage()) {
        emit clicked(m_slotIndex);
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void FrameCellWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && hasImage()) {
        resetZoomView();
        emit clicked(m_slotIndex);
        update();
        event->accept();
        return;
    }

    QWidget::mouseDoubleClickEvent(event);
}

void FrameCellWidget::wheelEvent(QWheelEvent* event)
{
    if (!handleZoomWheel(event, false)) {
        event->ignore();
        QWidget::wheelEvent(event);
    }
}
