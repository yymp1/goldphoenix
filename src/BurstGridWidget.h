#pragma once

#include <QImage>
#include <QVector>
#include <QWidget>

class FrameCellWidget;
class QGridLayout;
class QResizeEvent;

class BurstGridWidget : public QWidget
{
    Q_OBJECT

public:
    static constexpr int kColumnCount = 4;
    static constexpr int kMaxCellCount = 20;

    explicit BurstGridWidget(QWidget* parent = nullptr);

    void clear();
    void setResults(const QVector<QImage>& images, const QVector<qint64>& frameNumbers);
    void setCellImage(int index, const QImage& image, qint64 frameNumber = -1);
    void setSelectedIndex(int index);
    int selectedIndex() const;
    void setPeopleCount(int peopleCount);
    int peopleCount() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void cellActivated(int index);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateCellSizes();
    void refreshAccentColors();
    QColor colorForGroup(int groupIndex) const;

    QGridLayout* m_layout = nullptr;
    QVector<FrameCellWidget*> m_cells;
    int m_selectedIndex = -1;
    int m_peopleCount = 0;
    QSize m_cellSize { 260, 172 };
    int m_contentHeight = 0;
};
