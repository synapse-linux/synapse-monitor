// SPDX-License-Identifier: MIT
#include "localization.h"
#include "monitor_adapter.h"
#include "typography.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>

#include <sys/prctl.h>

#ifndef SYNAPSE_MONITOR_VERSION
#define SYNAPSE_MONITOR_VERSION "0.5.0-alpha.10"
#endif

namespace {

QString discoverBackend(const QString &requested) {
    if (!requested.isEmpty()) return requested;
    QString sibling = QDir(QCoreApplication::applicationDirPath())
                                .absoluteFilePath(QStringLiteral("synapse-monitor"));
    const QFileInfo siblingInfo(sibling);
    if (siblingInfo.isFile() && siblingInfo.isExecutable()) return sibling;
    return QStringLiteral("/usr/bin/synapse-monitor");
}

bool parseWindowSize(const QString &value, QSize *size) {
    const QStringList parts = value.split(QLatin1Char('x'));
    bool widthOk = false;
    bool heightOk = false;
    const int width = parts.size() == 2 ? parts.at(0).toInt(&widthOk) : 0;
    const int height = parts.size() == 2 ? parts.at(1).toInt(&heightOk) : 0;
    if (!widthOk || !heightOk || width < 700 || width > 3840
        || height < 480 || height > 2160)
        return false;
    *size = QSize(width, height);
    return true;
}

} // namespace

