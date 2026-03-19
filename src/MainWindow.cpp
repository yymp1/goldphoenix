#include "MainWindow.h"

#include "ImageUtils.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <QDir>
#include <QPixmap>
#include <QScrollBar>

#include <algorithm>

namespace
{
constexpr int kMaxGridCells = BurstGridWidget::kMaxCellCount;
constexpr int kToolbarControlHeight = 28;

void tuneCompactButton(QPushButton* button, bool emphasis = false)
{
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedHeight(kToolbarControlHeight);
    if (emphasis) {
        button->setProperty("emphasis", true);
    }
}

void tuneCompactCombo(QComboBox* combo, int minWidth = 110)
{
    combo->setMinimumWidth(minWidth);
    combo->setFixedHeight(kToolbarControlHeight);
}

void tuneCompactSpin(QSpinBox* spin, int minWidth = 72)
{
    spin->setMinimumWidth(minWidth);
    spin->setFixedHeight(kToolbarControlHeight);
}

void tuneCompactEdit(QLineEdit* edit, int minWidth = 0)
{
    if (minWidth > 0) {
        edit->setMinimumWidth(minWidth);
    }
    edit->setFixedHeight(kToolbarControlHeight);
}

QLabel* createSectionTitle(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("sectionTitle"));
    return label;
}

QLabel* createInfoChip(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("infoChip"));
    label->setWordWrap(true);
    return label;
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_settings(m_settingsManager.load())
    , m_ringBuffer(std::make_shared<RingBuffer>(m_settings.ringBufferSize))
    , m_processingWorker(std::make_unique<ProcessingWorker>(m_ringBuffer))
{
    setupUi();
    applySettingsToUi();

    connect(m_sourceCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::onSourceModeChanged);
    connect(m_refreshCameraButton, &QPushButton::clicked, this, &MainWindow::refreshCameras);
    connect(m_openVideoButton, &QPushButton::clicked, this, &MainWindow::onOpenVideo);
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartPreview);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopPreview);
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::onPlay);
    connect(m_pauseButton, &QPushButton::clicked, this, &MainWindow::onPause);
    connect(m_nextFrameButton, &QPushButton::clicked, this, &MainWindow::onNextFrame);
    connect(m_manualTriggerButton, &QPushButton::clicked, [this]() {
        m_processingWorker->requestManualTrigger();
    });
    connect(m_saveBurstButton, &QPushButton::clicked, this, &MainWindow::onSaveBurst);
    connect(m_clearButton, &QPushButton::clicked, this, &MainWindow::onClearResults);
    connect(m_previewWidget, &PreviewWidget::roiChanged, this, &MainWindow::onRoiChanged);
    connect(m_seekSlider, &QSlider::sliderPressed, this, &MainWindow::onSeekSliderPressed);
    connect(m_seekSlider, &QSlider::sliderReleased, this, &MainWindow::onSeekSliderReleased);
    connect(m_outputDirButton, &QPushButton::clicked, this, &MainWindow::onBrowseOutputDir);
    connect(m_outputDirEdit, &QLineEdit::editingFinished, this, &MainWindow::onParametersChanged);
    connect(m_loopCheck, &QCheckBox::toggled, this, &MainWindow::onParametersChanged);
    connect(m_cameraCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::onParametersChanged);
    connect(m_thresholdSpin, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onParametersChanged);
    connect(m_minAreaSpin, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onParametersChanged);
    connect(m_cooldownSpin, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onParametersChanged);
    connect(m_burstCountSpin, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onParametersChanged);
    connect(m_intervalSpin, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onParametersChanged);
    connect(m_peopleSpin, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onParametersChanged);
    connect(m_burstGrid, &BurstGridWidget::cellActivated, this, &MainWindow::onGridCellActivated);

    connectWorker();

    m_processingWorker->setMotionParameters(m_settings.motion);
    m_processingWorker->setBurstParameters(m_settings.burst);
    m_processingWorker->setAutoTriggerEnabled(true);
    m_processingWorker->start();

    refreshCameras();
    rebuildSource(m_settings.inputSourceType);

    m_previewTimer = new QTimer(this);
    connect(m_previewTimer, &QTimer::timeout, this, &MainWindow::onRefreshPreview);
    m_previewTimer->start(33);

    onStatusChanged(QStringLiteral("未触发"));
    onMotionStateChanged(false, 0.0);
    updateVideoUi({});
    updateBurstSummary();
    resetSelectedPreview();
    statusBar()->showMessage(QStringLiteral("就绪"), 3000);
}

