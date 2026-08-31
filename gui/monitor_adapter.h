// SPDX-License-Identifier: MIT
#ifndef SYNAPSE_MONITOR_GUI_MONITOR_ADAPTER_H
#define SYNAPSE_MONITOR_GUI_MONITOR_ADAPTER_H

#include <QAbstractListModel>
#include <QHash>
#include <QProcess>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

struct MonitorViewCapability {
    QString id;
    QString schema;
    QStringList sortIds;
    QStringList groupIds;
    QStringList columnIds;
    bool filterSupported = false;
};

struct MonitorPresentationContract {
    QHash<QString, MonitorViewCapability> views;
    QStringList orderedViews;
    qint64 maximumLineBytes = 0;
    int sampleMinimum = 0;
    int sampleMaximum = 0;
    int intervalMinimum = 0;
    int intervalMaximum = 0;
    int rowLimitMinimum = 0;
    int rowLimitMaximum = 0;
};

namespace MonitorContracts {

bool decodePresentation(const QByteArray &payload, MonitorPresentationContract *contract,
                        QString *errorId);
bool decodeFrame(const QByteArray &payload, const MonitorPresentationContract &presentation,
                 const QString &expectedView, qint64 expectedSequence,
                 int expectedIntervalMilliseconds, int rowLimit,
                 QVariantMap *frame, QVariantList *rows, QStringList *identities,
                 QString *errorId);
bool decodeInspection(const QByteArray &payload, qint64 expectedPid,
                      qint64 expectedStartTicks, QVariantMap *inspection,
                      QString *errorId);

} // namespace MonitorContracts

class MonitorRowsModel final : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles : quint16 { RowRole = Qt::UserRole + 1, IdentityRole };

    explicit MonitorRowsModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void replace(QVariantList rows, QStringList identities);
    void clear();

private:
    QVariantList rows_;
    QStringList identities_;
};

class MonitorAdapter final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool streaming READ streaming NOTIFY stateChanged)
    Q_PROPERTY(QString currentView READ currentView NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList views READ views NOTIFY capabilitiesChanged)
    Q_PROPERTY(QVariantMap payload READ payload NOTIFY frameChanged)
    Q_PROPERTY(QAbstractItemModel *rows READ rows CONSTANT)
    Q_PROPERTY(qint64 sequence READ sequence NOTIFY frameChanged)
    Q_PROPERTY(QString errorId READ errorId NOTIFY stateChanged)
    Q_PROPERTY(QString filter READ filter NOTIFY rowPresentationChanged)
    Q_PROPERTY(QString sortId READ sortId NOTIFY rowPresentationChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending NOTIFY rowPresentationChanged)
    Q_PROPERTY(QString groupId READ groupId NOTIFY selectionChanged)
    Q_PROPERTY(QStringList sortIds READ sortIds NOTIFY selectionChanged)
    Q_PROPERTY(QStringList groupIds READ groupIds NOTIFY selectionChanged)
    Q_PROPERTY(bool filterSupported READ filterSupported NOTIFY selectionChanged)
    Q_PROPERTY(int intervalMilliseconds READ intervalMilliseconds NOTIFY selectionChanged)
    Q_PROPERTY(QStringList filteredColumnIds READ filteredColumnIds
               NOTIFY rowPresentationChanged)
    Q_PROPERTY(int visibleRowCount READ visibleRowCount NOTIFY rowPresentationChanged)
    Q_PROPERTY(int sourceRowCount READ sourceRowCount NOTIFY rowPresentationChanged)
    Q_PROPERTY(QVariantMap inspection READ inspection NOTIFY inspectionChanged)
    Q_PROPERTY(bool inspectionBusy READ inspectionBusy NOTIFY inspectionChanged)
    Q_PROPERTY(QString inspectionErrorId READ inspectionErrorId NOTIFY inspectionChanged)

