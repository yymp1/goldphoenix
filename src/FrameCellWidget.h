#pragma once

#include <QColor>
#include <QImage>
#include <QWidget>

class QEnterEvent;
class QEvent;
class QMouseEvent;
class QPaintEvent;
class QWheelEvent;
class QAbstractScrollArea;

class FrameCellWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FrameCellWidget(int slotIndex, QWidget* parent = nullptr);
    ~FrameCellWidget() override;

    int slotIndex() const;
    void setSlotIndex(int index);

    void setImage(const QImage& image);
    void clearImage();
    bool hasImage() const;

    void setFrameNumber(qint64 frameNumber);
    qint64 frameNumber() const;

    void setAccentColor(const QColor& color);
    void clearAccentColor();

    void setSelected(bool selected);
    bool isSelected() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void clicked(int index);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent* event) override;
#else
    void enterEvent(QEvent* event) override;
#endif
    void leaveEvent(QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    QRectF contentRectF() const;
    QRectF aspectFillSourceRect(const QSizeF& targetSize) const;
    QRectF zoomSourceRect(const QRectF& baseSourceRect) const;
    QPointF clampedMousePos(const QRectF& targetRect) const;
    QPointF currentFocusPoint(const QRectF& baseSourceRect) const;
    bool isZoomViewActive() const;
    void updateAltState(bool altPressed);
    void activateZoomView();
    void resetZoomView();
    void updateFocusFromMouse();
    void ensureWheelRelayInstalled();
    bool handleZoomWheel(QWheelEvent* event, bool fromGlobalFilter);
    bool shouldHandleZoomInput(const QPointF& localPos, bool altPressed) const;

    static constexpr qreal kDefaultZoomFactor = 2.5;
    static constexpr qreal kMinZoomFactor = 1.0;
    static constexpr qreal kMaxZoomFactor = 10.0;
    static constexpr qreal kZoomStep = 0.5;

    int m_slotIndex = 0;
    QImage m_image;
    qint64 m_frameNumber = -1;
    QColor m_accentColor;
    bool m_hovered = false;
    bool m_selected = false;
    bool m_altPressed = false;
    bool m_zoomViewActive = false;
    QObject* m_wheelRelaySource = nullptr;
    QPointF m_mousePos;
    QPointF m_focusPoint;
    qreal m_zoomFactor = kDefaultZoomFactor;
};