MainWindow::~MainWindow()
{
    if (m_previewTimer) {
        m_previewTimer->stop();
    }

    if (m_source) {
        m_source->stop();
    }

    if (m_processingWorker) {
        m_processingWorker->stop();
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveSettings();

    if (m_source) {
        m_source->stop();
    }

    if (m_processingWorker) {
        m_processingWorker->stop();
    }

    QMainWindow::closeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    if (m_selectedBurstIndex >= 0) {
        selectPreviewImage(m_selectedBurstIndex);
    }
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("超级无敌金凤凰1.0"));
    setMinimumSize(1380, 820);
    resize(1580, 930);

    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("rootWidget"));

    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);

    auto* toolbarFrame = new QFrame(central);
    toolbarFrame->setObjectName(QStringLiteral("toolbarFrame"));
    toolbarFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* toolbarLayout = new QHBoxLayout(toolbarFrame);
    toolbarLayout->setContentsMargins(10, 6, 10, 6);
    toolbarLayout->setSpacing(6);

    m_sourceCombo = new QComboBox(toolbarFrame);
    m_sourceCombo->addItem(QStringLiteral("摄像头"), static_cast<int>(InputSourceType::Camera));
    m_sourceCombo->addItem(QStringLiteral("视频测试"), static_cast<int>(InputSourceType::VideoFile));
    tuneCompactCombo(m_sourceCombo, 108);

    m_cameraCombo = new QComboBox(toolbarFrame);
    tuneCompactCombo(m_cameraCombo, 140);

    m_refreshCameraButton = new QPushButton(QStringLiteral("刷新"), toolbarFrame);
    m_openVideoButton = new QPushButton(QStringLiteral("打开视频"), toolbarFrame);
    m_startButton = new QPushButton(QStringLiteral("开始"), toolbarFrame);
    m_stopButton = new QPushButton(QStringLiteral("停止"), toolbarFrame);
    m_playButton = new QPushButton(QStringLiteral("播放"), toolbarFrame);
    m_pauseButton = new QPushButton(QStringLiteral("暂停"), toolbarFrame);
    m_nextFrameButton = new QPushButton(QStringLiteral("下一帧"), toolbarFrame);
    m_manualTriggerButton = new QPushButton(QStringLiteral("手动触发"), toolbarFrame);
    m_saveBurstButton = new QPushButton(QStringLiteral("保存"), toolbarFrame);
    m_clearButton = new QPushButton(QStringLiteral("清空"), toolbarFrame);

    tuneCompactButton(m_refreshCameraButton);
    tuneCompactButton(m_openVideoButton);
    tuneCompactButton(m_startButton, true);
    tuneCompactButton(m_stopButton);
    tuneCompactButton(m_playButton);
    tuneCompactButton(m_pauseButton);
    tuneCompactButton(m_nextFrameButton);
    tuneCompactButton(m_manualTriggerButton);
    tuneCompactButton(m_saveBurstButton);
    tuneCompactButton(m_clearButton);

    auto* sourceLabel = new QLabel(QStringLiteral("输入源"), toolbarFrame);
    sourceLabel->setObjectName(QStringLiteral("toolbarLabel"));
    auto* cameraLabel = new QLabel(QStringLiteral("设备"), toolbarFrame);
    cameraLabel->setObjectName(QStringLiteral("toolbarLabel"));

    m_toolbarStateLabel = createInfoChip(QStringLiteral("状态：未触发"), toolbarFrame);
    m_toolbarStateLabel->setMinimumWidth(180);

    toolbarLayout->addWidget(sourceLabel);
    toolbarLayout->addWidget(m_sourceCombo);
    toolbarLayout->addWidget(cameraLabel);
    toolbarLayout->addWidget(m_cameraCombo);
    toolbarLayout->addWidget(m_refreshCameraButton);
    toolbarLayout->addWidget(m_openVideoButton);
    toolbarLayout->addSpacing(4);
    toolbarLayout->addWidget(m_startButton);
    toolbarLayout->addWidget(m_stopButton);
    toolbarLayout->addWidget(m_playButton);
    toolbarLayout->addWidget(m_pauseButton);
    toolbarLayout->addWidget(m_nextFrameButton);
    toolbarLayout->addWidget(m_manualTriggerButton);
    toolbarLayout->addWidget(m_saveBurstButton);
    toolbarLayout->addWidget(m_clearButton);
    toolbarLayout->addStretch(1);
    toolbarLayout->addWidget(m_toolbarStateLabel);
    rootLayout->addWidget(toolbarFrame);

    auto* contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(8);

    auto* leftPanel = new QFrame(central);
    leftPanel->setObjectName(QStringLiteral("mainCard"));
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(10, 10, 10, 10);
    leftLayout->setSpacing(8);

    auto* gridHeaderLayout = new QHBoxLayout();
    auto* titleLabel = createSectionTitle(QStringLiteral("关键帧工作台"), leftPanel);
    m_gridSummaryLabel = new QLabel(leftPanel);
    m_gridSummaryLabel->setObjectName(QStringLiteral("captionLabel"));
    gridHeaderLayout->addWidget(titleLabel);
    gridHeaderLayout->addStretch(1);
    gridHeaderLayout->addWidget(m_gridSummaryLabel);
    leftLayout->addLayout(gridHeaderLayout);

    m_gridScrollArea = new QScrollArea(leftPanel);
    m_gridScrollArea->setWidgetResizable(true);
    m_gridScrollArea->setFrameShape(QFrame::NoFrame);
    m_gridScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_gridScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_gridScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_gridScrollArea->verticalScrollBar()->setSingleStep(24);

    m_burstGrid = new BurstGridWidget(m_gridScrollArea);
    m_gridScrollArea->setWidget(m_burstGrid);
    leftLayout->addWidget(m_gridScrollArea, 1);

    auto* rightPanel = new QWidget(central);
    rightPanel->setMinimumWidth(300);
    rightPanel->setMaximumWidth(380);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);

    auto* previewCard = new QFrame(rightPanel);
    previewCard->setObjectName(QStringLiteral("sideCard"));
    auto* previewLayout = new QVBoxLayout(previewCard);
    previewLayout->setContentsMargins(10, 10, 10, 10);
    previewLayout->setSpacing(8);
    previewLayout->addWidget(createSectionTitle(QStringLiteral("输入预览"), previewCard));

    m_previewWidget = new PreviewWidget(previewCard);
    m_previewWidget->setMinimumSize(260, 156);
    m_previewWidget->setMaximumHeight(206);
    m_previewWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    previewLayout->addWidget(m_previewWidget);
    rightLayout->addWidget(previewCard);

    auto* selectedCard = new QFrame(rightPanel);
    selectedCard->setObjectName(QStringLiteral("sideCard"));
    auto* selectedLayout = new QVBoxLayout(selectedCard);
    selectedLayout->setContentsMargins(10, 10, 10, 10);
    selectedLayout->setSpacing(8);
    selectedLayout->addWidget(createSectionTitle(QStringLiteral("选中关键帧"), selectedCard));

    m_selectedInfoLabel = new QLabel(QStringLiteral("点击左侧关键帧查看"), selectedCard);
    m_selectedInfoLabel->setObjectName(QStringLiteral("captionLabel"));
    selectedLayout->addWidget(m_selectedInfoLabel);

    m_largePreviewLabel = new QLabel(QStringLiteral("Alt + 鼠标可在左侧格子内局部放大"), selectedCard);
    m_largePreviewLabel->setAlignment(Qt::AlignCenter);
    m_largePreviewLabel->setMinimumHeight(180);
    m_largePreviewLabel->setMaximumHeight(220);
    m_largePreviewLabel->setObjectName(QStringLiteral("previewLabel"));
    selectedLayout->addWidget(m_largePreviewLabel);
    rightLayout->addWidget(selectedCard);

    auto* controlScroll = new QScrollArea(rightPanel);
    controlScroll->setWidgetResizable(true);
    controlScroll->setFrameShape(QFrame::NoFrame);
    controlScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    controlScroll->verticalScrollBar()->setSingleStep(20);

    auto* controlContent = new QFrame(controlScroll);
    controlContent->setObjectName(QStringLiteral("controlCard"));
    auto* controlLayout = new QVBoxLayout(controlContent);
    controlLayout->setContentsMargins(10, 10, 10, 10);
    controlLayout->setSpacing(10);

    controlLayout->addWidget(createSectionTitle(QStringLiteral("状态"), controlContent));
    m_statusLabel = createInfoChip(QStringLiteral("状态：未触发"), controlContent);
    m_motionLabel = createInfoChip(QStringLiteral("运动：无"), controlContent);
    m_inputTypeLabel = createInfoChip(QStringLiteral("输入源：视频文件"), controlContent);
    m_videoInfoLabel = createInfoChip(QStringLiteral("帧 0 / 0 | 时间 00:00.000 / 00:00.000 | 已暂停"), controlContent);
    controlLayout->addWidget(m_statusLabel);
    controlLayout->addWidget(m_motionLabel);
    controlLayout->addWidget(m_inputTypeLabel);
    controlLayout->addWidget(m_videoInfoLabel);

    controlLayout->addSpacing(2);
    controlLayout->addWidget(createSectionTitle(QStringLiteral("视频"), controlContent));

    auto* pathRow = new QHBoxLayout();
    pathRow->setSpacing(6);
    m_videoPathEdit = new QLineEdit(controlContent);
    m_videoPathEdit->setPlaceholderText(QStringLiteral("选择本地视频文件"));
    tuneCompactEdit(m_videoPathEdit);
    auto* openInlineButton = new QPushButton(QStringLiteral("浏览"), controlContent);
    tuneCompactButton(openInlineButton);
    connect(openInlineButton, &QPushButton::clicked, this, &MainWindow::onOpenVideo);
    pathRow->addWidget(m_videoPathEdit, 1);
    pathRow->addWidget(openInlineButton);
    controlLayout->addLayout(pathRow);

    m_seekSlider = new QSlider(Qt::Horizontal, controlContent);
    m_seekSlider->setRange(0, 0);
    controlLayout->addWidget(m_seekSlider);

    auto* displayForm = new QFormLayout();
    displayForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    displayForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    displayForm->setHorizontalSpacing(10);
    displayForm->setVerticalSpacing(8);

    m_loopCheck = new QCheckBox(QStringLiteral("循环播放"), controlContent);
    m_peopleSpin = new QSpinBox(controlContent);
    m_peopleSpin->setRange(0, 10);
    m_peopleSpin->setSpecialValueText(QStringLiteral("关闭"));
    tuneCompactSpin(m_peopleSpin, 84);
    displayForm->addRow(QStringLiteral("人数"), m_peopleSpin);
    displayForm->addRow(QStringLiteral("循环"), m_loopCheck);
    controlLayout->addLayout(displayForm);

    controlLayout->addSpacing(2);
    controlLayout->addWidget(createSectionTitle(QStringLiteral("参数"), controlContent));

    m_thresholdSpin = new QSpinBox(controlContent);
    m_thresholdSpin->setRange(1, 255);
    tuneCompactSpin(m_thresholdSpin);

    m_minAreaSpin = new QSpinBox(controlContent);
    m_minAreaSpin->setRange(1, 500000);
    tuneCompactSpin(m_minAreaSpin, 96);

    m_cooldownSpin = new QSpinBox(controlContent);
    m_cooldownSpin->setRange(0, 10000);
    m_cooldownSpin->setSuffix(QStringLiteral(" ms"));
    tuneCompactSpin(m_cooldownSpin, 96);

    m_burstCountSpin = new QSpinBox(controlContent);
    m_burstCountSpin->setRange(1, kMaxGridCells);
    tuneCompactSpin(m_burstCountSpin, 84);

    m_intervalSpin = new QSpinBox(controlContent);
    m_intervalSpin->setRange(1, 2);
    tuneCompactSpin(m_intervalSpin, 84);

    m_outputDirEdit = new QLineEdit(controlContent);
    tuneCompactEdit(m_outputDirEdit);
    m_outputDirButton = new QPushButton(QStringLiteral("浏览"), controlContent);
    tuneCompactButton(m_outputDirButton);

    auto* outputRow = new QHBoxLayout();
    outputRow->setSpacing(6);
    outputRow->addWidget(m_outputDirEdit, 1);
    outputRow->addWidget(m_outputDirButton);

    auto* parameterLayout = new QFormLayout();
    parameterLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    parameterLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    parameterLayout->setHorizontalSpacing(10);
    parameterLayout->setVerticalSpacing(8);
    parameterLayout->addRow(QStringLiteral("运动阈值"), m_thresholdSpin);
    parameterLayout->addRow(QStringLiteral("最小面积"), m_minAreaSpin);
    parameterLayout->addRow(QStringLiteral("冷却时间"), m_cooldownSpin);
    parameterLayout->addRow(QStringLiteral("结果上限"), m_burstCountSpin);
    parameterLayout->addRow(QStringLiteral("单动作抓拍数"), m_intervalSpin);
    parameterLayout->addRow(QStringLiteral("输出目录"), outputRow);
    controlLayout->addLayout(parameterLayout);
    controlLayout->addStretch(1);

    controlScroll->setWidget(controlContent);
    rightLayout->addWidget(controlScroll, 1);

    contentLayout->addWidget(leftPanel, 4);
    contentLayout->addWidget(rightPanel, 1);
    rootLayout->addLayout(contentLayout, 1);

    setCentralWidget(central);

    setStyleSheet(QStringLiteral(
        "QWidget#rootWidget { background: #f6f7f9; color: #253041; }"
        "QFrame#toolbarFrame, QFrame#mainCard, QFrame#sideCard, QFrame#controlCard {"
        "  background: #ffffff;"
        "  border: 1px solid #d8dde6;"
        "  border-radius: 10px;"
        "}"
        "QLabel#sectionTitle { color: #1f2a3b; font-size: 15px; font-weight: 600; }"
        "QLabel#captionLabel { color: #687585; font-size: 12px; }"
        "QLabel#toolbarLabel { color: #526071; }"
        "QLabel#infoChip {"
        "  background: #fafbfd;"
        "  border: 1px solid #d6dde8;"
        "  border-radius: 7px;"
        "  padding: 6px 8px;"
        "  color: #233143;"
        "}"
        "QLabel#previewLabel {"
        "  background: #ffffff;"
        "  border: 1px solid #d8dde6;"
        "  border-radius: 8px;"
        "  color: #677486;"
        "}"
        "QPushButton, QComboBox, QLineEdit, QSpinBox {"
        "  background: #ffffff;"
        "  border: 1px solid #cfd7e3;"
        "  border-radius: 6px;"
        "  padding: 3px 8px;"
        "  color: #233143;"
        "}"
        "QPushButton:hover, QComboBox:hover, QLineEdit:hover, QSpinBox:hover { border-color: #88a2c5; background: #fcfdff; }"
        "QPushButton:pressed { background: #eef3fa; }"
        "QPushButton[emphasis=\"true\"] { background: #1e67d2; border-color: #1e67d2; color: white; }"
        "QPushButton[emphasis=\"true\"]:hover { background: #2d75df; border-color: #2d75df; }"
        "QCheckBox { color: #314154; spacing: 6px; }"
        "QCheckBox::indicator { width: 14px; height: 14px; }"
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { background: #edf1f6; width: 10px; margin: 2px; border-radius: 5px; }"
        "QScrollBar::handle:vertical { background: #c2ccd9; min-height: 32px; border-radius: 5px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QSlider::groove:horizontal { background: #d6dee9; height: 6px; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #4b84d8; width: 16px; margin: -5px 0; border-radius: 8px; }"
    ));
}

