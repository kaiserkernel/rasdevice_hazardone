#pragma once

#include <opencv2/dnn.hpp>
#include <string>
#include <vector>

#include "Detection.h"

// Wraps an ONNX YOLO-style object detector (trained on the Home-fire-dataset,
// see training/TRAINING_REPORT.md) and runs it through OpenCV's dnn module.
// Loading the ONNX graph via cv::dnn has no dependency on the training
// framework (Ultralytics/AGPL) at inference time -- only OpenCV is needed,
// which is what makes this practical to deploy on a Raspberry Pi via apt.
class DetectionEngine {
public:
    DetectionEngine() = default;

    // modelPath: .onnx file exported with a fixed input size (see training/).
    // classNamesPath: plain text file, one class name per line, matching the
    // exact class index order used during training/export.
    bool loadModel(const std::string& modelPath, const std::string& classNamesPath,
                    std::string* errorOut = nullptr);

    bool isLoaded() const { return loaded_; }
    int inputSize() const { return kInputSize; }
    const std::vector<std::string>& classNames() const { return classNames_; }

    void setConfidenceThreshold(float t) { confThreshold_ = t; }
    float confidenceThreshold() const { return confThreshold_; }
    void setNmsThreshold(float t) { nmsThreshold_ = t; }

    // Runs the network on a BGR frame and returns detections already mapped
    // back into the frame's original coordinate space.
    std::vector<Detection> detect(const cv::Mat& frameBgr, double* inferenceMsOut = nullptr);

private:
    struct LetterboxInfo {
        float scale = 1.0f;
        int padX = 0;
        int padY = 0;
    };

    cv::Mat letterbox(const cv::Mat& src, LetterboxInfo& info) const;

    cv::dnn::Net net_;
    std::vector<std::string> classNames_;
    bool loaded_ = false;
    float confThreshold_ = 0.40f;
    float nmsThreshold_ = 0.45f;
    static constexpr int kInputSize = 320;
};