int main(int argc, char **argv) {
    if (prctl(PR_SET_THP_DISABLE, 1L, 0L, 0L, 0L) != 0) {
        const int error = errno;
        std::fprintf(stderr, "synapse-monitor-gui: PR_SET_THP_DISABLE failed: %s\n",
                     std::strerror(error));
        return 70;
    }
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);

    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("synapse-monitor-gui"));
    QCoreApplication::setApplicationVersion(QStringLiteral(SYNAPSE_MONITOR_VERSION));
    QCoreApplication::setOrganizationName(QStringLiteral("Synapse"));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.synapse.Monitor"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Read-only graphical presentation for Synapse Monitor."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption backendOption(
        QStringLiteral("backend"),
        QStringLiteral("Absolute Synapse Monitor core path (developer/preview use)."),
        QStringLiteral("path"));
    QCommandLineOption viewOption(
        QStringLiteral("view"), QStringLiteral("Initial reviewed view identifier."),
        QStringLiteral("view"), QStringLiteral("processes"));
    QCommandLineOption localeOption(
        QStringLiteral("locale"), QStringLiteral("GUI locale (en_US or it_IT)."),
        QStringLiteral("locale"));
    QCommandLineOption testFramesOption(
        QStringLiteral("test-exit-after-frames"),
        QStringLiteral("Exit after N accepted frames (test only)."), QStringLiteral("count"));
    QCommandLineOption testTimeoutOption(
        QStringLiteral("test-ready-timeout"),
        QStringLiteral("Bounded test timeout in milliseconds."), QStringLiteral("milliseconds"));
    QCommandLineOption testSizeOption(
        QStringLiteral("test-window-size"),
        QStringLiteral("Bounded WIDTHxHEIGHT test window size."), QStringLiteral("size"));
    QCommandLineOption testGrabOption(
        QStringLiteral("test-grab"),
        QStringLiteral("Absolute PNG output path after the requested frames."),
        QStringLiteral("path"));
    parser.addOptions({backendOption, viewOption, localeOption, testFramesOption,
                       testTimeoutOption, testSizeOption, testGrabOption});
    parser.process(application);
    if (parser.isSet(backendOption)
        && qgetenv("SYNAPSE_MONITOR_ALLOW_TEST_BACKEND") != QByteArrayLiteral("1")) {
        qCritical("synapse-monitor-gui: backend override requires explicit test authority");
        return 2;
    }

    int testFrames = 0;
    int testTimeout = 0;
    QSize testSize;
    QString testGrab;
    if (parser.isSet(testFramesOption)) {
        bool ok = false;
        testFrames = parser.value(testFramesOption).toInt(&ok);
        if (!ok || testFrames < 1 || testFrames > 100) {
            qCritical("synapse-monitor-gui: invalid test frame count");
            return 2;
        }
    }
    if (parser.isSet(testTimeoutOption)) {
        bool ok = false;
        testTimeout = parser.value(testTimeoutOption).toInt(&ok);
        if (!ok || testTimeout < 250 || testTimeout > 30000 || testFrames == 0) {
            qCritical("synapse-monitor-gui: invalid test timeout");
            return 2;
        }
    }
    if (parser.isSet(testSizeOption)) {
        if (testFrames == 0 || !parseWindowSize(parser.value(testSizeOption), &testSize)) {
            qCritical("synapse-monitor-gui: invalid test window size");
            return 2;
        }
    }
    if (parser.isSet(testGrabOption)) {
        const QFileInfo output(parser.value(testGrabOption));
        if (testFrames == 0 || !output.isAbsolute() || output.fileName().isEmpty()
            || output.absoluteFilePath().toUtf8().size() >= 4096
            || !QDir(output.absolutePath()).exists()) {
            qCritical("synapse-monitor-gui: invalid test grab path");
            return 2;
        }
        testGrab = output.absoluteFilePath();
    }

    application.setFont(MonitorTypography::interfaceFont(application.font()));
    const QFontInfo resolvedFont(application.font());
    std::fprintf(stderr, "synapse-monitor-gui: typography=monospace resolved=%s\n",
                 resolvedFont.family().toUtf8().constData());

    MonitorAdapter adapter(discoverBackend(parser.value(backendOption)));

    MonitorLocalization localization;
    if (!localization.initialize(parser.value(localeOption))) {
        qCritical("synapse-monitor-gui: translation catalog unavailable");
        return 2;
    }

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &application,
                     [](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings)
            std::fprintf(stderr, "synapse-monitor-gui: qml=%s\n",
                         warning.toString().toUtf8().constData());
    });
    engine.rootContext()->setContextProperty(QStringLiteral("monitorAdapter"), &adapter);
    if (!QFile::exists(QStringLiteral(":/qml/Main.qml"))) {
        qCritical("synapse-monitor-gui: embedded QML unavailable");
        return 3;
    }
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 3;

    QQuickWindow *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!window) return 3;
    if (window->rendererInterface()->graphicsApi() != QSGRendererInterface::Software) {
        qCritical("synapse-monitor-gui: software renderer unavailable");
        return 3;
    }
    std::fprintf(stderr,
                 "synapse-monitor-gui: renderer=software transparent-huge-pages=disabled\n");
    if (testSize.isValid()) {
        window->setWidth(testSize.width());
        window->setHeight(testSize.height());
    }

    if (testFrames > 0) {
        QPointer<QQuickWindow> guardedWindow(window);
        const auto accepted = std::make_shared<int>(0);
        QObject::connect(&adapter, &MonitorAdapter::frameAccepted, &application,
                         [&application, guardedWindow, accepted, testFrames, testGrab]() {
            ++(*accepted);
            if (*accepted != testFrames) return;
            QTimer::singleShot(120, &application,
                               [guardedWindow, testGrab]() {
                bool saved = true;
                if (!testGrab.isEmpty()) {
                    const QImage image = guardedWindow ? guardedWindow->grabWindow()
                                                       : QImage();
                    saved = !image.isNull() && image.save(testGrab, "PNG");
                }
                QCoreApplication::exit(saved ? 0 : 4);
            });
        });
        const int boundedTimeout = testTimeout > 0 ? testTimeout : 10000;
        QTimer::singleShot(boundedTimeout, &application,
                           []() { QCoreApplication::exit(5); });
    }

    const bool initialized = adapter.initialize(parser.value(viewOption));
    if (!initialized && testFrames == 0)
        std::fprintf(stderr, "synapse-monitor-gui: adapter initialization failed\n");
    return application.exec();
}