void MainWindow::connectWorker()
{
    connect(m_processingWorker.get(), &ProcessingWorker::motionStateChanged, this, &MainWindow::onMotionStateChanged);
    connect(m_processingWorker.get(), &ProcessingWorker::statusChanged, this, &MainWindow::onStatusChanged);
    connect(m_processingWorker.get(), &ProcessingWorker::burstStarted, this, &MainWindow::onBurstStarted);
    connect(m_processingWorker.get(), &ProcessingWorker::burstFrameCaptured, this, &MainWindow::onBurstFrameCaptured);
    connect(m_processingWorker.get(), &ProcessingWorker::burstCompleted, this, &MainWindow::onBurstCompleted);
    connect(m_processingWorker.get(), &ProcessingWorker::errorOccurred, this, &MainWindow::onWorkerError);
}

void MainWindow::rebuildSource(InputSourceType type)
{
    if (m_source) {
        m_source->stop();
        m_source.reset();
    }

    if (type == InputSourceType::Camera) {
        auto source = std::make_unique<CameraFrameSource>();
        source->setBuffer(m_ringBuffer);
        source->setCameraIndex(m_cameraCombo->currentData().toInt());
        m_source = std::move(source);
    } else {
        auto source = std::make_unique<VideoFileFrameSource>();
        source->setBuffer(m_ringBuffer);
        source->setFilePath(m_videoPathEdit->text().trimmed());
        source->setLoop(m_loopCheck->isChecked());
        m_source = std::move(source);
    }

    attachSource(m_source.get());
    resetProcessing();
    updateControlsForMode();
    updateSourceLabel();
}

