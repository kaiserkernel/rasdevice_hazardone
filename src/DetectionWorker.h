#pragma once

#include <QImage>
#include <QMutex>
#include <QString>
#include <QThread>

#include <atomic>
#include <memory>
#include <opencv2/videoio.hpp>

#include "AlertManager.h"
#include "Detection.h"
#include "DetectionEngine.h"

// Owns the video source (USB camera or a video file, for testing without a
// camera attached) and the detection engine, and runs capture + inference in
// a background thread so the UI stays responsive. Detection does not run on
// every captured frame -- on a Raspberry Pi 4B the network forward pass is
// far slower than the camera's frame rate, so inference runs every
// detectEveryN() frames and every displayed frame is annotated with the most
// recently computed boxes.
class DetectionWorker : public QThread {
    Q_OBJECT
public:
    explicit DetectionWorker(QObject* parent = nullptr);
    ~DetectionWorker() override;

    bool openCamera(int index, QString* errorOut = nullptr);
    bool openVideoFile(const QString& path, QString* errorOut = nullptr);
    void stopCapture();

    void setDetectionEngine(std::shared_ptr<DetectionEngine> engine) { engine_ = std::move(engine); }
    void setDetectEveryNFrames(int n) { detectEveryN_ = n > 0 ? n : 1; }
    void setAlertLogPath(const QString& path) { alertManager_.setLogFilePath(path); }

    AlertManager& alertManager() { return alertManager_; }

signals:
    // image is already BGR->RGB converted and annotated with detection boxes.
    void frameReady(const QImage& image, double fps, double inferenceMs);
    void logMessage(const QString& message);
    void sourceError(const QString& message);
    void sourceRecovered();

protected:
    void run() override;

private:
    void drawDetections(cv::Mat& frame, const std::vector<Detection>& detections) const;

    cv::VideoCapture cap_;
    QMutex sourceMutex_;
    bool isFileSource_ = false;
    int cameraIndex_ = 0;
    QString videoPath_;

    std::atomic<bool> running_{false};
    std::shared_ptr<DetectionEngine> engine_;
    int detectEveryN_ = 3;

    std::vector<Detection> lastDetections_;
    AlertManager alertManager_;
};
