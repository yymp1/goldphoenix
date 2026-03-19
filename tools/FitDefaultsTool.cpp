#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;

struct SampleImage
{
    std::string name;
    cv::Mat gray320;
};

struct VideoFrame
{
    int index = -1;
    double timestampMs = 0.0;
    cv::Mat gray320;
    cv::Mat gray960;
};

struct DetectionResult
{
    std::vector<int> triggerFrames;
    double maxArea = 0.0;
};

struct CandidateConfig
{
    cv::Rect roi960;
    int threshold = 25;
    int minArea960 = 120;
    int cooldownMs = 800;
    double score = std::numeric_limits<double>::max();
    std::vector<int> triggerFrames;
};

static std::vector<fs::path> sortedJpgFiles(const fs::path& dir)
{
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });

        if (ext == ".jpg") {
            files.push_back(entry.path());
        }
    }

    std::sort(files.begin(), files.end(), [](const fs::path& left, const fs::path& right) {
        const std::string leftStem = left.stem().string();
        const std::string rightStem = right.stem().string();

        const bool leftNumeric = !leftStem.empty()
            && std::all_of(leftStem.begin(), leftStem.end(), [](unsigned char ch) { return std::isdigit(ch); });
        const bool rightNumeric = !rightStem.empty()
            && std::all_of(rightStem.begin(), rightStem.end(), [](unsigned char ch) { return std::isdigit(ch); });

        if (leftNumeric && rightNumeric) {
            return std::stoi(leftStem) < std::stoi(rightStem);
        }

        return left.filename().string() < right.filename().string();
    });
    return files;
}

static cv::Mat toGrayResized(const cv::Mat& src, const cv::Size& size)
{
    cv::Mat resized;
    cv::resize(src, resized, size, 0.0, 0.0, cv::INTER_AREA);

    if (resized.channels() == 3) {
        cv::cvtColor(resized, resized, cv::COLOR_BGR2GRAY);
    } else if (resized.channels() == 4) {
        cv::cvtColor(resized, resized, cv::COLOR_BGRA2GRAY);
    }

    return resized;
}

static std::vector<SampleImage> loadSamples(const fs::path& sampleDir)
{
    std::vector<SampleImage> samples;
    for (const fs::path& path : sortedJpgFiles(sampleDir)) {
        cv::Mat image = cv::imread(path.string(), cv::IMREAD_COLOR);
        if (image.empty()) {
            continue;
        }

        samples.push_back({path.filename().string(), toGrayResized(image, cv::Size(320, 180))});
    }
    return samples;
}

static std::vector<VideoFrame> decodeVideo(const fs::path& videoPath, double& fpsOut, cv::Size& fullSizeOut)
{
    cv::VideoCapture capture(videoPath.string(), cv::CAP_ANY);
    if (!capture.isOpened()) {
        throw std::runtime_error("Failed to open video: " + videoPath.string());
    }

    fpsOut = capture.get(cv::CAP_PROP_FPS);
    if (!(fpsOut > 1.0)) {
        fpsOut = 60.0;
    }

    fullSizeOut = cv::Size(
        static_cast<int>(capture.get(cv::CAP_PROP_FRAME_WIDTH)),
        static_cast<int>(capture.get(cv::CAP_PROP_FRAME_HEIGHT)));

    std::vector<VideoFrame> frames;
    cv::Mat frame;
    int index = 0;
    while (capture.read(frame)) {
        VideoFrame item;
        item.index = index;
        item.timestampMs = static_cast<double>(index) * 1000.0 / fpsOut;
        item.gray320 = toGrayResized(frame, cv::Size(320, 180));
        item.gray960 = toGrayResized(frame, cv::Size(960, 540));
        frames.push_back(std::move(item));
        ++index;
    }

    return frames;
}

static std::vector<std::vector<double>> buildCostMatrix(
    const std::vector<SampleImage>& samples,
    const std::vector<VideoFrame>& frames)
{
    std::vector<std::vector<double>> cost(samples.size(), std::vector<double>(frames.size(), 0.0));
    for (size_t sampleIndex = 0; sampleIndex < samples.size(); ++sampleIndex) {
        for (size_t frameIndex = 0; frameIndex < frames.size(); ++frameIndex) {
            cv::Mat diff;
            cv::absdiff(samples[sampleIndex].gray320, frames[frameIndex].gray320, diff);
            cost[sampleIndex][frameIndex] = cv::mean(diff)[0];
        }
    }
    return cost;
}

