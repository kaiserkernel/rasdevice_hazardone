#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(const QString& modelPath, const QString& classNamesPath, QWidget* parent)
    : QMainWindow(parent) {
    engine_ = std::make_shared<DetectionEngine>();
    std::string loadError;
    const bool loaded =
        engine_->loadModel(modelPath.toStdString(), classNamesPath.toStdString(), &loadError);

    buildUi();

    worker_ = new DetectionWorker(this);
    worker_->setDetectionEngine(engine_);
    worker_->setAlertLogPath(QApplication::applicationDirPath() + "/logs/alerts.log");

    connect(worker_, &DetectionWorker::frameReady, this, &MainWindow::onFrameReady);
    connect(worker_, &DetectionWorker::logMessage, this, &MainWindow::onLogMessage);
    connect(worker_, &DetectionWorker::sourceError, this, &MainWindow::onSourceError);
    connect(worker_, &DetectionWorker::sourceRecovered, this, &MainWindow::onSourceRecovered);
    connect(&worker_->alertManager(), &AlertManager::alertStateChanged, this,
            &MainWindow::onAlertStateChanged);

    modelLoaded_ = loaded;
    if (!loaded) {
        // Logged (and reflected in the alert banner below), not a blocking
        // dialog: hazard-1 is meant to run unattended (autostart on the Pi),
        // and a modal QMessageBox here would hang forever with no one
        // present to dismiss it.
        onLogMessage(QString("Model load FAILED: %1 -- running in preview-only mode.")
                         .arg(QString::fromStdString(loadError)));
    } else {
        onLogMessage(QString("Model loaded: %1 classes (%2)")
                         .arg(engine_->classNames().size())
                         .arg(QString::fromStdString(
                             [this] {
                                 std::string s;
                                 for (auto& n : engine_->classNames()) s += n + " ";
                                 return s;
                             }())));
    }

    updateAlertBanner();

    setWindowTitle("hazard-1 - Smoke/Fire Detection");
    resize(1000, 700);
}

MainWindow::~MainWindow() {
    if (worker_) {
        worker_->stopCapture();
        worker_->wait();
    }
}

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);

    alertBanner_ = new QLabel(" ", this);
    alertBanner_->setAlignment(Qt::AlignCenter);
    alertBanner_->setFixedHeight(36);
    alertBanner_->setStyleSheet("background-color: #333; color: white; font-weight: bold;");
    layout->addWidget(alertBanner_);

    videoLabel_ = new QLabel(this);
    videoLabel_->setAlignment(Qt::AlignCenter);
    videoLabel_->setMinimumSize(320, 240);
    videoLabel_->setStyleSheet("background-color: black;");
    videoLabel_->setText("No video source. Use File > Open Camera or Open Video File.");
    videoLabel_->setStyleSheet("background-color: black; color: #999;");
    layout->addWidget(videoLabel_, 1);

    setCentralWidget(central);

    // --- Menu ---
    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("Open &Camera...", this, &MainWindow::onOpenCamera);
    fileMenu->addAction("Open &Video File...", this, &MainWindow::onOpenVideoFile);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &QWidget::close);

    auto* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About", this, [this] {
        QMessageBox::information(this, "About hazard-1",
                                  "hazard-1 - indoor smoke/fire detection\n"
                                  "Built with Qt 5.14.2 + OpenCV (cv::dnn).\n"
                                  "Model trained on the Home-fire-dataset "
                                  "(CC BY-NC 4.0, Peng & Kim, IEEE Access 2025).");
    });

    // --- Toolbar ---
    auto* toolbar = addToolBar("Controls");
    startStopAction_ = toolbar->addAction("Start", this, &MainWindow::onStartStop);
    startStopAction_->setEnabled(false);

    toolbar->addWidget(new QLabel(" Confidence: "));
    confidenceSpin_ = new QDoubleSpinBox(this);
    confidenceSpin_->setRange(0.05, 0.95);
    confidenceSpin_->setSingleStep(0.05);
    confidenceSpin_->setValue(0.40);
    connect(confidenceSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            &MainWindow::onConfidenceChanged);
    toolbar->addWidget(confidenceSpin_);

    toolbar->addWidget(new QLabel("  Detect every N frames: "));
    detectEveryNSpin_ = new QSpinBox(this);
    detectEveryNSpin_->setRange(1, 15);
    detectEveryNSpin_->setValue(3);
    connect(detectEveryNSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
            &MainWindow::onDetectEveryNChanged);
    toolbar->addWidget(detectEveryNSpin_);

    // --- Status bar ---
    statusSource_ = new QLabel("Source: none", this);
    statusFps_ = new QLabel("FPS: -", this);
    statusInference_ = new QLabel("Inference: -", this);
    statusBar()->addPermanentWidget(statusSource_, 2);
    statusBar()->addPermanentWidget(statusFps_, 1);
    statusBar()->addPermanentWidget(statusInference_, 1);

    // --- Log dock ---
    auto* dock = new QDockWidget("Event Log", this);
    logView_ = new QPlainTextEdit(dock);
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(500);
    dock->setWidget(logView_);
    addDockWidget(Qt::BottomDockWidgetArea, dock);
}

