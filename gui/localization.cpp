// SPDX-License-Identifier: MIT
#include "localization.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocale>
#include <QRegularExpression>

#include "locales.inc"

MonitorLocalization::MonitorLocalization(QObject *parent) : QObject(parent) {}

QString MonitorLocalization::normalize(const QString &locale) const {
    // Only bounded locale data reaches a fixed, embedded resource allowlist.
    if (locale.size() > 64) return QStringLiteral("en_US");
    static const QRegularExpression syntax(QStringLiteral(
        "\\A[a-zA-Z]{2,3}(?:[_-][a-zA-Z]{2})?(?:\\.(?:UTF-8|utf8))?(?:@[a-z]+)?\\z"));
    if (!syntax.match(locale).hasMatch()) return QStringLiteral("en_US");
    QString value = locale;
    value.replace(QLatin1Char('-'), QLatin1Char('_'));
    value.remove(QRegularExpression(QStringLiteral("\\.(?:UTF_8|utf8)")));
    for (const char *id : kMonitorLocales) {
        const QString candidate = QString::fromLatin1(id);
        if (value.compare(candidate, Qt::CaseInsensitive) == 0) return candidate;
    }
    // The pinned set uses base identifiers as well as specific regions. Accept
    // only their Qt canonical names, not arbitrary prefix/region/modifier text.
    if (!value.contains(QLatin1Char('@'))) {
        for (const char *id : kMonitorLocales) {
            const QString candidate = QString::fromLatin1(id);
            if (candidate.contains(QLatin1Char('@'))) continue;
            if (value.compare(QLocale(candidate).name(), Qt::CaseInsensitive) == 0)
                return candidate;
        }
        if (value == QStringLiteral("en")) return QStringLiteral("en_US");
        if (value == QStringLiteral("it")) return QStringLiteral("it_IT");
        if (value == QStringLiteral("cs")) return QStringLiteral("cs_CZ");
        if (value == QStringLiteral("fi")) return QStringLiteral("fi_FI");
        if (value == QStringLiteral("tr")) return QStringLiteral("tr_TR");
        if (value == QStringLiteral("pt")) return QStringLiteral("pt_PT");
    }
    return QStringLiteral("en_US");
}

bool MonitorLocalization::initialize(const QString &requestedLocale) {
    QString requested = requestedLocale;
    if (requested.isEmpty()) {
        for (const char *key : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
            const QByteArray value = qgetenv(key);
            if (!value.isEmpty()) { requested = QString::fromLatin1(value); break; }
        }
    }
    return loadLocale(normalize(requested));
}

bool MonitorLocalization::loadLocale(const QString &locale) {
    QCoreApplication::removeTranslator(&translator_);
    QCoreApplication::removeTranslator(&fallback_);
    selected_ = QStringLiteral("en_US");
    QGuiApplication::setLayoutDirection(Qt::LeftToRight);
    if (!fallback_.load(QStringLiteral(":/i18n/synapse-monitor_en_US.qm"))
        || !QCoreApplication::installTranslator(&fallback_)) return false;
    if (locale != QStringLiteral("en_US")
        && translator_.load(QStringLiteral(":/i18n/synapse-monitor_%1.qm").arg(locale))
        && QCoreApplication::installTranslator(&translator_)) selected_ = locale;
    const bool rtl = selected_ == QStringLiteral("ar") || selected_ == QStringLiteral("fa")
                     || selected_ == QStringLiteral("he");
    QGuiApplication::setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
    return true;
}