static std::vector<int> pickOrderedMatches(const std::vector<std::vector<double>>& cost)
{
    const int sampleCount = static_cast<int>(cost.size());
    const int frameCount = sampleCount > 0 ? static_cast<int>(cost.front().size()) : 0;
    const int minGapFrames = 18;

    std::vector<std::vector<double>> dp(sampleCount, std::vector<double>(frameCount, std::numeric_limits<double>::max()));
    std::vector<std::vector<int>> prev(sampleCount, std::vector<int>(frameCount, -1));

    for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
        dp[0][frameIndex] = cost[0][frameIndex];
    }

    for (int sampleIndex = 1; sampleIndex < sampleCount; ++sampleIndex) {
        double bestPrefix = std::numeric_limits<double>::max();
        int bestPrefixIndex = -1;

        for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
            const int candidatePrev = frameIndex - minGapFrames;
            if (candidatePrev >= 0 && dp[sampleIndex - 1][candidatePrev] < bestPrefix) {
                bestPrefix = dp[sampleIndex - 1][candidatePrev];
                bestPrefixIndex = candidatePrev;
            }

            if (bestPrefixIndex >= 0) {
                dp[sampleIndex][frameIndex] = bestPrefix + cost[sampleIndex][frameIndex];
                prev[sampleIndex][frameIndex] = bestPrefixIndex;
            }
        }
    }

    int bestFrame = -1;
    double bestCost = std::numeric_limits<double>::max();
    for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
        if (dp[sampleCount - 1][frameIndex] < bestCost) {
            bestCost = dp[sampleCount - 1][frameIndex];
            bestFrame = frameIndex;
        }
    }

    std::vector<int> matches(sampleCount, -1);
    int current = bestFrame;
    for (int sampleIndex = sampleCount - 1; sampleIndex >= 0; --sampleIndex) {
        matches[sampleIndex] = current;
        if (sampleIndex > 0) {
            current = prev[sampleIndex][current];
        }
    }

    return matches;
}

static cv::Rect clampRect(const cv::Rect& rect, const cv::Size& size)
{
    return rect & cv::Rect(0, 0, size.width, size.height);
}

static cv::Rect expandRect(const cv::Rect& rect, const cv::Size& bounds, double xRatio, double yRatio)
{
    const int dx = static_cast<int>(rect.width * xRatio);
    const int dy = static_cast<int>(rect.height * yRatio);
    return clampRect(cv::Rect(rect.x - dx, rect.y - dy, rect.width + dx * 2, rect.height + dy * 2), bounds);
}

static cv::Rect unionRect(const std::vector<cv::Rect>& rects)
{
    if (rects.empty()) {
        return {};
    }

    cv::Rect merged = rects.front();
    for (size_t index = 1; index < rects.size(); ++index) {
        merged |= rects[index];
    }
    return merged;
}

