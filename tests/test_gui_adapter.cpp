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
        const QString path = directory->filePath(QStringLiteral("synapse-monitor"));
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
        QVERIFY(!contract.views.value(QStringLiteral("information")).filterSupported);
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
        adapter.closeInspection();
        QVERIFY(!adapter.setFilter(QString::fromUtf8("caffè")));
        QCOMPARE(adapter.errorId(), QStringLiteral("filter-invalid"));
    }
};

QTEST_GUILESS_MAIN(MonitorGuiAdapterTest)
#include "test_gui_adapter.moc"
