#include "DetectionWorker.h"

#include <QMutexLocker>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <deque>

namespace {
constexpr int kMaxConsecutiveReadFailures = 15;
constexpr int kReconnectDelayMs = 1000;
}  // namespace

DetectionWorker::DetectionWorker(QObject* parent) : QThread(parent) {
    connect(&alertManager_, &AlertManager::eventLogged, this, &DetectionWorker::logMessage);
}

DetectionWorker::~DetectionWorker() {
    stopCapture();
    wait();
}

bool DetectionWorker::openCamera(int index, QString* errorOut) {
    QMutexLocker lock(&sourceMutex_);
#ifdef HAZARD1_PLATFORM_LINUX
    const bool ok = cap_.open(index, cv::CAP_V4L2);
#else
    const bool ok = cap_.open(index);
#endif
    if (!ok) {
        if (errorOut) *errorOut = QString("Could not open camera index %1").arg(index);
        return false;
    }
    cap_.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    isFileSource_ = false;
    cameraIndex_ = index;
    return true;
}

bool DetectionWorker::openVideoFile(const QString& path, QString* errorOut) {
    QMutexLocker lock(&sourceMutex_);
    const bool ok = cap_.open(path.toStdString());
    if (!ok) {
        if (errorOut) *errorOut = QString("Could not open video file: %1").arg(path);
        return false;
    }
    isFileSource_ = true;
    videoPath_ = path;
    return true;
}

void DetectionWorker::stopCapture() {
    running_ = false;
}

void DetectionWorker::drawDetections(cv::Mat& frame, const std::vector<Detection>& detections) const {
    for (const auto& d : detections) {
        const cv::Scalar color = (d.className == "fire") ? cv::Scalar(0, 0, 255)      // red (BGR)
                                                           : cv::Scalar(0, 255, 255);  // yellow
        cv::rectangle(frame, d.box, color, 2);
        char label[64];
        std::snprintf(label, sizeof(label), "%s %.0f%%", d.className.c_str(), d.confidence * 100.0f);
        const int baseY = std::max(d.box.y, 18);
        cv::putText(frame, label, cv::Point(d.box.x, baseY - 4), cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 2);
    }
}

void DetectionWorker::run() {
    running_ = true;
    int frameCount = 0;
    int consecutiveFailures = 0;
    double inferenceMs = 0.0;

    std::deque<std::chrono::steady_clock::time_point> frameTimes;

    while (running_) {
        cv::Mat frame;
        bool readOk = false;
        {
            QMutexLocker lock(&sourceMutex_);
            if (cap_.isOpened()) readOk = cap_.read(frame);
        }

        if (!readOk || frame.empty()) {
            consecutiveFailures++;
            if (isFileSource_ && consecutiveFailures == 1) {
                // End of file: loop playback so a test video can run indefinitely.
                QMutexLocker lock(&sourceMutex_);
                cap_.set(cv::CAP_PROP_POS_FRAMES, 0);
            } else if (!isFileSource_ && consecutiveFailures >= kMaxConsecutiveReadFailures) {
                emit sourceError(QString("Lost camera %1, attempting to reconnect...").arg(cameraIndex_));
                QThread::msleep(kReconnectDelayMs);
                QMutexLocker lock(&sourceMutex_);
                cap_.release();
#ifdef HAZARD1_PLATFORM_LINUX
                cap_.open(cameraIndex_, cv::CAP_V4L2);
#else
                cap_.open(cameraIndex_);
#endif
                consecutiveFailures = 0;
            } else {
                QThread::msleep(50);
            }
            continue;
        }
        if (consecutiveFailures >= kMaxConsecutiveReadFailures) {
            emit sourceRecovered();
        }
        consecutiveFailures = 0;
        frameCount++;

        if (engine_ && engine_->isLoaded() && frameCount % detectEveryN_ == 0) {
            double ms = 0.0;
            lastDetections_ = engine_->detect(frame, &ms);
            inferenceMs = ms;

            bool fireSeen = false;
            bool smokeSeen = false;
            for (const auto& d : lastDetections_) {
                if (d.className == "fire") fireSeen = true;
                if (d.className == "smoke") smokeSeen = true;
            }
            alertManager_.processFrame(fireSeen, smokeSeen);
        }

        drawDetections(frame, lastDetections_);

        cv::Mat rgb;
        cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
        QImage image(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
        image = image.copy();  // detach from the cv::Mat buffer before it's reused.

        const auto now = std::chrono::steady_clock::now();
        frameTimes.push_back(now);
        while (frameTimes.size() > 30) frameTimes.pop_front();
        double fps = 0.0;
        if (frameTimes.size() >= 2) {
            const double seconds =
                std::chrono::duration<double>(frameTimes.back() - frameTimes.front()).count();
            if (seconds > 0.0) fps = (frameTimes.size() - 1) / seconds;
        }

        emit frameReady(image, fps, inferenceMs);

        if (isFileSource_) {
            // Throttle file playback to a sane rate; a live camera already
            // blocks at its native frame rate inside cap_.read().
            QThread::msleep(33);
        }
    }

    QMutexLocker lock(&sourceMutex_);
    cap_.release();
}