void MainWindow::onOpenCamera() {
    bool ok = false;
    const int index = QInputDialog::getInt(this, "Open Camera", "Camera index:", 0, 0, 9, 1, &ok);
    if (!ok) return;

    openCameraAndStart(index);
}

void MainWindow::onOpenVideoFile() {
    const QString path = QFileDialog::getOpenFileName(this, "Open Video File", QString(),
                                                        "Video files (*.mp4 *.avi *.mkv *.mov)");
    if (path.isEmpty()) return;

    openVideoFileAndStart(path);
}

bool MainWindow::openCameraAndStart(int index) {
    worker_->stopCapture();
    worker_->wait();
    running_ = false;

    QString error;
    if (!worker_->openCamera(index, &error)) {
        onLogMessage("ERROR: " + error);
        return false;
    }
    sourceDescription_ = QString("Camera %1").arg(index);
    statusSource_->setText("Source: " + sourceDescription_);
    startStopAction_->setEnabled(true);
    onLogMessage("Opened " + sourceDescription_);
    worker_->start();
    running_ = true;
    startStopAction_->setText("Stop");
    return true;
}

bool MainWindow::openVideoFileAndStart(const QString& path) {
    worker_->stopCapture();
    worker_->wait();
    running_ = false;

    QString error;
    if (!worker_->openVideoFile(path, &error)) {
        onLogMessage("ERROR: " + error);
        return false;
    }
    sourceDescription_ = path;
    statusSource_->setText("Source: " + sourceDescription_);
    startStopAction_->setEnabled(true);
    onLogMessage("Opened video file " + path);
    worker_->start();
    running_ = true;
    startStopAction_->setText("Stop");
    return true;
}

void MainWindow::onStartStop() {
    if (!running_) {
        worker_->start();
        running_ = true;
        startStopAction_->setText("Stop");
    } else {
        worker_->stopCapture();
        worker_->wait();
        running_ = false;
        startStopAction_->setText("Start");
    }
}

void MainWindow::onConfidenceChanged(double value) {
    engine_->setConfidenceThreshold(static_cast<float>(value));
}

void MainWindow::onDetectEveryNChanged(int value) { worker_->setDetectEveryNFrames(value); }

void MainWindow::onFrameReady(const QImage& image, double fps, double inferenceMs) {
    videoLabel_->setPixmap(
        QPixmap::fromImage(image).scaled(videoLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    statusFps_->setText(QString("FPS: %1").arg(fps, 0, 'f', 1));
    statusInference_->setText(QString("Inference: %1 ms").arg(inferenceMs, 0, 'f', 0));
}

void MainWindow::updateAlertBanner() {
    if (!modelLoaded_) {
        alertBanner_->setText("NO DETECTION MODEL LOADED - camera preview only");
        alertBanner_->setStyleSheet("background-color: #555; color: white; font-weight: bold;");
        return;
    }
    const bool fire = worker_->alertManager().fireActive();
    const bool smoke = worker_->alertManager().smokeActive();
    if (fire && smoke) {
        alertBanner_->setText("FIRE AND SMOKE DETECTED");
        alertBanner_->setStyleSheet("background-color: #b30000; color: white; font-weight: bold;");
    } else if (fire) {
        alertBanner_->setText("FIRE DETECTED");
        alertBanner_->setStyleSheet("background-color: #cc0000; color: white; font-weight: bold;");
    } else if (smoke) {
        alertBanner_->setText("SMOKE DETECTED");
        alertBanner_->setStyleSheet("background-color: #cc8400; color: white; font-weight: bold;");
    } else {
        alertBanner_->setText("Monitoring - no hazard detected");
        alertBanner_->setStyleSheet("background-color: #2e7d32; color: white; font-weight: bold;");
    }
}

void MainWindow::onAlertStateChanged(bool fireActive, bool smokeActive) {
    updateAlertBanner();
    if (fireActive || smokeActive) {
        QApplication::beep();
    }
}

void MainWindow::onLogMessage(const QString& message) {
    logView_->appendPlainText(message);
}

void MainWindow::onSourceError(const QString& message) {
    statusSource_->setText("Source: " + sourceDescription_ + "  [RECONNECTING]");
    onLogMessage("ERROR: " + message);
}

void MainWindow::onSourceRecovered() {
    statusSource_->setText("Source: " + sourceDescription_);
    onLogMessage("Source recovered.");
}
