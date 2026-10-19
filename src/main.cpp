#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>

#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("hazard-1");
    QApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Indoor smoke/fire detection from a USB camera.");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption modelOpt("model", "Path to the ONNX detection model.", "path",
                                 QApplication::applicationDirPath() + "/models/fire_smoke_yolov8n.onnx");
    QCommandLineOption classesOpt("classes", "Path to the classes.names file.", "path",
                                   QApplication::applicationDirPath() + "/models/classes.names");
    QCommandLineOption cameraOpt("camera", "Open this camera index on startup and begin monitoring.",
                                  "index");
    QCommandLineOption videoOpt("video", "Open this video file on startup and begin monitoring.",
                                 "path");
    parser.addOption(modelOpt);
    parser.addOption(classesOpt);
    parser.addOption(cameraOpt);
    parser.addOption(videoOpt);
    parser.process(app);

    MainWindow window(parser.value(modelOpt), parser.value(classesOpt));
    window.show();

    if (parser.isSet(cameraOpt)) {
        window.openCameraAndStart(parser.value(cameraOpt).toInt());
    } else if (parser.isSet(videoOpt)) {
        window.openVideoFileAndStart(parser.value(videoOpt));
    }

    return QApplication::exec();
}
