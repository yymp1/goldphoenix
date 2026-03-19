#pragma once

#include <QImage>
#include <QWidget>

class PreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewWidget(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void setRoi(const QRect& roi);
    QRect roi() const;
    QSize imageSize() const;

signals:
    void roiChanged(const QRect& roi);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRect targetRect() const;
    QPoint widgetToImage(const QPoint& widgetPos) const;
    QRect imageToWidget(const QRect& imageRect) const;

    QImage m_image;
    QRect m_roi;
    bool m_dragging = false;
    QPoint m_dragStartImage;
    QRect m_dragCurrent;
};