VideoFileFrameSource* MainWindow::videoSource() const
{
    return dynamic_cast<VideoFileFrameSource*>(m_source.get());
}

CameraFrameSource* MainWindow::cameraSource() const
{
    return dynamic_cast<CameraFrameSource*>(m_source.get());
}

void MainWindow::attachSource(FrameSourceBase* source)
{
    if (!source) {
        return;
    }

    connect(source, &FrameSourceBase::errorOccurred, this, &MainWindow::onSourceError);
    connect(source, &FrameSourceBase::videoStateChanged, this, &MainWindow::onVideoStateChanged);
}

void MainWindow::applySettingsToUi()
{
    const int sourceIndex = m_sourceCombo->findData(static_cast<int>(m_settings.inputSourceType));
    if (sourceIndex >= 0) {
        m_sourceCombo->setCurrentIndex(sourceIndex);
    }

    m_videoPathEdit->setText(m_settings.lastVideoPath);
    m_thresholdSpin->setValue(m_settings.motion.threshold);
    m_minAreaSpin->setValue(m_settings.motion.minArea);
    m_cooldownSpin->setValue(m_settings.motion.cooldownMs);
    m_burstCountSpin->setValue(std::clamp(m_settings.burst.burstCount, 1, kMaxGridCells));
    m_intervalSpin->setValue(m_settings.burst.capturesPerAction);
    m_outputDirEdit->setText(m_settings.outputDir);
    m_loopCheck->setChecked(m_settings.loopPlayback);
    m_peopleSpin->setValue(std::clamp(m_settings.peopleCount, 0, 10));
    m_burstGrid->setPeopleCount(m_peopleSpin->value());
}

