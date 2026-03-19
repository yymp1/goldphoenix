#include "SettingsManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include <algorithm>
#include <cmath>

namespace
{
constexpr int kConfigVersion = 2;

bool nearlyEqual(double left, double right, double epsilon = 0.0005)
{
    return std::abs(left - right) <= epsilon;
}

bool isSameRoi(const QRectF& left, const QRectF& right)
{
    return nearlyEqual(left.x(), right.x())
        && nearlyEqual(left.y(), right.y())
        && nearlyEqual(left.width(), right.width())
        && nearlyEqual(left.height(), right.height());
}
}

SettingsManager::SettingsManager()
{
    m_configPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config.ini"));
}

AppSettings SettingsManager::load() const
{
    AppSettings settings;
    const AppSettings fittedDefaults;
    settings.outputDir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("captures"));

    QSettings store(m_configPath, QSettings::IniFormat);
    const int configVersion = store.value(QStringLiteral("general/configVersion"), 0).toInt();
    const QString mode = store.value(QStringLiteral("general/sourceMode"), QStringLiteral("video")).toString();
    settings.inputSourceType = (mode == QStringLiteral("camera")) ? InputSourceType::Camera : InputSourceType::VideoFile;
    settings.cameraIndex = store.value(QStringLiteral("general/cameraIndex"), 0).toInt();
    settings.lastVideoPath = store.value(QStringLiteral("general/lastVideoPath")).toString();
    settings.loopPlayback = store.value(QStringLiteral("general/loopPlayback"), false).toBool();
    settings.ringBufferSize = store.value(QStringLiteral("general/ringBufferSize"), 8).toInt();
    settings.outputDir = store.value(QStringLiteral("general/outputDir"), settings.outputDir).toString();

    const double roiX = store.value(QStringLiteral("roi/x"), settings.roiNormalized.x()).toDouble();
    const double roiY = store.value(QStringLiteral("roi/y"), settings.roiNormalized.y()).toDouble();
    const double roiW = store.value(QStringLiteral("roi/w"), settings.roiNormalized.width()).toDouble();
    const double roiH = store.value(QStringLiteral("roi/h"), settings.roiNormalized.height()).toDouble();
    settings.roiNormalized = QRectF(roiX, roiY, roiW, roiH);

    settings.motion.threshold = store.value(QStringLiteral("motion/threshold"), settings.motion.threshold).toInt();
    settings.motion.minArea = store.value(QStringLiteral("motion/minArea"), settings.motion.minArea).toInt();
    settings.motion.cooldownMs = store.value(QStringLiteral("motion/cooldownMs"), settings.motion.cooldownMs).toInt();

    settings.burst.burstCount = store.value(QStringLiteral("burst/count"), settings.burst.burstCount).toInt();
    settings.burst.capturesPerAction = store.value(
        QStringLiteral("burst/capturesPerAction"),
        settings.burst.capturesPerAction).toInt();
    settings.peopleCount = store.value(QStringLiteral("ui/peopleCount"), settings.peopleCount).toInt();

    if (settings.lastVideoPath.isEmpty()) {
        const QStringList candidates = {
            QDir::current().filePath(QStringLiteral("test3.MOV")),
            QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("test3.MOV"))
        };

        for (const QString& candidate : candidates) {
            if (QFileInfo::exists(candidate)) {
                settings.lastVideoPath = QDir::toNativeSeparators(candidate);
                break;
            }
        }
    }

    if (settings.outputDir.isEmpty()) {
        settings.outputDir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("captures"));
    }

    settings.ringBufferSize = std::max(1, settings.ringBufferSize);
    settings.burst.burstCount = std::clamp(settings.burst.burstCount, 1, 20);
    settings.burst.capturesPerAction = std::clamp(settings.burst.capturesPerAction, 1, 2);
    settings.motion.threshold = std::max(1, settings.motion.threshold);
    settings.motion.minArea = std::max(1, settings.motion.minArea);
    settings.motion.cooldownMs = std::max(0, settings.motion.cooldownMs);
    settings.peopleCount = std::clamp(settings.peopleCount, 0, 10);

    if (configVersion < kConfigVersion) {
        const QRectF legacyDefaultRoi(0.25, 0.25, 0.5, 0.5);
        const MotionParameters legacyMotion { 25, 1500, 800 };

        if (isSameRoi(settings.roiNormalized, legacyDefaultRoi)) {
            settings.roiNormalized = fittedDefaults.roiNormalized;
        }

        if (settings.motion.threshold == legacyMotion.threshold
            && settings.motion.minArea == legacyMotion.minArea
            && settings.motion.cooldownMs == legacyMotion.cooldownMs) {
            settings.motion = fittedDefaults.motion;
        }
    }

    return settings;
}

void SettingsManager::save(const AppSettings& settings) const
{
    QSettings store(m_configPath, QSettings::IniFormat);
    store.setValue(QStringLiteral("general/sourceMode"), settings.inputSourceType == InputSourceType::Camera ? QStringLiteral("camera") : QStringLiteral("video"));
    store.setValue(QStringLiteral("general/cameraIndex"), settings.cameraIndex);
    store.setValue(QStringLiteral("general/lastVideoPath"), settings.lastVideoPath);
    store.setValue(QStringLiteral("general/loopPlayback"), settings.loopPlayback);
    store.setValue(QStringLiteral("general/ringBufferSize"), settings.ringBufferSize);
    store.setValue(QStringLiteral("general/outputDir"), settings.outputDir);
    store.setValue(QStringLiteral("general/configVersion"), kConfigVersion);

    store.setValue(QStringLiteral("roi/x"), settings.roiNormalized.x());
    store.setValue(QStringLiteral("roi/y"), settings.roiNormalized.y());
    store.setValue(QStringLiteral("roi/w"), settings.roiNormalized.width());
    store.setValue(QStringLiteral("roi/h"), settings.roiNormalized.height());

    store.setValue(QStringLiteral("motion/threshold"), settings.motion.threshold);
    store.setValue(QStringLiteral("motion/minArea"), settings.motion.minArea);
    store.setValue(QStringLiteral("motion/cooldownMs"), settings.motion.cooldownMs);

    store.setValue(QStringLiteral("burst/count"), settings.burst.burstCount);
    store.setValue(QStringLiteral("burst/capturesPerAction"), settings.burst.capturesPerAction);
    store.setValue(QStringLiteral("ui/peopleCount"), std::clamp(settings.peopleCount, 0, 10));
    store.remove(QStringLiteral("burst/intervalMs"));
    store.sync();
}

QString SettingsManager::configPath() const
{
    return m_configPath;
}
