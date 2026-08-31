// SPDX-License-Identifier: MIT
#include "localization.h"

#include <QCoreApplication>
#include <QLocale>

MonitorLocalization::MonitorLocalization(QObject *parent) : QObject(parent) {}

QString MonitorLocalization::normalize(const QString &locale) const {
    const QString normalized = locale.trimmed().replace(QLatin1Char('-'), QLatin1Char('_'));
    if (normalized.startsWith(QStringLiteral("it"), Qt::CaseInsensitive))
        return QStringLiteral("it_IT");
    return QStringLiteral("en_US");
}

bool MonitorLocalization::initialize(const QString &requestedLocale) {
    const QString requested = requestedLocale.isEmpty() ? QLocale::system().name()
                                                        : requestedLocale;
    return loadLocale(normalize(requested));
}

bool MonitorLocalization::loadLocale(const QString &locale) {
    QString selected = normalize(locale);
    QString resource = QStringLiteral(":/i18n/synapse-monitor_%1.qm").arg(selected);
    if (!translator_.load(resource)) {
        selected = QStringLiteral("en_US");
        resource = QStringLiteral(":/i18n/synapse-monitor_en_US.qm");
        if (!translator_.load(resource)) return false;
    }
    return QCoreApplication::installTranslator(&translator_);
}