void MainWindow::updateControlsForMode()
{
    const bool videoMode = m_sourceCombo->currentData().toInt() == static_cast<int>(InputSourceType::VideoFile);
    const bool running = m_source && m_source->isRunning();

    m_cameraCombo->setEnabled(!videoMode);
    m_refreshCameraButton->setEnabled(!videoMode);
    m_openVideoButton->setEnabled(videoMode);
    m_videoPathEdit->setEnabled(videoMode);
    m_playButton->setEnabled(videoMode && running);
    m_pauseButton->setEnabled(videoMode && running);
    m_nextFrameButton->setEnabled(videoMode && running);
    m_seekSlider->setEnabled(videoMode && running && m_seekSlider->maximum() > 0);
    m_loopCheck->setEnabled(videoMode);
    m_stopButton->setEnabled(running);
    m_manualTriggerButton->setEnabled(running);
}

void MainWindow::updateSourceLabel()
{
    const bool videoMode = m_sourceCombo->currentData().toInt() == static_cast<int>(InputSourceType::VideoFile);
    const QString typeText = videoMode ? QStringLiteral("视频文件") : QStringLiteral("摄像头");
    const QString nameText = m_source ? m_source->sourceName() : QString();

    const QString text = nameText.isEmpty()
        ? QStringLiteral("输入源：%1").arg(typeText)
        : QStringLiteral("输入源：%1 | %2").arg(typeText, nameText);

    m_inputTypeLabel->setText(text);
}

void MainWindow::updateVideoUi(const VideoState& state)
{
    const QString playText = state.playing ? QStringLiteral("播放中") : QStringLiteral("已暂停");
    m_videoInfoLabel->setText(QStringLiteral("帧 %1 / %2 | 时间 %3 / %4 | %5")
        .arg(state.currentFrame)
        .arg(state.totalFrames)
        .arg(ImageUtils::formatDurationMs(state.currentMs))
        .arg(ImageUtils::formatDurationMs(state.totalMs))
        .arg(playText));

    if (!m_userSeeking) {
        const qint64 sliderMax = std::max<qint64>(0, state.totalFrames - 1);
        m_seekSlider->setRange(0, static_cast<int>(sliderMax));
        m_seekSlider->setValue(static_cast<int>(std::min(state.currentFrame, sliderMax)));
    }

    m_seekSlider->setEnabled(videoSource() && videoSource()->isRunning() && m_seekSlider->maximum() > 0);
}

void MainWindow::setPreviewFrame(const FramePacket& packet)
{
    if (!packet.isValid()) {
        return;
    }

    const QSize newFrameSize(packet.frame.cols, packet.frame.rows);
    if (newFrameSize != m_currentFrameSize || m_currentPixelRoi.isEmpty()) {
        m_currentFrameSize = newFrameSize;
        applyNormalizedRoi();
    }

    m_previewWidget->setImage(ImageUtils::matToQImage(packet.frame));
    m_previewWidget->setRoi(m_currentPixelRoi);
}

