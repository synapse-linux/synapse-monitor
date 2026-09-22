// SPDX-License-Identifier: MIT
#include "localization.h"
#include <QGuiApplication>
#include <QTranslator>
#include <cstdio>
#include "locales.inc"

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    MonitorLocalization loc;
    int failures = 0;
    auto check = [&failures](bool value, const char *name) {
        if (!value) { std::fprintf(stderr, "FAIL: %s\n", name); ++failures; }
    };
    const QString mode = argc > 1 ? QString::fromLatin1(argv[1]) : QStringLiteral("full");
    if (mode == QStringLiteral("missing")) {
        check(!loc.initialize(QStringLiteral("en_US")), "missing fallback fails closed");
    } else {
        for (const QString &bad : {QStringLiteral("it-evil"), QStringLiteral("../it_IT"),
                                  QStringLiteral("ar@evil"), QStringLiteral("it_IT\n"),
                                  QStringLiteral("it_ZZ"), QString(65, QLatin1Char('a'))}) {
            check(loc.initialize(bad), "fallback catalog loads");
            check(loc.selectedLocale() == QStringLiteral("en_US"), "unknown or malformed locale");
            check(qtTrId("synapse.monitor.view.processes") == QStringLiteral("Processes"),
                  "malformed Italian prefix must not select Italian");
        }
        if (mode == QStringLiteral("full")) {
            for (const char *id : kMonitorLocales) {
                check(loc.initialize(QString::fromLatin1(id)), id);
                check(loc.selectedLocale() == QString::fromLatin1(id), id);
                const bool rtl = loc.selectedLocale() == QStringLiteral("ar")
                                 || loc.selectedLocale() == QStringLiteral("fa")
                                 || loc.selectedLocale() == QStringLiteral("he");
                check((QGuiApplication::layoutDirection() == Qt::RightToLeft) == rtl, id);
                check(!qtTrId("synapse.monitor.view.processes").startsWith(QStringLiteral("synapse.")), id);
            }
            for (const char *name : {"it", "it-IT", "it_IT.UTF-8", "it_IT.utf8"}) {
                check(loc.initialize(QString::fromLatin1(name)), name);
                check(loc.selectedLocale() == QStringLiteral("it_IT"), name);
            }
            qunsetenv("LC_ALL"); qputenv("LC_MESSAGES", "ar"); qputenv("LANG", "it_IT");
            check(loc.initialize(QString()), "session initialization");
            check(loc.selectedLocale() == QStringLiteral("ar"), "LC_MESSAGES precedes LANG");
            qputenv("LC_ALL", "en_US");
            check(loc.initialize(QString()), "LC_ALL initialization");
            check(loc.selectedLocale() == QStringLiteral("en_US"), "LC_ALL precedes LC_MESSAGES");
        } else {
            check(loc.initialize(QStringLiteral("ar")), "partial Arabic catalog loads");
            check(loc.selectedLocale() == QStringLiteral("ar"), "partial Arabic selected");
            check(qtTrId("synapse.monitor.title") == QStringLiteral("TEST AR"), "selected message wins");
            check(qtTrId("synapse.monitor.view.processes") == QStringLiteral("Processes"), "missing message uses English");
            check(QGuiApplication::layoutDirection() == Qt::RightToLeft, "Arabic is RTL");
            check(loc.initialize(QStringLiteral("fr")), "missing catalog uses English");
            check(loc.selectedLocale() == QStringLiteral("en_US"), "missing catalog fallback identity");
        }
        check(loc.initialize(QStringLiteral("en_US")), "English loads");
        check(QGuiApplication::layoutDirection() == Qt::LeftToRight, "English restores LTR");
    }
    std::printf("Localization %s checks: %s\n", mode.toLatin1().constData(), failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