public:
    explicit MonitorAdapter(QString backendPath, QObject *parent = nullptr);
    ~MonitorAdapter() override;

    bool initialize(const QString &initialView = QStringLiteral("processes"));
    bool ready() const;
    bool streaming() const;
    QString currentView() const;
    QVariantList views() const;
    QVariantMap payload() const;
    QAbstractItemModel *rows();
    qint64 sequence() const;
    QString errorId() const;
    QString filter() const;
    QString sortId() const;
    bool sortAscending() const;
    QString groupId() const;
    QStringList sortIds() const;
    QStringList groupIds() const;
    bool filterSupported() const;
    int intervalMilliseconds() const;
    QStringList filteredColumnIds() const;
    int visibleRowCount() const;
    int sourceRowCount() const;
    QVariantMap inspection() const;
    bool inspectionBusy() const;
    QString inspectionErrorId() const;

    Q_INVOKABLE bool selectView(const QString &viewId);
    Q_INVOKABLE bool setFilter(const QString &filter);
    Q_INVOKABLE bool setSortId(const QString &sortId);
    Q_INVOKABLE bool requestSort(const QString &sortId);
    Q_INVOKABLE bool setGroupId(const QString &groupId);
    Q_INVOKABLE bool setIntervalMilliseconds(int milliseconds);
    Q_INVOKABLE QVariantList columnFilterOptions(const QString &columnId) const;
    Q_INVOKABLE QStringList activeColumnFilterTokens(const QString &columnId) const;
    Q_INVOKABLE bool columnFilterActive(const QString &columnId) const;
    Q_INVOKABLE bool setColumnFilter(const QString &columnId,
                                     const QStringList &tokens, bool enabled);
    Q_INVOKABLE bool clearColumnFilter(const QString &columnId);
    Q_INVOKABLE void clearAllColumnFilters();
    Q_INVOKABLE bool inspectProcess(qint64 pid, qint64 startTicks);
    Q_INVOKABLE void closeInspection();

signals:
    void stateChanged();
    void capabilitiesChanged();
    void selectionChanged();
    void frameChanged();
    void frameAccepted();
    void rowPresentationChanged();
    void inspectionChanged();

private:
    bool loadPresentation();
    bool startStream(bool preserveFrame = false);
    void stopStream();
    void consumeStreamOutput();
    void consumeStreamError();
    void failStream(const QString &errorId);
    void finishInspection(int exitCode, QProcess::ExitStatus status);
    void failInspection(const QString &errorId);
    void rebuildPresentedRows();
    bool validDisplayColumn(const QString &columnId) const;
    QString sortField(const QString &sortId) const;
    bool defaultSortAscending(const QString &sortId) const;
    MonitorViewCapability currentCapability() const;
    QString defaultSort(const QString &viewId) const;
    QString defaultGroup(const QString &viewId) const;

    static constexpr qint64 kMaximumContractLine = qint64{2} * 1024 * 1024;
    static constexpr qsizetype kMaximumErrorBytes = qsizetype{32} * 1024;
    static constexpr qsizetype kMaximumDescribeBytes = qsizetype{256} * 1024;
    static constexpr qsizetype kMaximumInspectionBytes = qsizetype{256} * 1024;
    static constexpr qsizetype kMaximumFilterOptions = 512;

    QString backendPath_;
    MonitorPresentationContract presentation_;
    QVariantList views_;
    MonitorRowsModel rows_;
    QProcess stream_;
    QProcess inspectionProcess_;
    QByteArray streamBuffer_;
    QByteArray streamErrorBuffer_;
    QByteArray inspectionOutput_;
    QByteArray inspectionError_;
    QVariantMap payload_;
    QVariantList acceptedRows_;
    QStringList acceptedIdentities_;
    QHash<QString, QSet<QString>> columnFilters_;
    mutable QHash<QString, QSet<QString>> issuedFilterTokens_;
    QVariantMap inspection_;
    QString currentView_;
    QString filter_;
    QString sortId_;
    QString groupId_;
    QString errorId_;
    QString inspectionErrorId_;
    qint64 sequence_ = -1;
    qint64 streamSequence_ = -1;
    qint64 inspectionPid_ = 0;
    qint64 inspectionStartTicks_ = 0;
    int intervalMilliseconds_ = 2000;
    int sampleMilliseconds_ = 250;
    int rowLimit_ = 512;
    bool sortAscending_ = true;
    bool ready_ = false;
    bool streaming_ = false;
    bool stopping_ = false;
    bool initialized_ = false;
    bool inspectionBusy_ = false;
};

#endif