void MainWindow::applyNormalizedRoi()
{
    if (!m_currentFrameSize.isValid()) {
        return;
    }

    QRect roi = ImageUtils::denormalizeRoi(m_settings.roiNormalized, m_currentFrameSize);
    if (roi.isEmpty()) {
        roi = QRect(
            m_currentFrameSize.width() / 4,
            m_currentFrameSize.height() / 4,
            m_currentFrameSize.width() / 2,
            m_currentFrameSize.height() / 2);
    }

    m_currentPixelRoi = roi;
    m_previewWidget->setRoi(m_currentPixelRoi);
    m_processingWorker->setRoi(m_currentPixelRoi);
}

void MainWindow::setNormalizedRoi(const QRect& pixelRoi)
{
    if (!m_currentFrameSize.isValid() || pixelRoi.isEmpty()) {
        return;
    }

    m_currentPixelRoi = pixelRoi;
    m_settings.roiNormalized = ImageUtils::normalizeRoi(pixelRoi, m_currentFrameSize);
    m_previewWidget->setRoi(m_currentPixelRoi);
    m_processingWorker->setRoi(m_currentPixelRoi);
    saveSettings();
}

void MainWindow::clearThumbnails()
{
    m_burstGrid->clear();
    m_currentBurstImages.clear();
    m_currentBurstFrameNumbers.clear();
    m_currentCaptureId.clear();
    m_selectedBurstIndex = -1;
    resetSelectedPreview();
    updateBurstSummary();
}

void MainWindow::setThumbnailImage(int index, const QImage& image, qint64 frameNumber)
{
    if (index < 0 || index >= kMaxGridCells || image.isNull()) {
        return;
    }

    m_burstGrid->setCellImage(index, image, frameNumber);
    updateBurstSummary();
}

void MainWindow::selectPreviewImage(int index)
{
    if (index < 0 || index >= m_currentBurstImages.size() || m_currentBurstImages.at(index).isNull()) {
        return;
    }

    const QSize targetSize = m_largePreviewLabel->size().isValid() ? m_largePreviewLabel->size() : QSize(320, 200);
    const QPixmap pixmap = QPixmap::fromImage(m_currentBurstImages.at(index)).scaled(
        targetSize,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation);

    m_largePreviewLabel->setText(QString());
    m_largePreviewLabel->setPixmap(pixmap);
    m_selectedBurstIndex = index;
    m_burstGrid->setSelectedIndex(index);

    const qint64 frameNumber = (index < m_currentBurstFrameNumbers.size()) ? m_currentBurstFrameNumbers.at(index) : -1;
    m_selectedInfoLabel->setText(frameNumber >= 0
        ? QStringLiteral("结果 %1 / 帧 %2").arg(index + 1).arg(frameNumber)
        : QStringLiteral("结果 %1").arg(index + 1));
}

void MainWindow::updateBurstSummary()
{
    const int imageCount = std::min(static_cast<int>(m_currentBurstImages.size()), kMaxGridCells);
    const QString peopleText = (m_peopleSpin && m_peopleSpin->value() > 0)
        ? QStringLiteral(" | 人数分组 %1").arg(m_peopleSpin->value())
        : QString();

    if (m_gridSummaryLabel) {
        m_gridSummaryLabel->setText(
            QStringLiteral("已显示 %1 / %2 | 4 列滚动网格 | Alt + 鼠标局部放大%3")
                .arg(imageCount)
                .arg(kMaxGridCells)
                .arg(peopleText));
    }
}

void MainWindow::resetSelectedPreview()
{
    if (m_largePreviewLabel) {
        m_largePreviewLabel->setPixmap(QPixmap());
        m_largePreviewLabel->setText(QStringLiteral("点击左侧关键帧查看\nAlt + 鼠标可在格子内局部放大"));
    }
    if (m_selectedInfoLabel) {
        m_selectedInfoLabel->setText(QStringLiteral("点击左侧关键帧查看"));
    }
}

void MainWindow::saveSettings()
{
    m_settings = currentSettings();
    m_settingsManager.save(m_settings);
}

void MainWindow::resetProcessing()
{
    m_processingWorker->resetAnalysis();
    m_lastPreviewSequence = 0;
}

AppSettings MainWindow::currentSettings() const
{
    AppSettings settings = m_settings;
    settings.inputSourceType = static_cast<InputSourceType>(m_sourceCombo->currentData().toInt());
    settings.cameraIndex = m_cameraCombo->currentData().isValid() ? m_cameraCombo->currentData().toInt() : -1;
    settings.lastVideoPath = m_videoPathEdit->text().trimmed();
    settings.motion.threshold = m_thresholdSpin->value();
    settings.motion.minArea = m_minAreaSpin->value();
    settings.motion.cooldownMs = m_cooldownSpin->value();
    settings.burst.burstCount = m_burstCountSpin->value();
    settings.burst.capturesPerAction = m_intervalSpin->value();
    settings.loopPlayback = m_loopCheck->isChecked();
    settings.outputDir = m_outputDirEdit->text().trimmed();
    settings.peopleCount = m_peopleSpin->value();
    if (settings.outputDir.isEmpty()) {
        settings.outputDir = QDir(qApp->applicationDirPath()).filePath(QStringLiteral("captures"));
    }
    return settings;
}

void MainWindow::onSourceModeChanged(int)
{
    rebuildSource(static_cast<InputSourceType>(m_sourceCombo->currentData().toInt()));
    saveSettings();
}

