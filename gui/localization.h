// SPDX-License-Identifier: MIT
#ifndef SYNAPSE_MONITOR_GUI_LOCALIZATION_H
#define SYNAPSE_MONITOR_GUI_LOCALIZATION_H

#include <QObject>
#include <QString>
#include <QTranslator>

class MonitorLocalization final : public QObject {
    Q_OBJECT

public:
    explicit MonitorLocalization(QObject *parent = nullptr);
    bool initialize(const QString &requestedLocale);
    QString selectedLocale() const { return selected_; }

private:
    QString normalize(const QString &locale) const;
    bool loadLocale(const QString &locale);

    QTranslator fallback_;
    QTranslator translator_;
    QString selected_;
};

#endif
