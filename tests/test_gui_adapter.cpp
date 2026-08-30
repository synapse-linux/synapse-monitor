// SPDX-License-Identifier: MIT
#include "monitor_adapter.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <functional>

class MonitorGuiAdapterTest final : public QObject {
    Q_OBJECT

private:
    QString core() const {
        return QString::fromLocal8Bit(qgetenv("SYNAPSE_MONITOR_TEST_CORE"));
    }

    QByteArray run(const QStringList &arguments) const {
        QProcess process;
        process.setProgram(core());
        process.setArguments(arguments);
        process.setProcessChannelMode(QProcess::SeparateChannels);
        process.start(QIODevice::ReadOnly);
        if (!process.waitForStarted(1000) || !process.waitForFinished(10000)) return {};
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0
            || !process.readAllStandardError().isEmpty())
            return {};
        return process.readAllStandardOutput();
    }

    MonitorPresentationContract presentation() const {
        MonitorPresentationContract result;
        QString error;
        const bool decoded = MonitorContracts::decodePresentation(
            run({QStringLiteral("describe"), QStringLiteral("--format"),
                 QStringLiteral("json")}), &result, &error);
        Q_ASSERT(decoded);
        return result;
    }

    QString fakeBackend(QTemporaryDir *directory, const QByteArray &streamBody) const {
        if (!directory || !directory->isValid()) return {};
        QString path = directory->filePath(QStringLiteral("synapse-monitor"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return {};
        file.write("#!/usr/bin/env bash\nset -euo pipefail\n"
                   "if [[ ${1:-} == describe ]]; then\n"
                   "  exec \"$SYNAPSE_MONITOR_TEST_CORE\" \"$@\"\n"
                   "fi\n"
                   "if [[ ${1:-} == stream ]]; then\n");
        file.write(streamBody);
        file.write("\nfi\nexit 2\n");
        file.close();
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ExeOwner);
        return path;
    }

    QByteArray frame(const QString &view) const {
        QStringList arguments = {QStringLiteral("stream"), QStringLiteral("--view"), view,
                                 QStringLiteral("--format"), QStringLiteral("ndjson"),
                                 QStringLiteral("--interval-ms"), QStringLiteral("250"),
                                 QStringLiteral("--limit"), QStringLiteral("16"),
                                 QStringLiteral("--iterations"), QStringLiteral("1")};
        if (view == QStringLiteral("processes") || view == QStringLiteral("performance"))
            arguments.append({QStringLiteral("--sample-ms"), QStringLiteral("100")});
        return run(arguments);
    }

private slots:
    void initTestCase() {
        QVERIFY2(!core().isEmpty(), "SYNAPSE_MONITOR_TEST_CORE is required");
        const QFileInfo file(core());
        QVERIFY(file.isAbsolute());
        QVERIFY(file.isExecutable());
    }

    void presentationAcceptsExactCoreContract() {
        const MonitorPresentationContract contract = presentation();
        QCOMPARE(contract.orderedViews,
                 QStringList({QStringLiteral("processes"), QStringLiteral("performance"),
                              QStringLiteral("services"), QStringLiteral("startup"),
                              QStringLiteral("connections"), QStringLiteral("information")}));
        QCOMPARE(contract.maximumLineBytes, qint64(2 * 1024 * 1024));
        QCOMPARE(contract.views.value(QStringLiteral("performance")).schema,
                 QStringLiteral("synapse.monitor.performance/v2"));
        QVERIFY(contract.views.value(QStringLiteral("processes")).filterSupported);
        QCOMPARE(contract.views.value(QStringLiteral("processes")).sortIds,
                 QStringList({QStringLiteral("cpu"), QStringLiteral("memory"),
                              QStringLiteral("read"), QStringLiteral("write"),
                              QStringLiteral("name"), QStringLiteral("class"),
                              QStringLiteral("pid"), QStringLiteral("user"),
                              QStringLiteral("state"), QStringLiteral("threads")}));
        QVERIFY(contract.views.value(QStringLiteral("startup")).sortIds
                    .contains(QStringLiteral("location")));
        QVERIFY(!contract.views.value(QStringLiteral("information")).filterSupported);
    }

    void undeclaredSortIdentifierFailsClosed() {
        QJsonObject object = QJsonDocument::fromJson(
            run({QStringLiteral("describe"), QStringLiteral("--format"),
                 QStringLiteral("json")})).object();
        QJsonArray views = object.value(QStringLiteral("views")).toArray();
        QJsonObject processes = views.at(0).toObject();
        QJsonArray sorts = processes.value(QStringLiteral("sort")).toArray();
        sorts.append(QStringLiteral("unreviewed"));
        processes.insert(QStringLiteral("sort"), sorts);
        views.replace(0, processes);
        object.insert(QStringLiteral("views"), views);
        MonitorPresentationContract contract;
        QString error;
        QVERIFY(!MonitorContracts::decodePresentation(
            QJsonDocument(object).toJson(QJsonDocument::Compact), &contract, &error));
        QCOMPARE(error, QStringLiteral("presentation-invalid"));
    }

    void eachViewAcceptsOneExactFrame() {
        const MonitorPresentationContract contract = presentation();
        for (const QString &view : contract.orderedViews) {
            QVariantMap payload;
            QVariantList rows;
            QStringList identities;
            QString error;
            const QByteArray wire = frame(view).trimmed();
            QVERIFY2(!wire.isEmpty(), qPrintable(view));
            QVERIFY2(MonitorContracts::decodeFrame(wire, contract, view, 0, 250, 16,
                                                    &payload, &rows, &identities, &error),
                     qPrintable(view + QLatin1Char(':') + error));
            QCOMPARE(payload.value(QStringLiteral("view")).toString(), view);
            if (view != QStringLiteral("performance")
                && view != QStringLiteral("information"))
                QCOMPARE(rows.size(), identities.size());
        }
    }

    void wrongPresentationMajorFailsClosed() {
        QJsonDocument document = QJsonDocument::fromJson(
            run({QStringLiteral("describe"), QStringLiteral("--format"),
                 QStringLiteral("json")}));
        QJsonObject object = document.object();
        object.insert(QStringLiteral("schema"), QStringLiteral("synapse.monitor.presentation/v2"));
        MonitorPresentationContract contract;
        QString error;
        QVERIFY(!MonitorContracts::decodePresentation(document.object().isEmpty()
                                                           ? QByteArray()
                                                           : QJsonDocument(object).toJson(QJsonDocument::Compact),
                                                       &contract, &error));
        QCOMPARE(error, QStringLiteral("presentation-invalid"));
    }

    void mutationAuthorityInPresentationFailsClosed() {
        QJsonObject object = QJsonDocument::fromJson(
            run({QStringLiteral("describe"), QStringLiteral("--format"),
                 QStringLiteral("json")})).object();
        QJsonObject authority = object.value(QStringLiteral("authority")).toObject();
        authority.insert(QStringLiteral("processMutation"), true);
        object.insert(QStringLiteral("authority"), authority);
        MonitorPresentationContract contract;
        QString error;
        QVERIFY(!MonitorContracts::decodePresentation(
            QJsonDocument(object).toJson(QJsonDocument::Compact), &contract, &error));
        QCOMPARE(error, QStringLiteral("presentation-invalid"));
    }

    void oversizedFrameFailsClosed() {
        const MonitorPresentationContract contract = presentation();
        QVariantMap payload;
        QVariantList rows;
        QStringList identities;
        QString error;
        const QByteArray oversized(2 * 1024 * 1024 + 1, 'x');
        QVERIFY(!MonitorContracts::decodeFrame(oversized, contract,
                                                QStringLiteral("processes"), 0, 250,
                                                16, &payload, &rows, &identities, &error));
        QCOMPARE(error, QStringLiteral("stream-invalid"));
    }

    void sequenceGapFailsClosed() {
        const MonitorPresentationContract contract = presentation();
        QVariantMap payload;
        QVariantList rows;
        QStringList identities;
        QString error;
        QVERIFY(!MonitorContracts::decodeFrame(frame(QStringLiteral("processes")).trimmed(),
                                                contract, QStringLiteral("processes"), 1,
                                                250, 16, &payload, &rows, &identities, &error));
        QCOMPARE(error, QStringLiteral("stream-sequence-invalid"));
    }

    void mutationSemanticsInFrameFailsClosed() {
        const MonitorPresentationContract contract = presentation();
        QJsonObject object = QJsonDocument::fromJson(frame(QStringLiteral("processes"))).object();
        QJsonObject semantics = object.value(QStringLiteral("semantics")).toObject();
        semantics.insert(QStringLiteral("processControl"), true);
        object.insert(QStringLiteral("semantics"), semantics);
        QVariantMap payload;
        QVariantList rows;
        QStringList identities;
        QString error;
        QVERIFY(!MonitorContracts::decodeFrame(QJsonDocument(object).toJson(QJsonDocument::Compact),
                                                contract, QStringLiteral("processes"), 0,
                                                250, 16, &payload, &rows, &identities, &error));
        QCOMPARE(error, QStringLiteral("stream-invalid"));
    }

    void duplicateStableIdentityFailsClosed() {
        const MonitorPresentationContract contract = presentation();
        QJsonObject object = QJsonDocument::fromJson(frame(QStringLiteral("processes"))).object();
        QJsonArray rowsJson = object.value(QStringLiteral("rows")).toArray();
        QVERIFY(!rowsJson.isEmpty());
        rowsJson.append(rowsJson.first());
        object.insert(QStringLiteral("rows"), rowsJson);
        QVariantMap payload;
        QVariantList rows;
        QStringList identities;
        QString error;
        QVERIFY(!MonitorContracts::decodeFrame(QJsonDocument(object).toJson(QJsonDocument::Compact),
                                                contract, QStringLiteral("processes"), 0,
                                                250, 32, &payload, &rows, &identities, &error));
        QCOMPARE(error, QStringLiteral("stream-invalid"));
    }

    void unavailableHistoryRemainsDistinctFromZero() {
        const MonitorPresentationContract contract = presentation();
        QJsonObject object = QJsonDocument::fromJson(frame(QStringLiteral("performance"))).object();
        QJsonObject history = object.value(QStringLiteral("history")).toObject();
        const QStringList keys = {QStringLiteral("cpuPercentMilli"),
                                  QStringLiteral("memoryPercentMilli"),
                                  QStringLiteral("gpuPercentMilli"),
                                  QStringLiteral("diskBytesPerSecond"),
                                  QStringLiteral("networkBytesPerSecond")};
        for (const QString &key : keys)
            history.insert(key, QJsonArray({QJsonValue(QJsonValue::Null), 0}));
        object.insert(QStringLiteral("history"), history);
        QVariantMap payload;
        QVariantList rows;
        QStringList identities;
        QString error;
        QVERIFY(MonitorContracts::decodeFrame(QJsonDocument(object).toJson(QJsonDocument::Compact),
                                               contract, QStringLiteral("performance"), 0,
                                               250, 16, &payload, &rows, &identities, &error));
        const QVariantList values = payload.value(QStringLiteral("history")).toMap()
                                        .value(QStringLiteral("gpuPercentMilli")).toList();
        QCOMPARE(values.size(), 2);
        QVERIFY(values.at(0).isNull());
        QCOMPARE(values.at(1).toInt(), 0);
    }

    void fabricatedGpuMemoryKindFailsClosed() {
        const MonitorPresentationContract contract = presentation();
        QJsonObject object = QJsonDocument::fromJson(
            frame(QStringLiteral("performance"))).object();
        QJsonObject gpu = object.value(QStringLiteral("gpu")).toObject();
        gpu.insert(QStringLiteral("memoryKind"), QStringLiteral("dedicated"));
        object.insert(QStringLiteral("gpu"), gpu);
        QVariantMap payload;
        QVariantList rows;
        QStringList identities;
        QString error;
        QVERIFY(!MonitorContracts::decodeFrame(
            QJsonDocument(object).toJson(QJsonDocument::Compact), contract,
            QStringLiteral("performance"), 0, 250, 16,
            &payload, &rows, &identities, &error));
        QCOMPARE(error, QStringLiteral("stream-invalid"));
    }

    void inspectionRevalidatesPidAndStartTicks() {
        const qint64 pid = QCoreApplication::applicationPid();
        const QByteArray wire = run({QStringLiteral("inspect"), QStringLiteral("--pid"),
                                     QString::number(pid), QStringLiteral("--format"),
                                     QStringLiteral("json")});
        const QJsonObject identity = QJsonDocument::fromJson(wire).object()
                                         .value(QStringLiteral("identity")).toObject();
        const qint64 start = identity.value(QStringLiteral("startTicks")).toInteger();
        QVERIFY(start > 0);
        QVariantMap inspection;
        QString error;
        QVERIFY(MonitorContracts::decodeInspection(wire, pid, start, &inspection, &error));
        QVERIFY(!MonitorContracts::decodeInspection(wire, pid, start + 1,
                                                     &inspection, &error));
        QCOMPARE(error, QStringLiteral("inspection-identity-changed"));
    }

    void rowsModelExposesOnlyTypedRowsAndIdentity() {
        MonitorRowsModel model;
        model.replace({QVariantMap({{QStringLiteral("name"), QStringLiteral("alpha")}})},
                      {QStringLiteral("1/2")});
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0), MonitorRowsModel::IdentityRole).toString(),
                 QStringLiteral("1/2"));
        QCOMPARE(model.data(model.index(0), MonitorRowsModel::RowRole).toMap()
                     .value(QStringLiteral("name")).toString(), QStringLiteral("alpha"));
        model.clear();
        QCOMPARE(model.rowCount(), 0);
    }

    void oversizedLiveLineTerminatesChild() {
        QTemporaryDir directory;
        const QString backend = fakeBackend(
            &directory,
            QByteArrayLiteral("  head -c 2097153 /dev/zero | tr '\\\\0' x\n  sleep 5"));
        QVERIFY(!backend.isEmpty());
        MonitorAdapter adapter(backend);
        QVERIFY(adapter.initialize(QStringLiteral("processes")));
        QTRY_COMPARE_WITH_TIMEOUT(adapter.errorId(),
                                  QStringLiteral("stream-line-too-large"), 5000);
        QVERIFY(!adapter.streaming());
        QVERIFY(!adapter.ready());
    }

    void boundedStderrTerminatesChild() {
        QTemporaryDir directory;
        const QString backend = fakeBackend(
            &directory,
            QByteArrayLiteral("  printf 'synthetic failure\\n' >&2\n  sleep 5"));
        QVERIFY(!backend.isEmpty());
        MonitorAdapter adapter(backend);
        QVERIFY(adapter.initialize(QStringLiteral("processes")));
        QTRY_COMPARE_WITH_TIMEOUT(adapter.errorId(), QStringLiteral("stream-failed"),
                                  5000);
        QVERIFY(!adapter.streaming());
        QVERIFY(!adapter.ready());
    }

    void displayedHeaderSortsAreContinuousAndDirectionToggles() {
        MonitorAdapter adapter(core());
        QSignalSpy accepted(&adapter, &MonitorAdapter::frameAccepted);
        QSignalSpy stateChanged(&adapter, &MonitorAdapter::stateChanged);
        QVERIFY(adapter.initialize(QStringLiteral("processes")));
        QTRY_VERIFY_WITH_TIMEOUT(accepted.count() > 0, 5000);
        QCOMPARE(adapter.sortIds().size(), 10);
        QVERIFY(adapter.ready());
        QVERIFY(adapter.streaming());
        QVERIFY(adapter.sourceRowCount() > 1);
        accepted.clear();
        stateChanged.clear();

        QVERIFY(adapter.setSortId(QStringLiteral("pid")));
        QCOMPARE(adapter.sortId(), QStringLiteral("pid"));
        QVERIFY(adapter.sortAscending());
        QCOMPARE(adapter.visibleRowCount(), adapter.sourceRowCount());
        QList<qint64> ascending;
        for (int index = 0; index < adapter.rows()->rowCount(); ++index)
            ascending.append(adapter.rows()->data(adapter.rows()->index(index, 0),
                                                   MonitorRowsModel::RowRole).toMap()
                                 .value(QStringLiteral("pid")).toLongLong());
        QVERIFY(std::is_sorted(ascending.cbegin(), ascending.cend()));

        QVERIFY(adapter.requestSort(QStringLiteral("pid")));
        QVERIFY(!adapter.sortAscending());
        QList<qint64> descending;
        for (int index = 0; index < adapter.rows()->rowCount(); ++index)
            descending.append(adapter.rows()->data(adapter.rows()->index(index, 0),
                                                    MonitorRowsModel::RowRole).toMap()
                                  .value(QStringLiteral("pid")).toLongLong());
        QVERIFY(std::is_sorted(descending.cbegin(), descending.cend(),
                               std::greater<qint64>()));
        QCOMPARE(accepted.count(), 0);
        QCOMPARE(stateChanged.count(), 0);
        QVERIFY(adapter.ready());
        QVERIFY(adapter.streaming());

        QVERIFY(adapter.setSortId(QStringLiteral("class")));
        QCOMPARE(adapter.payload().value(QStringLiteral("selection")).toMap()
                     .value(QStringLiteral("sort")).toString(),
                 QStringLiteral("cpu"));
        QCOMPARE(adapter.sortId(), QStringLiteral("class"));
    }

    void excelStyleColumnFiltersStayTypedAndLocal() {
        MonitorAdapter adapter(core());
        QSignalSpy accepted(&adapter, &MonitorAdapter::frameAccepted);
        QSignalSpy stateChanged(&adapter, &MonitorAdapter::stateChanged);
        QVERIFY(adapter.initialize(QStringLiteral("processes")));
        QTRY_VERIFY_WITH_TIMEOUT(accepted.count() > 0, 5000);
        const int sourceRows = adapter.sourceRowCount();
        QVERIFY(sourceRows > 1);
        const QVariantList options = adapter.columnFilterOptions(QStringLiteral("name"));
        QVERIFY(options.size() > 1);
        for (const QVariant &option : options) {
            const QVariantMap value = option.toMap();
            QCOMPARE(value.value(QStringLiteral("token")).toString().size(), 64);
            QVERIFY(value.value(QStringLiteral("count")).toInt() > 0);
        }
        accepted.clear();
        stateChanged.clear();
        const QString selected = options.first().toMap()
                                     .value(QStringLiteral("token")).toString();
        QVERIFY(adapter.setColumnFilter(QStringLiteral("name"), {selected}, true));
        QCOMPARE(adapter.filteredColumnIds(), QStringList({QStringLiteral("name")}));
        QVERIFY(adapter.visibleRowCount() > 0);
        QVERIFY(adapter.visibleRowCount() < sourceRows);
        QCOMPARE(adapter.sourceRowCount(), sourceRows);
        QCOMPARE(accepted.count(), 0);
        QCOMPARE(stateChanged.count(), 0);
        QVERIFY(adapter.ready());
        QVERIFY(adapter.streaming());

        QVERIFY(adapter.setColumnFilter(QStringLiteral("name"), {}, true));
        QVERIFY(adapter.columnFilterActive(QStringLiteral("name")));
        QCOMPARE(adapter.visibleRowCount(), 0);
        QVERIFY(adapter.clearColumnFilter(QStringLiteral("name")));
        QCOMPARE(adapter.visibleRowCount(), sourceRows);
        QVERIFY(adapter.filteredColumnIds().isEmpty());
        QVERIFY(!adapter.setColumnFilter(QStringLiteral("name"),
                                         {QString(64, QLatin1Char('0'))}, true));
        QCOMPARE(adapter.errorId(), QStringLiteral("column-filter-invalid"));
    }

    void unavailableValuesRemainLastInBothDirections() {
        QJsonObject object = QJsonDocument::fromJson(
            frame(QStringLiteral("connections"))).object();
        QVERIFY(!object.isEmpty());
        QJsonObject stream = object.value(QStringLiteral("stream")).toObject();
        stream.insert(QStringLiteral("intervalMilliseconds"), 1000);
        stream.insert(QStringLiteral("sequence"), 0);
        object.insert(QStringLiteral("stream"), stream);
        const auto connection = [](qint64 inode, const QJsonValue &pid,
                                   const QString &process) {
            QJsonObject row;
            row.insert(QStringLiteral("protocol"), QStringLiteral("tcp"));
            row.insert(QStringLiteral("local"), QStringLiteral("127.0.0.1:1"));
            row.insert(QStringLiteral("remote"), QStringLiteral("0.0.0.0:0"));
            row.insert(QStringLiteral("state"), QStringLiteral("listen"));
            row.insert(QStringLiteral("socketInode"), inode);
            row.insert(QStringLiteral("pid"), pid);
            row.insert(QStringLiteral("process"), process);
            return row;
        };
        object.insert(QStringLiteral("rows"),
                      QJsonArray({connection(101, QJsonValue(QJsonValue::Null),
                                             QStringLiteral("unavailable")),
                                  connection(102, 2, QStringLiteral("two")),
                                  connection(103, 1, QStringLiteral("one"))}));
        const QByteArray wire = QJsonDocument(object).toJson(QJsonDocument::Compact);
        QVERIFY(!wire.contains('\''));
        QTemporaryDir directory;
        const QString backend = fakeBackend(
            &directory, QByteArrayLiteral("  printf '%s\\n' '") + wire
                            + QByteArrayLiteral("'\n  sleep 5"));
        QVERIFY(!backend.isEmpty());
        MonitorAdapter adapter(backend);
        QSignalSpy accepted(&adapter, &MonitorAdapter::frameAccepted);
        QVERIFY(adapter.initialize(QStringLiteral("connections")));
        QTRY_VERIFY_WITH_TIMEOUT(accepted.count() > 0, 5000);
        QCOMPARE(adapter.sourceRowCount(), 3);

        QVERIFY(adapter.setSortId(QStringLiteral("pid")));
        const auto pidAt = [&adapter](int index) {
            return adapter.rows()->data(adapter.rows()->index(index, 0),
                                        MonitorRowsModel::RowRole).toMap()
                .value(QStringLiteral("pid"));
        };
        QCOMPARE(pidAt(0).toLongLong(), qint64(1));
        QCOMPARE(pidAt(1).toLongLong(), qint64(2));
        QVERIFY(pidAt(2).isNull());
        QVERIFY(adapter.requestSort(QStringLiteral("pid")));
        QCOMPARE(pidAt(0).toLongLong(), qint64(2));
        QCOMPARE(pidAt(1).toLongLong(), qint64(1));
        QVERIFY(pidAt(2).isNull());

        const QVariantList options = adapter.columnFilterOptions(QStringLiteral("pid"));
        QCOMPARE(options.size(), 3);
        QString unavailableToken;
        for (const QVariant &entry : options) {
            const QVariantMap option = entry.toMap();
            if (option.value(QStringLiteral("unavailable")).toBool())
                unavailableToken = option.value(QStringLiteral("token")).toString();
        }
        QCOMPARE(unavailableToken.size(), 64);
        QVERIFY(adapter.setColumnFilter(QStringLiteral("pid"),
                                        {unavailableToken}, true));
        QCOMPARE(adapter.visibleRowCount(), 1);
        QVERIFY(pidAt(0).isNull());
        QVERIFY(adapter.ready());
        QVERIFY(adapter.streaming());
    }

    void liveAdapterOwnsAndStopsItsChild() {
        MonitorAdapter adapter(core());
        QSignalSpy accepted(&adapter, &MonitorAdapter::frameAccepted);
        QVERIFY(adapter.initialize(QStringLiteral("processes")));
        QVERIFY(accepted.wait(5000));
        QVERIFY(adapter.ready());
        QVERIFY(adapter.streaming());
        QCOMPARE(adapter.sequence(), qint64(0));
        QVERIFY(adapter.rows()->rowCount() > 0);
        const QVariantMap first = adapter.rows()->data(adapter.rows()->index(0, 0),
                                                        MonitorRowsModel::RowRole).toMap();
        QSignalSpy inspected(&adapter, &MonitorAdapter::inspectionChanged);
        QVERIFY(adapter.inspectProcess(first.value(QStringLiteral("pid")).toLongLong(),
                                       first.value(QStringLiteral("startTicks")).toLongLong()));
        QTRY_VERIFY_WITH_TIMEOUT(!adapter.inspectionBusy()
                                 && !adapter.inspection().isEmpty(), 5000);
        QVERIFY(inspected.count() >= 1);
        QCOMPARE(adapter.inspection().value(QStringLiteral("identity")).toMap()
                     .value(QStringLiteral("pid")).toLongLong(),
                 first.value(QStringLiteral("pid")).toLongLong());
        const QVariantMap inspectionBeforeSort = adapter.inspection();
        QVERIFY(adapter.setSortId(QStringLiteral("pid")));
        QVERIFY(adapter.requestSort(QStringLiteral("pid")));
        QCOMPARE(adapter.inspection(), inspectionBeforeSort);
        adapter.closeInspection();
        QVERIFY(!adapter.setFilter(QString::fromUtf8("caffè")));
        QCOMPARE(adapter.errorId(), QStringLiteral("filter-invalid"));
    }
};

QTEST_GUILESS_MAIN(MonitorGuiAdapterTest)
#include "test_gui_adapter.moc"