void MainWindow::onOpenVideo()
{
    const QString startDir = QFileInfo(m_videoPathEdit->text()).absolutePath();
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择视频文件"),
        startDir.isEmpty() ? QDir::currentPath() : startDir,
        QStringLiteral("Videos (*.mp4 *.avi *.mov *.MOV *.mkv);;All Files (*.*)"));

    if (path.isEmpty()) {
        return;
    }

    const int videoModeIndex = m_sourceCombo->findData(static_cast<int>(InputSourceType::VideoFile));
    if (videoModeIndex >= 0 && m_sourceCombo->currentIndex() != videoModeIndex) {
        m_sourceCombo->setCurrentIndex(videoModeIndex);
    }

    m_videoPathEdit->setText(QDir::toNativeSeparators(path));
    if (auto* source = videoSource()) {
        source->setFilePath(m_videoPathEdit->text());
    }

    updateSourceLabel();
    saveSettings();
}

void MainWindow::onStartPreview()
{
    if (!m_source) {
        rebuildSource(static_cast<InputSourceType>(m_sourceCombo->currentData().toInt()));
    }

    if (m_source && m_source->isRunning()) {
        m_source->stop();
    }

    if (auto* source = cameraSource()) {
        source->setCameraIndex(m_cameraCombo->currentData().toInt());
    }

    if (auto* source = videoSource()) {
        source->setFilePath(m_videoPathEdit->text().trimmed());
        source->setLoop(m_loopCheck->isChecked());
    }

    saveSettings();
    resetProcessing();

    if (!m_source || !m_source->start()) {
        updateControlsForMode();
        return;
    }

    updateControlsForMode();
    updateSourceLabel();
    statusBar()->showMessage(QStringLiteral("预览已启动"), 3000);
}

void MainWindow::onStopPreview()
{
    if (m_source) {
        m_source->stop();
    }

    m_ringBuffer->clear();
    resetProcessing();
    updateControlsForMode();
    updateVideoUi({});
    statusBar()->showMessage(QStringLiteral("预览已停止"), 3000);
}

void MainWindow::onPlay()
{
    if (auto* source = videoSource()) {
        source->resume();
        updateControlsForMode();
    }
}

void MainWindow::onPause()
{
    if (auto* source = videoSource()) {
        source->pause();
        updateControlsForMode();
    }
}

void MainWindow::onNextFrame()
{
    if (auto* source = videoSource()) {
        m_processingWorker->resetAnalysis();
        source->nextFrame();
        updateControlsForMode();
    }
}

void MainWindow::onSaveBurst()
{
    if (m_currentBurstImages.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("当前没有可保存的抓拍结果。"));
        return;
    }

    const QString basePath = m_outputDirEdit->text().trimmed().isEmpty()
        ? QDir(qApp->applicationDirPath()).filePath(QStringLiteral("captures"))
        : m_outputDirEdit->text().trimmed();

    QDir baseDir(basePath);
    if (!baseDir.exists() && !baseDir.mkpath(QStringLiteral("."))) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("输出目录创建失败：%1").arg(basePath));
        return;
    }

    const QString captureId = m_currentCaptureId.isEmpty()
        ? QStringLiteral("capture_%1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz")))
        : m_currentCaptureId;

    if (!baseDir.mkpath(captureId)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("抓拍目录创建失败：%1").arg(captureId));
        return;
    }

    QDir captureDir(baseDir.filePath(captureId));
    for (int index = 0; index < m_currentBurstImages.size(); ++index) {
        if (m_currentBurstImages.at(index).isNull()) {
            continue;
        }

        const qint64 frameNumber = (index < m_currentBurstFrameNumbers.size()) ? m_currentBurstFrameNumbers.at(index) : -1;
        const QString fileName = frameNumber >= 0
            ? QStringLiteral("%1_f%2_%3.png")
                .arg(index + 1, 2, 10, QChar('0'))
                .arg(frameNumber, 6, 10, QChar('0'))
                .arg(captureId)
            : QStringLiteral("%1_%2.png")
                .arg(index + 1, 2, 10, QChar('0'))
                .arg(captureId);

        if (!m_currentBurstImages.at(index).save(captureDir.filePath(fileName), "PNG")) {
            QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("图片写入失败：%1").arg(fileName));
            return;
        }
    }

    statusBar()->showMessage(QStringLiteral("抓拍结果已保存到 %1").arg(QDir::toNativeSeparators(captureDir.path())), 5000);
}

void MainWindow::onClearResults()
{
    clearThumbnails();
    resetProcessing();
    statusBar()->showMessage(QStringLiteral("已清空本轮抓拍结果"), 3000);
}

void MainWindow::onGridCellActivated(int index)
{
    selectPreviewImage(index);
}

void MainWindow::onRefreshPreview()
{
    FramePacket packet;
    if (!m_ringBuffer->latest(packet) || packet.sequence == m_lastPreviewSequence) {
        return;
    }

    m_lastPreviewSequence = packet.sequence;
    setPreviewFrame(packet);
}

void MainWindow::onRoiChanged(const QRect& roiPixels)
{
    setNormalizedRoi(roiPixels);
}

void MainWindow::onVideoStateChanged(const VideoState& state)
{
    if (!videoSource()) {
        return;
    }

    updateVideoUi(state);
    updateControlsForMode();
}

void MainWindow::onSourceError(const QString& message)
{
    statusBar()->showMessage(message, 5000);
    QMessageBox::warning(this, QStringLiteral("输入源错误"), message);
}