static cv::Rect buildBaseRoi960(const std::vector<VideoFrame>& frames, const std::vector<int>& matchedFrames)
{
    std::vector<cv::Rect> boxes;
    const cv::Size size(960, 540);
    const int minY = static_cast<int>(size.height * 0.52);

    for (int match : matchedFrames) {
        const int baselineIndex = std::max(0, match - 4);
        if (baselineIndex >= static_cast<int>(frames.size()) || match >= static_cast<int>(frames.size())) {
            continue;
        }

        cv::Mat diff;
        cv::absdiff(frames[match].gray960, frames[baselineIndex].gray960, diff);
        cv::rectangle(diff, cv::Rect(0, 0, size.width, minY), cv::Scalar(0), cv::FILLED);
        cv::GaussianBlur(diff, diff, cv::Size(5, 5), 0);
        cv::threshold(diff, diff, 18, 255, cv::THRESH_BINARY);
        cv::dilate(diff, diff, cv::Mat(), cv::Point(-1, -1), 2);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(diff, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        for (const auto& contour : contours) {
            const double area = cv::contourArea(contour);
            if (area < 140.0) {
                continue;
            }

            const cv::Rect box = cv::boundingRect(contour);
            if (box.y < minY) {
                continue;
            }
            boxes.push_back(box);
        }
    }

    cv::Rect merged = unionRect(boxes);
    if (merged.empty()) {
        return cv::Rect(300, 320, 360, 120);
    }

    merged = expandRect(merged, size, 0.20, 0.28);
    merged.y = std::max(merged.y, static_cast<int>(size.height * 0.55));
    merged.height = std::min(merged.height, size.height - merged.y);
    return clampRect(merged, size);
}

static DetectionResult runDetectorOn960(
    const std::vector<VideoFrame>& frames,
    const cv::Rect& roi960,
    int thresholdValue,
    int minArea960,
    int cooldownMs)
{
    DetectionResult result;
    if (frames.empty()) {
        return result;
    }

    cv::Mat previousGray;
    bool previousMotion = false;
    double lastTriggerMs = -1e9;

    for (const VideoFrame& frame : frames) {
        const cv::Rect bounded = clampRect(roi960, frame.gray960.size());
        cv::Mat roi = frame.gray960(bounded);
        cv::Mat currentGray;
        cv::GaussianBlur(roi, currentGray, cv::Size(5, 5), 0);

        if (previousGray.empty()) {
            previousGray = currentGray.clone();
            continue;
        }

        cv::Mat diff;
        cv::absdiff(currentGray, previousGray, diff);
        cv::threshold(diff, diff, thresholdValue, 255, cv::THRESH_BINARY);
        cv::dilate(diff, diff, cv::Mat(), cv::Point(-1, -1), 2);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(diff, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        double largestArea = 0.0;
        for (const auto& contour : contours) {
            largestArea = std::max(largestArea, cv::contourArea(contour));
        }

        result.maxArea = std::max(result.maxArea, largestArea);
        const bool motionDetected = largestArea >= static_cast<double>(minArea960);

        if (motionDetected && !previousMotion && (frame.timestampMs - lastTriggerMs) >= cooldownMs) {
            result.triggerFrames.push_back(frame.index);
            lastTriggerMs = frame.timestampMs;
        }

        previousMotion = motionDetected;
        previousGray = currentGray.clone();
    }

    return result;
}

static double scoreDetector(
    const std::vector<int>& matchedFrames,
    const std::vector<int>& triggerFrames)
{
    if (matchedFrames.empty()) {
        return 1e9;
    }

    std::vector<bool> usedTrigger(triggerFrames.size(), false);
    int matchedCount = 0;
    int totalDistance = 0;

    for (int match : matchedFrames) {
        const int windowStart = std::max(0, match - 18);
        const int windowEnd = match + 4;
        const int target = std::max(0, match - 6);

        int bestTriggerIndex = -1;
        int bestDistance = std::numeric_limits<int>::max();

        for (size_t index = 0; index < triggerFrames.size(); ++index) {
            if (usedTrigger[index]) {
                continue;
            }

            const int trigger = triggerFrames[index];
            if (trigger < windowStart || trigger > windowEnd) {
                continue;
            }

            const int distance = std::abs(trigger - target);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestTriggerIndex = static_cast<int>(index);
            }
        }

        if (bestTriggerIndex >= 0) {
            usedTrigger[bestTriggerIndex] = true;
            totalDistance += bestDistance;
            ++matchedCount;
        }
    }

    int falseTriggers = 0;
    for (bool used : usedTrigger) {
        if (!used) {
            ++falseTriggers;
        }
    }

    const int missed = static_cast<int>(matchedFrames.size()) - matchedCount;
    return static_cast<double>(missed * 1500 + falseTriggers * 500 + totalDistance * 8);
}

static cv::Rect scaleRectToFull(const cv::Rect& roi960, const cv::Size& fullSize)
{
    const double scaleX = static_cast<double>(fullSize.width) / 960.0;
    const double scaleY = static_cast<double>(fullSize.height) / 540.0;

    cv::Rect roi(
        static_cast<int>(roi960.x * scaleX),
        static_cast<int>(roi960.y * scaleY),
        static_cast<int>(roi960.width * scaleX),
        static_cast<int>(roi960.height * scaleY));

    return clampRect(roi, fullSize);
}

static std::string normalizedRectString(const cv::Rect& rect, const cv::Size& fullSize)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4)
        << (static_cast<double>(rect.x) / fullSize.width) << ", "
        << (static_cast<double>(rect.y) / fullSize.height) << ", "
        << (static_cast<double>(rect.width) / fullSize.width) << ", "
        << (static_cast<double>(rect.height) / fullSize.height);
    return oss.str();
}

static void writeRoiDebugImage(
    const fs::path& videoPath,
    int frameIndex,
    const cv::Rect& roiFull,
    const fs::path& outputPath)
{
    cv::VideoCapture capture(videoPath.string(), cv::CAP_ANY);
    if (!capture.isOpened()) {
        return;
    }

    capture.set(cv::CAP_PROP_POS_FRAMES, frameIndex);
    cv::Mat frame;
    if (!capture.read(frame) || frame.empty()) {
        return;
    }

    cv::rectangle(frame, roiFull, cv::Scalar(0, 255, 0), 8);
    cv::putText(frame, "Suggested ROI", cv::Point(roiFull.x, std::max(40, roiFull.y - 14)),
        cv::FONT_HERSHEY_SIMPLEX, 1.4, cv::Scalar(0, 255, 0), 4, cv::LINE_AA);
    cv::imwrite(outputPath.string(), frame);
}

