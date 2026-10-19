#include "DetectionEngine.h"

#include <opencv2/imgproc.hpp>

#include <chrono>
#include <fstream>
#include <map>

bool DetectionEngine::loadModel(const std::string& modelPath, const std::string& classNamesPath,
                                 std::string* errorOut) {
    loaded_ = false;
    classNames_.clear();

    std::ifstream namesFile(classNamesPath);
    if (!namesFile.is_open()) {
        if (errorOut) *errorOut = "Could not open class names file: " + classNamesPath;
        return false;
    }
    std::string line;
    while (std::getline(namesFile, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
            line.pop_back();
        if (!line.empty()) classNames_.push_back(line);
    }
    if (classNames_.empty()) {
        if (errorOut) *errorOut = "Class names file is empty: " + classNamesPath;
        return false;
    }

    try {
        net_ = cv::dnn::readNetFromONNX(modelPath);
    } catch (const cv::Exception& e) {
        if (errorOut) *errorOut = std::string("Failed to load ONNX model: ") + e.what();
        return false;
    }
    if (net_.empty()) {
        if (errorOut) *errorOut = "ONNX model loaded empty net: " + modelPath;
        return false;
    }

    net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

    loaded_ = true;
    return true;
}

cv::Mat DetectionEngine::letterbox(const cv::Mat& src, LetterboxInfo& info) const {
    const int srcW = src.cols;
    const int srcH = src.rows;
    info.scale = std::min(static_cast<float>(kInputSize) / srcW,
                           static_cast<float>(kInputSize) / srcH);
    const int newW = static_cast<int>(std::round(srcW * info.scale));
    const int newH = static_cast<int>(std::round(srcH * info.scale));

    cv::Mat resized;
    cv::resize(src, resized, cv::Size(newW, newH), 0, 0, cv::INTER_LINEAR);

    info.padX = (kInputSize - newW) / 2;
    info.padY = (kInputSize - newH) / 2;

    cv::Mat canvas(kInputSize, kInputSize, src.type(), cv::Scalar(114, 114, 114));
    resized.copyTo(canvas(cv::Rect(info.padX, info.padY, newW, newH)));
    return canvas;
}

std::vector<Detection> DetectionEngine::detect(const cv::Mat& frameBgr, double* inferenceMsOut) {
    std::vector<Detection> results;
    if (!loaded_ || frameBgr.empty()) return results;

    LetterboxInfo lb;
    cv::Mat input = letterbox(frameBgr, lb);

    cv::Mat blob = cv::dnn::blobFromImage(input, 1.0 / 255.0, cv::Size(kInputSize, kInputSize),
                                           cv::Scalar(), /*swapRB=*/true, /*crop=*/false);
    net_.setInput(blob);

    const auto t0 = std::chrono::steady_clock::now();
    cv::Mat output = net_.forward();
    const auto t1 = std::chrono::steady_clock::now();
    if (inferenceMsOut) {
        *inferenceMsOut = std::chrono::duration<double, std::milli>(t1 - t0).count();
    }

    // Ultralytics YOLOv8 ONNX export (dynamic=False) produces a single output
    // tensor shaped [1, 4 + numClasses, numAnchors]: rows are
    // [cx, cy, w, h, class0_score, class1_score, ...] in the *columns*, i.e.
    // channels-first. Reshape away the batch dim and transpose so each row is
    // one anchor prediction: [cx, cy, w, h, score_0 .. score_{numClasses-1}].
    const int numClasses = static_cast<int>(classNames_.size());
    const int expectedRows = 4 + numClasses;
    if (output.dims < 3 || output.size[1] != expectedRows) {
        // Unexpected export shape (e.g. a different opset/model); bail out
        // quietly rather than reading garbage memory.
        return results;
    }

    cv::Mat reshaped = output.reshape(1, expectedRows);  // (4+numClasses) x numAnchors
    cv::Mat predictions;
    cv::transpose(reshaped, predictions);  // numAnchors x (4+numClasses)

    std::vector<cv::Rect> boxesByClass;
    std::vector<float> scoresByClass;
    std::vector<int> classIdsAll;
    std::vector<cv::Rect> boxesAll;
    std::vector<float> scoresAll;

    for (int i = 0; i < predictions.rows; ++i) {
        const float* row = predictions.ptr<float>(i);
        const float cx = row[0];
        const float cy = row[1];
        const float w = row[2];
        const float h = row[3];

        int bestClass = -1;
        float bestScore = 0.0f;
        for (int c = 0; c < numClasses; ++c) {
            const float score = row[4 + c];
            if (score > bestScore) {
                bestScore = score;
                bestClass = c;
            }
        }
        if (bestClass < 0 || bestScore < confThreshold_) continue;

        // Undo letterbox padding/scale to map back to the original frame.
        const float x1 = (cx - w / 2.0f - lb.padX) / lb.scale;
        const float y1 = (cy - h / 2.0f - lb.padY) / lb.scale;
        const float boxW = w / lb.scale;
        const float boxH = h / lb.scale;

        cv::Rect box(static_cast<int>(std::round(x1)), static_cast<int>(std::round(y1)),
                     static_cast<int>(std::round(boxW)), static_cast<int>(std::round(boxH)));
        box &= cv::Rect(0, 0, frameBgr.cols, frameBgr.rows);
        if (box.width <= 0 || box.height <= 0) continue;

        boxesAll.push_back(box);
        scoresAll.push_back(bestScore);
        classIdsAll.push_back(bestClass);
    }

    // Run NMS per class so overlapping fire/smoke boxes of *different*
    // classes (which commonly co-occur) don't suppress each other.
    std::map<int, std::vector<int>> indicesByClass;
    for (int i = 0; i < static_cast<int>(classIdsAll.size()); ++i) {
        indicesByClass[classIdsAll[i]].push_back(i);
    }
    for (auto& [classId, idxList] : indicesByClass) {
        std::vector<cv::Rect> clsBoxes;
        std::vector<float> clsScores;
        for (int idx : idxList) {
            clsBoxes.push_back(boxesAll[idx]);
            clsScores.push_back(scoresAll[idx]);
        }
        std::vector<int> kept;
        cv::dnn::NMSBoxes(clsBoxes, clsScores, confThreshold_, nmsThreshold_, kept);
        for (int k : kept) {
            Detection d;
            d.classId = classId;
            d.className = classNames_[classId];
            d.confidence = clsScores[k];
            d.box = clsBoxes[k];
            results.push_back(d);
        }
    }

    return results;
}