void MainWindow::onWorkerError(const QString& message)
{
    statusBar()->showMessage(message, 5000);
    QMessageBox::warning(this, QStringLiteral("处理错误"), message);
}

void MainWindow::onMotionStateChanged(bool motionDetected, double area)
{
    if (motionDetected) {
        m_motionLabel->setText(QStringLiteral("运动：检测到运动（面积 %1）").arg(static_cast<int>(area)));
    } else {
        m_motionLabel->setText(QStringLiteral("运动：无"));
    }
}

void MainWindow::onStatusChanged(const QString& status)
{
    const QString text = QStringLiteral("状态：%1").arg(status);
    m_statusLabel->setText(text);
    m_toolbarStateLabel->setText(text);
}

void MainWindow::onBurstStarted(const QString& reason)
{
    const QString reasonText = (reason == QStringLiteral("manual")) ? QStringLiteral("手动") : QStringLiteral("自动");
    statusBar()->showMessage(QStringLiteral("%1动作已触发，等待关键帧").arg(reasonText), 3000);
}

void MainWindow::onBurstFrameCaptured(int index, const QImage& image)
{
    if (index < 0 || index >= kMaxGridCells) {
        return;
    }

    if (m_currentBurstImages.size() <= index) {
        m_currentBurstImages.resize(index + 1);
    }
    if (m_currentBurstFrameNumbers.size() <= index) {
        m_currentBurstFrameNumbers.resize(index + 1);
    }

    m_currentBurstImages[index] = image;
    if (m_currentBurstFrameNumbers.at(index) == 0) {
        m_currentBurstFrameNumbers[index] = -1;
    }

    setThumbnailImage(index, image);

    if (m_selectedBurstIndex < 0) {
        selectPreviewImage(index);
    }
}

void MainWindow::onBurstCompleted(const BurstResult& result)
{
    clearThumbnails();

    const int visibleCount = std::min(static_cast<int>(result.images.size()), kMaxGridCells);
    m_currentBurstImages = result.images.mid(0, visibleCount);
    m_currentBurstFrameNumbers = result.frameNumbers.mid(0, visibleCount);
    m_currentCaptureId = result.captureId;

    m_burstGrid->setResults(m_currentBurstImages, m_currentBurstFrameNumbers);
    updateBurstSummary();

    if (!m_currentBurstImages.isEmpty()) {
        selectPreviewImage(0);
    }

    QStringList frameTexts;
    for (qint64 frameNumber : m_currentBurstFrameNumbers) {
        if (frameNumber >= 0) {
            frameTexts.push_back(QString::number(frameNumber));
        }
    }

    const QString suffix = frameTexts.isEmpty()
        ? QString()
        : QStringLiteral(" | 帧: %1").arg(frameTexts.join(QStringLiteral(", ")));
    statusBar()->showMessage(QStringLiteral("已更新关键帧结果，共 %1 张%2").arg(m_currentBurstImages.size()).arg(suffix), 5000);
}

void MainWindow::onBrowseOutputDir()
{
    const QString path = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择输出目录"),
        m_outputDirEdit->text().trimmed().isEmpty() ? QDir::currentPath() : m_outputDirEdit->text().trimmed());

    if (path.isEmpty()) {
        return;
    }

    m_outputDirEdit->setText(QDir::toNativeSeparators(path));
    saveSettings();
}

void MainWindow::onSeekSliderPressed()
{
    m_userSeeking = true;
}

void MainWindow::onSeekSliderReleased()
{
    m_userSeeking = false;

    if (auto* source = videoSource()) {
        m_processingWorker->resetAnalysis();
        source->seekFrame(m_seekSlider->value());
    }
}

void MainWindow::onParametersChanged()
{
    m_processingWorker->setMotionParameters(MotionParameters {
        m_thresholdSpin->value(),
        m_minAreaSpin->value(),
        m_cooldownSpin->value()
    });

    m_processingWorker->setBurstParameters(BurstParameters {
        m_burstCountSpin->value(),
        m_intervalSpin->value()
    });

    m_burstGrid->setPeopleCount(m_peopleSpin->value());
    updateBurstSummary();

    if (auto* source = cameraSource()) {
        source->setCameraIndex(m_cameraCombo->currentData().toInt());
    }

    if (auto* source = videoSource()) {
        source->setFilePath(m_videoPathEdit->text().trimmed());
        source->setLoop(m_loopCheck->isChecked());
    }

    saveSettings();
    updateControlsForMode();
    updateSourceLabel();
}

void MainWindow::refreshCameras()
{
    const int previousCamera = m_cameraCombo->currentData().isValid() ? m_cameraCombo->currentData().toInt() : m_settings.cameraIndex;
    m_cameraCombo->clear();

    const auto cameras = CameraFrameSource::enumerateCameras(5);
    if (cameras.isEmpty()) {
        m_cameraCombo->addItem(QStringLiteral("未发现可用摄像头"), -1);
    } else {
        for (const auto& camera : cameras) {
            m_cameraCombo->addItem(camera.second, camera.first);
        }
    }

    int targetIndex = m_cameraCombo->findData(m_settings.cameraIndex);
    if (targetIndex < 0) {
        targetIndex = m_cameraCombo->findData(previousCamera);
    }
    if (targetIndex < 0) {
        targetIndex = 0;
    }
    m_cameraCombo->setCurrentIndex(targetIndex);
}
