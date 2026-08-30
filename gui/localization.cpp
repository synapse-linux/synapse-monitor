// SPDX-License-Identifier: MIT
#include "localization.h"

#include <QCoreApplication>
#include <QLocale>
#include <QQmlApplicationEngine>

MonitorLocalization::MonitorLocalization(QObject *parent) : QObject(parent) {}

QString MonitorLocalization::currentLocale() const { return currentLocale_; }

QStringList MonitorLocalization::availableLocales() const {
    return {QStringLiteral("it_IT"), QStringLiteral("en_US")};
}

QString MonitorLocalization::normalize(const QString &locale) const {
    const QString normalized = locale.trimmed().replace(QLatin1Char('-'), QLatin1Char('_'));
    if (normalized.startsWith(QStringLiteral("it"), Qt::CaseInsensitive))
        return QStringLiteral("it_IT");
    return QStringLiteral("en_US");
}

bool MonitorLocalization::initialize(const QString &requestedLocale) {
    const QString requested = requestedLocale.isEmpty() ? QLocale::system().name()
                                                        : requestedLocale;
    return setLocale(normalize(requested));
}

void MonitorLocalization::attachEngine(QQmlApplicationEngine *engine) { engine_ = engine; }

bool MonitorLocalization::setLocale(const QString &locale) {
    const QString selected = normalize(locale);
    if (selected == currentLocale_) return true;
    QCoreApplication::removeTranslator(&translator_);
    const QString resource = QStringLiteral(":/i18n/synapse-monitor_%1.qm").arg(selected);
    if (!translator_.load(resource)) {
        if (selected != QStringLiteral("en_US")
            && translator_.load(QStringLiteral(":/i18n/synapse-monitor_en_US.qm"))) {
            currentLocale_ = QStringLiteral("en_US");
        } else {
            currentLocale_.clear();
            return false;
        }
    } else {
        currentLocale_ = selected;
    }
    QCoreApplication::installTranslator(&translator_);
    if (engine_) engine_->retranslate();
    emit currentLocaleChanged();
    return true;
}
