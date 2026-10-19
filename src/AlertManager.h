#pragma once

#include <QObject>
#include <QString>

// Turns raw per-frame detections into a debounced alert state and an event
// log. A single frame with a fire/smoke box is not enough to raise an alert
// (webcam noise/lighting can trigger a false positive for a frame or two) --
// an alert only goes active after kStreakThreshold consecutive frames keep
// seeing that class, and clears after the same number of clean frames.
class AlertManager : public QObject {
    Q_OBJECT
public:
    explicit AlertManager(QObject* parent = nullptr);

    void setLogFilePath(const QString& path);

    // Called once per processed (detected-on) frame with whether fire/smoke
    // was seen in that frame.
    void processFrame(bool fireSeen, bool smokeSeen);

    bool fireActive() const { return fireActive_; }
    bool smokeActive() const { return smokeActive_; }

signals:
    void alertStateChanged(bool fireActive, bool smokeActive);
    void eventLogged(const QString& message);

private:
    void logEvent(const QString& message);
    void updateStreak(bool seenThisFrame, int& streak, bool& active, const char* label);

    static constexpr int kStreakThreshold = 5;

    int fireStreak_ = 0;
    int clearStreakFire_ = 0;
    int smokeStreak_ = 0;
    int clearStreakSmoke_ = 0;
    bool fireActive_ = false;
    bool smokeActive_ = false;
    QString logFilePath_;
};
