#pragma once

#include <opencv2/core.hpp>
#include <string>

// A single detected object produced by DetectionEngine for one video frame.
struct Detection {
    int classId = -1;
    std::string className;
    float confidence = 0.0f;
    cv::Rect box;
};
