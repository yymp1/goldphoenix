#include "CameraFrameSource.h"

#include <QDateTime>
#include <QDebug>

#include <chrono>

CameraFrameSource::CameraFrameSource(QObject* parent)
    : FrameSourceBase(parent)
{
}

CameraFrameSource::~CameraFrameSource()
{
    stop();
}

void CameraFrameSource::setCameraIndex(int index)
{
    m_cameraIndex = index;
}

int CameraFrameSource::cameraIndex() const
{
    return m_cameraIndex;
}

QVector<QPair<int, QString>> CameraFrameSource::enumerateCameras(int maxIndex)
{
    QVector<QPair<int, QString>> cameras;

    for (int index = 0; index <= maxIndex; ++index) {
        cv::VideoCapture capture;
        if (openCapture(capture, index)) {
            cameras.append({index, QStringLiteral("摄像头 %1").arg(index)});
            capture.release();
        }
    }

    return cameras;
}

InputSourceType CameraFrameSource::sourceType() const
{
    return InputSourceType::Camera;
}

bool CameraFrameSource::openCapture(cv::VideoCapture& capture, int index)
{
#ifdef _WIN32
    if (capture.open(index, cv::CAP_DSHOW)) {
        return true;
    }
#endif
    return capture.open(index, cv::CAP_ANY);
}

bool CameraFrameSource::start()
{
    if (m_running.load()) {
        return true;
    }

    if (m_cameraIndex < 0) {
        emit errorOccurred(QStringLiteral("没有可用摄像头可打开。"));
        return false;
    }

    clearBuffer();

    if (!openCapture(m_capture, m_cameraIndex)) {
        emit errorOccurred(QStringLiteral("摄像头打开失败：索引 %1").arg(m_cameraIndex));
        return false;
    }

    m_capture.set(cv::CAP_PROP_BUFFERSIZE, 1);
    m_frameCounter = 0;
    m_stopRequested = false;
    m_paused = false;
    m_running = true;
    m_thread = std::thread(&CameraFrameSource::captureLoop, this);
    qInfo() << "Camera source started:" << m_cameraIndex;
    return true;
}

void CameraFrameSource::stop()
{
    if (!m_running.load()) {
        return;
    }

    m_stopRequested = true;
    m_controlCv.notify_all();

    if (m_thread.joinable()) {
        m_thread.join();
    }

    if (m_capture.isOpened()) {
        m_capture.release();
    }

    m_running = false;
    m_paused = false;
    qInfo() << "Camera source stopped";
}

void CameraFrameSource::pause()
{
    m_paused = true;
}

void CameraFrameSource::resume()
{
    m_paused = false;
    m_controlCv.notify_all();
}

bool CameraFrameSource::isRunning() const
{
    return m_running.load();
}

bool CameraFrameSource::isPaused() const
{
    return m_paused.load();
}

QString CameraFrameSource::sourceName() const
{
    return QStringLiteral("摄像头 %1").arg(m_cameraIndex);
}

void CameraFrameSource::captureLoop()
{
    using namespace std::chrono_literals;

    while (!m_stopRequested.load()) {
        {
            std::unique_lock<std::mutex> lock(m_controlMutex);
            m_controlCv.wait(lock, [&]() {
                return m_stopRequested.load() || !m_paused.load();
            });
        }

        if (m_stopRequested.load()) {
            break;
        }

        cv::Mat frame;
        if (!m_capture.read(frame) || frame.empty()) {
            emit errorOccurred(QStringLiteral("摄像头读取帧失败。"));
            std::this_thread::sleep_for(30ms);
            continue;
        }

        pushFrame(frame, m_frameCounter++, QDateTime::currentMSecsSinceEpoch());
    }
}
