#include "AppTypes.h"
#include "BurstCaptureManager.h"
#include "ImageUtils.h"
#include "MotionDetector.h"

#include <QCoreApplication>

#include <opencv2/videoio.hpp>

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    const fs::path workspace = (argc >= 2) ? fs::path(argv[1]) : fs::path("D:/poke");
    const fs::path videoPath = workspace / "test3.MOV";

    cv::VideoCapture capture(videoPath.string(), cv::CAP_ANY);
    if (!capture.isOpened()) {
        std::cerr << "Failed to open video: " << videoPath.string() << "\n";
        return 1;
    }

    const cv::Size frameSize(
        static_cast<int>(capture.get(cv::CAP_PROP_FRAME_WIDTH)),
        static_cast<int>(capture.get(cv::CAP_PROP_FRAME_HEIGHT)));

    MotionDetector detector;
    detector.setParameters(MotionParameters { 16, 3040, 550 });

    BurstCaptureManager manager;
    manager.setParameters(BurstParameters { 12, 1 });
    const QRect roi = ImageUtils::denormalizeRoi(
        QRectF(0.1698, 0.6574, 0.6198, 0.3426),
        QSize(frameSize.width, frameSize.height));
    manager.setRoi(roi);

    cv::Mat frame;
    qint64 frameIndex = 0;
    int actionIndex = 0;
    while (capture.read(frame)) {
        FramePacket packet;
        packet.frame = frame;
        packet.frameNumber = frameIndex;
        packet.timestampMs = static_cast<qint64>(capture.get(cv::CAP_PROP_POS_MSEC));
        packet.sequence = frameIndex + 1;

        const MotionResult motion = detector.detect(packet.frame, roi, packet.timestampMs);

        if (!manager.isActionActive() && motion.shouldTrigger) {
            std::cout << "Trigger at frame " << frameIndex
                      << " area=" << motion.largestArea
                      << " box=(" << motion.motionBox.x() << "," << motion.motionBox.y()
                      << "," << motion.motionBox.width() << "," << motion.motionBox.height() << ")\n";
            manager.startAction(packet, motion, QStringLiteral("motion"));
        } else if (manager.isActionActive()) {
            const BurstUpdate update = manager.processAction(packet, motion);
            if (update.sessionChanged) {
                ++actionIndex;
                std::cout << "Action " << actionIndex << ":";
                for (qint64 capturedFrame : update.capturedFrameNumbers) {
                    std::cout << ' ' << capturedFrame;
                }
                std::cout << "\n";
            }
        }

        ++frameIndex;
    }

    return 0;
}
