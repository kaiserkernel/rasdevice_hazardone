#pragma once

#include <QMainWindow>
#include <QString>

#include <memory>

#include "DetectionEngine.h"
#include "DetectionWorker.h"

class QLabel;
class QPlainTextEdit;
class QDoubleSpinBox;
class QSpinBox;
class QAction;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString& modelPath, const QString& classNamesPath,
                         QWidget* parent = nullptr);
    ~MainWindow() override;

    // Used for headless/CLI startup (e.g. a systemd service on the Pi) so the
    // app can go straight to monitoring without any menu interaction.
    bool openCameraAndStart(int index);
    bool openVideoFileAndStart(const QString& path);

private slots:
    void onOpenCamera();
    void onOpenVideoFile();
    void onStartStop();
    void onConfidenceChanged(double value);
    void onDetectEveryNChanged(int value);

    void onFrameReady(const QImage& image, double fps, double inferenceMs);
    void onAlertStateChanged(bool fireActive, bool smokeActive);
    void onLogMessage(const QString& message);
    void onSourceError(const QString& message);
    void onSourceRecovered();

private:
    void buildUi();
    void startWorkerIfPossible();
    void updateAlertBanner();

    std::shared_ptr<DetectionEngine> engine_;
    DetectionWorker* worker_ = nullptr;

    QLabel* videoLabel_ = nullptr;
    QLabel* alertBanner_ = nullptr;
    QLabel* statusFps_ = nullptr;
    QLabel* statusInference_ = nullptr;
    QLabel* statusSource_ = nullptr;
    QPlainTextEdit* logView_ = nullptr;
    QDoubleSpinBox* confidenceSpin_ = nullptr;
    QSpinBox* detectEveryNSpin_ = nullptr;
    QAction* startStopAction_ = nullptr;

    bool running_ = false;
    bool modelLoaded_ = false;
    QString sourceDescription_;
};
