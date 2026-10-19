#include "AlertManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

AlertManager::AlertManager(QObject* parent) : QObject(parent) {}

void AlertManager::setLogFilePath(const QString& path) {
    logFilePath_ = path;
    QDir().mkpath(QFileInfo(path).absolutePath());
}

void AlertManager::updateStreak(bool seenThisFrame, int& streak, bool& active, const char* label) {
    if (seenThisFrame) {
        streak++;
        if (!active && streak >= kStreakThreshold) {
            active = true;
            logEvent(QString("%1 ALERT: sustained detection over %2 frames")
                         .arg(label)
                         .arg(kStreakThreshold));
        }
    } else {
        streak = 0;
        if (active) {
            active = false;
            logEvent(QString("%1 clear: no detection").arg(label));
        }
    }
}

void AlertManager::processFrame(bool fireSeen, bool smokeSeen) {
    const bool wasFireActive = fireActive_;
    const bool wasSmokeActive = smokeActive_;

    updateStreak(fireSeen, fireStreak_, fireActive_, "FIRE");
    updateStreak(smokeSeen, smokeStreak_, smokeActive_, "SMOKE");

    if (fireActive_ != wasFireActive || smokeActive_ != wasSmokeActive) {
        emit alertStateChanged(fireActive_, smokeActive_);
    }
}

void AlertManager::logEvent(const QString& message) {
    const QString timestamped =
        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz") + "  " + message;

    if (!logFilePath_.isEmpty()) {
        QFile f(logFilePath_);
        if (f.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&f);
            out << timestamped << '\n';
        }
    }
    emit eventLogged(timestamped);
}