int main(int argc, char** argv)
{
    try {
        const fs::path workspace = (argc >= 2) ? fs::path(argv[1]) : fs::path("D:/poke");
        const fs::path videoPath = workspace / "test3.MOV";
        const fs::path sampleDir = workspace / "sample";
        const fs::path analysisDir = workspace / "analysis";
        fs::create_directories(analysisDir);

        const std::vector<SampleImage> samples = loadSamples(sampleDir);
        if (samples.empty()) {
            throw std::runtime_error("No sample images found in sample directory.");
        }

        double fps = 60.0;
        cv::Size fullSize;
        const std::vector<VideoFrame> frames = decodeVideo(videoPath, fps, fullSize);
        if (frames.empty()) {
            throw std::runtime_error("No frames decoded from video.");
        }

        const auto cost = buildCostMatrix(samples, frames);
        const std::vector<int> matchedFrames = pickOrderedMatches(cost);
        const cv::Rect baseRoi960 = buildBaseRoi960(frames, matchedFrames);

        std::vector<cv::Rect> roiCandidates = {
            baseRoi960,
            expandRect(baseRoi960, cv::Size(960, 540), 0.10, 0.10),
            expandRect(baseRoi960, cv::Size(960, 540), 0.18, 0.16),
            clampRect(cv::Rect(baseRoi960.x, std::max(baseRoi960.y, 320), baseRoi960.width, baseRoi960.height), cv::Size(960, 540)),
            clampRect(cv::Rect(baseRoi960.x, std::max(baseRoi960.y, 340), baseRoi960.width, baseRoi960.height), cv::Size(960, 540)),
            clampRect(cv::Rect(baseRoi960.x, std::max(baseRoi960.y, 355), baseRoi960.width, baseRoi960.height), cv::Size(960, 540))
        };

        CandidateConfig best;
        for (const cv::Rect& roi : roiCandidates) {
            for (int thresholdValue : {16, 18, 20, 22, 24, 26, 28}) {
                for (int minArea960 : {80, 100, 120, 140, 160, 190, 220, 260}) {
                    for (int cooldownMs : {350, 450, 550, 650, 800}) {
                        const DetectionResult detection = runDetectorOn960(frames, roi, thresholdValue, minArea960, cooldownMs);
                        const double score = scoreDetector(matchedFrames, detection.triggerFrames);

                        if (score < best.score) {
                            best.roi960 = roi;
                            best.threshold = thresholdValue;
                            best.minArea960 = minArea960;
                            best.cooldownMs = cooldownMs;
                            best.score = score;
                            best.triggerFrames = detection.triggerFrames;
                        }
                    }
                }
            }
        }

        const cv::Rect roiFull = scaleRectToFull(best.roi960, fullSize);
        const int minAreaFull = best.minArea960 * 16;

        std::ofstream report(analysisDir / "fit_defaults_report.txt", std::ios::out | std::ios::trunc);
        report << "video_fps=" << std::fixed << std::setprecision(4) << fps << "\n";
        report << "video_size=" << fullSize.width << "x" << fullSize.height << "\n";
        report << "sample_matches:\n";
        for (size_t index = 0; index < samples.size(); ++index) {
            report << "  " << samples[index].name << " -> frame " << matchedFrames[index]
                   << " (" << std::fixed << std::setprecision(3)
                   << (static_cast<double>(matchedFrames[index]) / fps) << "s)\n";
        }
        report << "roi_full_px=" << roiFull.x << "," << roiFull.y << "," << roiFull.width << "," << roiFull.height << "\n";
        report << "roi_normalized=" << normalizedRectString(roiFull, fullSize) << "\n";
        report << "threshold=" << best.threshold << "\n";
        report << "min_area_full=" << minAreaFull << "\n";
        report << "cooldown_ms=" << best.cooldownMs << "\n";
        report << "trigger_frames=";
        for (int trigger : best.triggerFrames) {
            report << trigger << " ";
        }
        report << "\n";
        report << "score=" << best.score << "\n";
        report.close();

        writeRoiDebugImage(videoPath, matchedFrames.front(), roiFull, analysisDir / "suggested_roi.jpg");

        std::cout << "Report written: " << (analysisDir / "fit_defaults_report.txt").string() << "\n";
        std::cout << "Suggested ROI normalized: " << normalizedRectString(roiFull, fullSize) << "\n";
        std::cout << "Suggested threshold: " << best.threshold << "\n";
        std::cout << "Suggested minArea(full): " << minAreaFull << "\n";
        std::cout << "Suggested cooldownMs: " << best.cooldownMs << "\n";
        std::cout << "Trigger frames:";
        for (int trigger : best.triggerFrames) {
            std::cout << ' ' << trigger;
        }
        std::cout << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }
}
