#include "BurstGridWidget.h"

#include "FrameCellWidget.h"

#include <QGridLayout>
#include <QResizeEvent>

#include <algorithm>

namespace
{
constexpr int kSpacing = 10;
constexpr int kOuterMargin = 6;
constexpr qreal kCellAspectRatio = 0.675;
}

BurstGridWidget::BurstGridWidget(QWidget* parent)
    : QWidget(parent)
{
    m_layout = new QGridLayout(this);
    m_layout->setContentsMargins(kOuterMargin, kOuterMargin, kOuterMargin, kOuterMargin);
    m_layout->setHorizontalSpacing(kSpacing);
    m_layout->setVerticalSpacing(kSpacing);
    m_layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_layout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    m_cells.reserve(kMaxCellCount);
    for (int index = 0; index < kMaxCellCount; ++index) {
        auto* cell = new FrameCellWidget(index, this);
        connect(cell, &FrameCellWidget::clicked, this, [this](int clickedIndex) {
            setSelectedIndex(clickedIndex);
            emit cellActivated(clickedIndex);
        });

        m_cells.push_back(cell);
        m_layout->addWidget(cell, index / kColumnCount, index % kColumnCount);
    }

    refreshAccentColors();
    updateCellSizes();
}

void BurstGridWidget::clear()
{
    for (FrameCellWidget* cell : m_cells) {
        cell->clearImage();
        cell->setSelected(false);
    }

    m_selectedIndex = -1;
    refreshAccentColors();
}

void BurstGridWidget::setResults(const QVector<QImage>& images, const QVector<qint64>& frameNumbers)
{
    for (int index = 0; index < m_cells.size(); ++index) {
        if (index < images.size()) {
            m_cells.at(index)->setImage(images.at(index));
            m_cells.at(index)->setFrameNumber(index < frameNumbers.size() ? frameNumbers.at(index) : -1);
        } else {
            m_cells.at(index)->clearImage();
        }
    }

    if (m_selectedIndex >= images.size()) {
        setSelectedIndex(images.isEmpty() ? -1 : 0);
    }
}

void BurstGridWidget::setCellImage(int index, const QImage& image, qint64 frameNumber)
{
    if (index < 0 || index >= m_cells.size()) {
        return;
    }

    m_cells.at(index)->setImage(image);
    m_cells.at(index)->setFrameNumber(frameNumber);
}

void BurstGridWidget::setSelectedIndex(int index)
{
    if (index == m_selectedIndex) {
        return;
    }

    if (m_selectedIndex >= 0 && m_selectedIndex < m_cells.size()) {
        m_cells.at(m_selectedIndex)->setSelected(false);
    }

    m_selectedIndex = (index >= 0 && index < m_cells.size()) ? index : -1;

    if (m_selectedIndex >= 0) {
        m_cells.at(m_selectedIndex)->setSelected(true);
    }
}

int BurstGridWidget::selectedIndex() const
{
    return m_selectedIndex;
}

void BurstGridWidget::setPeopleCount(int peopleCount)
{
    const int clampedCount = std::clamp(peopleCount, 0, kMaxCellCount / 2);
    if (m_peopleCount == clampedCount) {
        return;
    }

    m_peopleCount = clampedCount;
    refreshAccentColors();
}

int BurstGridWidget::peopleCount() const
{
    return m_peopleCount;
}

QSize BurstGridWidget::sizeHint() const
{
    return { kColumnCount * m_cellSize.width(), std::max(m_contentHeight, minimumSizeHint().height()) };
}

QSize BurstGridWidget::minimumSizeHint() const
{
    return { (kColumnCount * 180) + (kSpacing * (kColumnCount - 1)) + (kOuterMargin * 2), 3 * 140 };
}

void BurstGridWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateCellSizes();
}

void BurstGridWidget::updateCellSizes()
{
    const QMargins margins = m_layout->contentsMargins();
    const int availableWidth = std::max(
        1,
        width() - margins.left() - margins.right() - (kSpacing * (kColumnCount - 1)));

    const int cellWidth = std::clamp(availableWidth / kColumnCount, 180, 320);
    const int cellHeight = std::max(126, static_cast<int>(cellWidth * kCellAspectRatio));
    const QSize nextSize(cellWidth, cellHeight);

    if (nextSize == m_cellSize && m_contentHeight > 0) {
        return;
    }

    m_cellSize = nextSize;
    for (FrameCellWidget* cell : m_cells) {
        cell->setFixedSize(m_cellSize);
    }

    const int rowCount = (m_cells.size() + kColumnCount - 1) / kColumnCount;
    m_contentHeight = margins.top()
        + margins.bottom()
        + (rowCount * m_cellSize.height())
        + (std::max(0, rowCount - 1) * kSpacing);

    setMinimumHeight(m_contentHeight);
    updateGeometry();
}

void BurstGridWidget::refreshAccentColors()
{
    for (FrameCellWidget* cell : m_cells) {
        cell->clearAccentColor();
    }

    if (m_peopleCount <= 0) {
        return;
    }

    for (int groupIndex = 0; groupIndex < m_peopleCount; ++groupIndex) {
        const QColor accent = colorForGroup(groupIndex);
        const int firstIndex = groupIndex;
        const int secondIndex = groupIndex + m_peopleCount;

        if (firstIndex >= 0 && firstIndex < m_cells.size()) {
            m_cells.at(firstIndex)->setAccentColor(accent);
        }
        if (secondIndex >= 0 && secondIndex < m_cells.size()) {
            m_cells.at(secondIndex)->setAccentColor(accent);
        }
    }
}

QColor BurstGridWidget::colorForGroup(int groupIndex) const
{
    static const QVector<QColor> palette = {
        QColor(166, 37, 45),
        QColor(24, 88, 201),
        QColor(28, 135, 62),
        QColor(103, 47, 173),
        QColor(201, 108, 18),
        QColor(0, 131, 148),
        QColor(175, 33, 123),
        QColor(166, 128, 22),
        QColor(150, 41, 78),
        QColor(74, 109, 118)
    };

    if (palette.isEmpty()) {
        return QColor(79, 142, 247);
    }

    return palette.at(groupIndex % palette.size());
}
