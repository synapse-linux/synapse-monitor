// SPDX-License-Identifier: MIT
#ifndef SYNAPSE_MONITOR_GUI_LOCALIZATION_H
#define SYNAPSE_MONITOR_GUI_LOCALIZATION_H

#include <QObject>
#include <QStringList>
#include <QTranslator>

class QQmlApplicationEngine;

class MonitorLocalization final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentLocale READ currentLocale NOTIFY currentLocaleChanged)
    Q_PROPERTY(QStringList availableLocales READ availableLocales CONSTANT)

public:
    explicit MonitorLocalization(QObject *parent = nullptr);
    QString currentLocale() const;
    QStringList availableLocales() const;
    bool initialize(const QString &requestedLocale);
    void attachEngine(QQmlApplicationEngine *engine);
    Q_INVOKABLE bool setLocale(const QString &locale);

signals:
    void currentLocaleChanged();

private:
    QString normalize(const QString &locale) const;

    QTranslator translator_;
    QString currentLocale_;
    QQmlApplicationEngine *engine_ = nullptr;
};

#endif
