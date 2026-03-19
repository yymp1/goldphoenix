#pragma once

#include "AppTypes.h"
#include "BurstGridWidget.h"
#include "CameraFrameSource.h"
#include "PreviewWidget.h"
#include "ProcessingWorker.h"
#include "SettingsManager.h"
#include "VideoFileFrameSource.h"

#include <QMainWindow>

#include <memory>

class QCloseEvent;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QSlider;
class QSpinBox;
class QTimer;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onSourceModeChanged(int index);
    void onOpenVideo();
    void onStartPreview();
    void onStopPreview();
    void onPlay();
    void onPause();
    void onNextFrame();
    void onSaveBurst();
    void onClearResults();
    void onGridCellActivated(int index);
    void onRefreshPreview();
    void onRoiChanged(const QRect& roiPixels);
    void onVideoStateChanged(const VideoState& state);
    void onSourceError(const QString& message);
    void onWorkerError(const QString& message);
    void onMotionStateChanged(bool motionDetected, double area);
    void onStatusChanged(const QString& status);
    void onBurstStarted(const QString& reason);
    void onBurstFrameCaptured(int index, const QImage& image);
    void onBurstCompleted(const BurstResult& result);
    void onBrowseOutputDir();
    void onSeekSliderPressed();
    void onSeekSliderReleased();
    void onParametersChanged();
    void refreshCameras();

private:
    void setupUi();
    void connectWorker();
    void rebuildSource(InputSourceType type);
    VideoFileFrameSource* videoSource() const;
    CameraFrameSource* cameraSource() const;
    void attachSource(FrameSourceBase* source);
    void applySettingsToUi();
    void updateControlsForMode();
    void updateSourceLabel();
    void updateVideoUi(const VideoState& state);
    void setPreviewFrame(const FramePacket& packet);
    void applyNormalizedRoi();
    void setNormalizedRoi(const QRect& pixelRoi);
    void clearThumbnails();
    void setThumbnailImage(int index, const QImage& image, qint64 frameNumber = -1);
    void selectPreviewImage(int index);
    void updateBurstSummary();
    void resetSelectedPreview();
    void saveSettings();
    void resetProcessing();
    AppSettings currentSettings() const;

    SettingsManager m_settingsManager;
    AppSettings m_settings;
    std::shared_ptr<RingBuffer> m_ringBuffer;
    std::unique_ptr<FrameSourceBase> m_source;
    std::unique_ptr<ProcessingWorker> m_processingWorker;
    QTimer* m_previewTimer = nullptr;
    qint64 m_lastPreviewSequence = 0;
    QSize m_currentFrameSize;
    QRect m_currentPixelRoi;
    bool m_userSeeking = false;
    QVector<QImage> m_currentBurstImages;
    QVector<qint64> m_currentBurstFrameNumbers;
    QString m_currentCaptureId;
    int m_selectedBurstIndex = -1;

    QComboBox* m_sourceCombo = nullptr;
    QComboBox* m_cameraCombo = nullptr;
    QPushButton* m_refreshCameraButton = nullptr;
    QPushButton* m_openVideoButton = nullptr;
    QPushButton* m_startButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QPushButton* m_playButton = nullptr;
    QPushButton* m_pauseButton = nullptr;
    QPushButton* m_nextFrameButton = nullptr;
    QPushButton* m_manualTriggerButton = nullptr;
    QPushButton* m_saveBurstButton = nullptr;
    QPushButton* m_clearButton = nullptr;
    QCheckBox* m_loopCheck = nullptr;
    QLineEdit* m_videoPathEdit = nullptr;
    QSlider* m_seekSlider = nullptr;
    QLabel* m_videoInfoLabel = nullptr;
    PreviewWidget* m_previewWidget = nullptr;
    QLabel* m_toolbarStateLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_motionLabel = nullptr;
    QLabel* m_inputTypeLabel = nullptr;
    QLabel* m_largePreviewLabel = nullptr;
    QLabel* m_selectedInfoLabel = nullptr;
    QLabel* m_gridSummaryLabel = nullptr;
    BurstGridWidget* m_burstGrid = nullptr;
    QScrollArea* m_gridScrollArea = nullptr;
    QSpinBox* m_thresholdSpin = nullptr;
    QSpinBox* m_minAreaSpin = nullptr;
    QSpinBox* m_cooldownSpin = nullptr;
    QSpinBox* m_burstCountSpin = nullptr;
    QSpinBox* m_intervalSpin = nullptr;
    QSpinBox* m_peopleSpin = nullptr;
    QLineEdit* m_outputDirEdit = nullptr;
    QPushButton* m_outputDirButton = nullptr;
};
