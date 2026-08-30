// SPDX-License-Identifier: MIT
#include "monitor_adapter.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

bool fail(QString *errorId, const QString &value) {
    if (errorId) *errorId = value;
    return false;
}

bool integerValue(const QJsonValue &value, qint64 minimum, qint64 *result = nullptr) {
    if (!value.isDouble()) return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number
        || number < static_cast<double>(minimum)
        || number > 9007199254740991.0)
        return false;
    if (result) *result = static_cast<qint64>(number);
    return true;
}

bool oneJsonObject(QByteArray payload, qsizetype maximumBytes, QJsonObject *object,
                   QString *errorId) {
    if (payload.isEmpty() || payload.size() > maximumBytes || payload.contains('\0'))
        return fail(errorId, QStringLiteral("contract-invalid"));
    if (payload.endsWith('\n')) payload.chop(1);
    if (payload.isEmpty() || payload.contains('\n') || payload.contains('\r'))
        return fail(errorId, QStringLiteral("contract-invalid"));
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(errorId, QStringLiteral("contract-invalid"));
    *object = document.object();
    return true;
}

bool stringArray(const QJsonValue &value, QStringList *output, int maximum = 64) {
    if (!value.isArray()) return false;
    const QJsonArray array = value.toArray();
    if (array.size() > maximum) return false;
    QStringList values;
    QSet<QString> seen;
    for (const auto &entry : array) {
        if (!entry.isString()) return false;
        const QString text = entry.toString();
        if (text.isEmpty() || text.size() > 64 || seen.contains(text)) return false;
        seen.insert(text);
        values.append(text);
    }
    *output = values;
    return true;
}

bool requiredFalse(const QJsonObject &object, const QStringList &required) {
    for (const QString &key : required) {
        if (!object.contains(key) || !object.value(key).isBool() || object.value(key).toBool())
            return false;
    }
    return true;
}

bool allFalse(const QJsonObject &object, const QStringList &required) {
    if (!requiredFalse(object, required)) return false;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (it.value().isBool() && it.value().toBool()) return false;
    }
    return true;
}

bool boundedHistory(const QJsonObject &history) {
    const QStringList keys = {QStringLiteral("cpuPercentMilli"),
                              QStringLiteral("memoryPercentMilli"),
                              QStringLiteral("gpuPercentMilli"),
                              QStringLiteral("diskBytesPerSecond"),
                              QStringLiteral("networkBytesPerSecond")};
    qsizetype length = -1;
    for (const QString &key : keys) {
        if (!history.value(key).isArray()) return false;
        const QJsonArray values = history.value(key).toArray();
        if (values.size() > 60 || (length >= 0 && values.size() != length)) return false;
        length = values.size();
        for (const auto &value : values)
            if (!value.isNull() && !value.isDouble()) return false;
    }
    return true;
}

bool uniqueRows(const QJsonArray &rows, const QString &view, int rowLimit,
                QVariantList *variantRows, QStringList *identities) {
    if (rows.size() > rowLimit) return false;
    QVariantList output;
    QStringList keys;
    QSet<QString> seen;
    output.reserve(rows.size());
    keys.reserve(rows.size());
    for (const auto &entry : rows) {
        if (!entry.isObject()) return false;
        const QJsonObject row = entry.toObject();
        QString identity;
        if (view == QStringLiteral("processes")) {
            qint64 pid = 0;
            qint64 start = 0;
            if (!integerValue(row.value(QStringLiteral("pid")), 1, &pid)
                || !integerValue(row.value(QStringLiteral("startTicks")), 1, &start)
                || !row.value(QStringLiteral("name")).isString())
                return false;
            identity = QString::number(pid) + QLatin1Char('/') + QString::number(start);
        } else if (view == QStringLiteral("services")) {
            if (!row.value(QStringLiteral("name")).isString()) return false;
            identity = row.value(QStringLiteral("name")).toString();
        } else if (view == QStringLiteral("startup")) {
            if (!row.value(QStringLiteral("id")).isString()) return false;
            identity = row.value(QStringLiteral("id")).toString();
        } else if (view == QStringLiteral("connections")) {
            qint64 inode = 0;
            if (!integerValue(row.value(QStringLiteral("socketInode")), 1, &inode)) return false;
            identity = QString::number(inode);
        } else {
            return false;
        }
        if (identity.isEmpty() || identity.size() > 256 || seen.contains(identity)) return false;
        seen.insert(identity);
        keys.append(identity);
        output.append(row.toVariantMap());
    }
    *variantRows = std::move(output);
    *identities = std::move(keys);
    return true;
}

bool validPerformance(const QJsonObject &root) {
    if (!root.value(QStringLiteral("cpu")).isObject()
        || !root.value(QStringLiteral("memory")).isObject()
        || !root.value(QStringLiteral("gpus")).isObject()
        || !root.value(QStringLiteral("thermals")).isObject()
        || !root.value(QStringLiteral("disks")).isObject()
        || !root.value(QStringLiteral("network")).isObject()
        || !root.value(QStringLiteral("history")).isObject())
        return false;
    const QJsonObject cpu = root.value(QStringLiteral("cpu")).toObject();
    const QJsonObject gpus = root.value(QStringLiteral("gpus")).toObject();
    const QJsonObject thermals = root.value(QStringLiteral("thermals")).toObject();
    const QJsonObject disks = root.value(QStringLiteral("disks")).toObject();
    const QJsonObject network = root.value(QStringLiteral("network")).toObject();
    if (!cpu.value(QStringLiteral("logicalProcessors")).isArray()
        || cpu.value(QStringLiteral("logicalProcessors")).toArray().size() > 512
        || !gpus.value(QStringLiteral("rows")).isArray()
        || gpus.value(QStringLiteral("rows")).toArray().size() > 32
        || !thermals.value(QStringLiteral("temperatures")).isArray()
        || thermals.value(QStringLiteral("temperatures")).toArray().size() > 256
        || !thermals.value(QStringLiteral("fans")).isArray()
        || thermals.value(QStringLiteral("fans")).toArray().size() > 128
        || !disks.value(QStringLiteral("rows")).isArray()
        || disks.value(QStringLiteral("rows")).toArray().size() > 128
        || !network.value(QStringLiteral("rows")).isArray()
        || network.value(QStringLiteral("rows")).toArray().size() > 128
        || gpus.value(QStringLiteral("integratedGpuTemperatureInferred")).toBool(true))
        return false;
    return boundedHistory(root.value(QStringLiteral("history")).toObject());
}

bool validFrameSemantics(const QJsonObject &root, const QString &view) {
    if (!root.value(QStringLiteral("semantics")).isObject()) return false;
    const QJsonObject semantics = root.value(QStringLiteral("semantics")).toObject();
    if (view == QStringLiteral("processes"))
        return requiredFalse(semantics, {QStringLiteral("processControl"),
                                    QStringLiteral("commandLinesExposed"),
                                    QStringLiteral("pathsExposed")});
    if (view == QStringLiteral("performance"))
        return requiredFalse(semantics, {QStringLiteral("integratedGpuTemperatureInferred"),
                                    QStringLiteral("telemetry")});
    if (view == QStringLiteral("services"))
        return requiredFalse(semantics, {QStringLiteral("serviceMutation"),
                                    QStringLiteral("pathsExposed"),
                                    QStringLiteral("unitFileContentExposed")});
    if (view == QStringLiteral("startup"))
        return requiredFalse(semantics, {QStringLiteral("startupMutation"),
                                    QStringLiteral("rawLaunchCommandsExposed"),
                                    QStringLiteral("fullLocationsExposed")});
    if (view == QStringLiteral("connections"))
        return requiredFalse(semantics, {QStringLiteral("socketOpened"),
                                    QStringLiteral("connectionControl")});
    if (view == QStringLiteral("information"))
        return requiredFalse(semantics, {QStringLiteral("hostNameExposed"),
                                    QStringLiteral("serialNumbersExposed"),
                                    QStringLiteral("pathsExposed")});
    return false;
}

bool printableFilter(const QString &filter) {
    if (filter.toUtf8().size() > 64) return false;
    for (const QChar character : filter)
        if (character.unicode() < 0x20 || character.unicode() > 0x7e) return false;
    return true;
}

} // namespace

namespace MonitorContracts {

bool decodePresentation(const QByteArray &payload, MonitorPresentationContract *contract,
                        QString *errorId) {
    if (!contract) return fail(errorId, QStringLiteral("presentation-invalid"));
    QJsonObject root;
    if (!oneJsonObject(payload, qsizetype{256} * 1024, &root, errorId))
        return fail(errorId, QStringLiteral("presentation-invalid"));
    if (root.value(QStringLiteral("schema")).toString()
            != QStringLiteral("synapse.monitor.presentation/v1")
        || !root.value(QStringLiteral("readOnly")).toBool()
        || !root.value(QStringLiteral("producer")).isObject()
        || !root.value(QStringLiteral("views")).isArray()
        || !root.value(QStringLiteral("formats")).isObject()
        || !root.value(QStringLiteral("sampling")).isObject()
        || !root.value(QStringLiteral("history")).isObject()
        || !root.value(QStringLiteral("availability")).isObject()
        || !root.value(QStringLiteral("localization")).isObject()
        || !root.value(QStringLiteral("inspection")).isObject()
        || !root.value(QStringLiteral("authority")).isObject()
        || !root.value(QStringLiteral("privacy")).isObject())
        return fail(errorId, QStringLiteral("presentation-invalid"));
    const QJsonObject producer = root.value(QStringLiteral("producer")).toObject();
    if (producer.value(QStringLiteral("id")).toString() != QStringLiteral("synapse-monitor")
        || !producer.value(QStringLiteral("version")).isString()
        || producer.value(QStringLiteral("version")).toString().isEmpty()
        || producer.value(QStringLiteral("version")).toString().size() > 64)
        return fail(errorId, QStringLiteral("presentation-invalid"));

    const QStringList authorityKeys = {
        QStringLiteral("processMutation"), QStringLiteral("serviceMutation"),
        QStringLiteral("startupMutation"), QStringLiteral("connectionControl"),
        QStringLiteral("gpuControl"), QStringLiteral("thermalControl"),
        QStringLiteral("privilegedHelper"), QStringLiteral("listener"),
        QStringLiteral("telemetry")};
    const QStringList privacyKeys = {
        QStringLiteral("commandLinesExposed"), QStringLiteral("environmentsExposed"),
        QStringLiteral("fullPathsExposed"), QStringLiteral("authenticationCredentialsExposed"),
        QStringLiteral("serialNumbersExposed"), QStringLiteral("hostNameExposed")};
    if (!allFalse(root.value(QStringLiteral("authority")).toObject(), authorityKeys)
        || !allFalse(root.value(QStringLiteral("privacy")).toObject(), privacyKeys))
        return fail(errorId, QStringLiteral("presentation-invalid"));

    const QJsonObject formats = root.value(QStringLiteral("formats")).toObject();
    const QJsonObject stream = formats.value(QStringLiteral("stream")).toObject();
    qint64 maximumLine = 0;
    qint64 sequenceStart = -1;
    if (stream.value(QStringLiteral("id")).toString() != QStringLiteral("ndjson")
        || stream.value(QStringLiteral("mediaType")).toString()
            != QStringLiteral("application/x-ndjson")
        || stream.value(QStringLiteral("framing")).toString()
            != QStringLiteral("one-complete-object-per-line")
        || stream.value(QStringLiteral("payload")).toString()
            != QStringLiteral("complete-view-snapshot")
        || stream.value(QStringLiteral("streamMetadataSchema")).toString()
            != QStringLiteral("synapse.monitor.stream-frame/v1")
        || stream.value(QStringLiteral("backpressure")).toString()
            != QStringLiteral("blocking")
        || stream.value(QStringLiteral("errorChannel")).toString()
            != QStringLiteral("stderr")
        || !integerValue(stream.value(QStringLiteral("sequenceStartsAt")), 0,
                         &sequenceStart)
        || sequenceStart != 0
        || !integerValue(stream.value(QStringLiteral("maximumLineBytes")), 1, &maximumLine)
        || maximumLine != qint64{2} * 1024 * 1024)
        return fail(errorId, QStringLiteral("presentation-invalid"));
    const QJsonObject history = root.value(QStringLiteral("history")).toObject();
    qint64 historyMaximum = 0;
    if (!integerValue(history.value(QStringLiteral("maximumSamples")), 1,
                      &historyMaximum)
        || historyMaximum != 60
        || history.value(QStringLiteral("ordering")).toString()
            != QStringLiteral("oldest-to-newest")
        || !history.value(QStringLiteral("unavailableSample")).isNull()
        || !history.value(QStringLiteral("measuredZeroDistinctFromUnavailable")).toBool())
        return fail(errorId, QStringLiteral("presentation-invalid"));
    const QJsonObject availability = root.value(QStringLiteral("availability")).toObject();
    if (!availability.value(QStringLiteral("unavailableNumeric")).isNull()
        || availability.value(QStringLiteral("measuredZero")).toInt(-1) != 0
        || !availability.value(QStringLiteral("partialRowsRetained")).toBool())
        return fail(errorId, QStringLiteral("presentation-invalid"));
    const QJsonObject localization = root.value(QStringLiteral("localization")).toObject();
    if (localization.value(QStringLiteral("identifiers")).toString()
            != QStringLiteral("locale-neutral")
        || !localization.value(QStringLiteral("humanLabelsOwnedByGui")).toBool())
        return fail(errorId, QStringLiteral("presentation-invalid"));
    const QJsonObject inspection = root.value(QStringLiteral("inspection")).toObject();
    QStringList inspectionIdentity;
    if (inspection.value(QStringLiteral("schema")).toString()
            != QStringLiteral("synapse.monitor.process-inspection/v1")
        || !stringArray(inspection.value(QStringLiteral("identity")),
                        &inspectionIdentity)
        || inspectionIdentity != QStringList({QStringLiteral("pid"),
                                              QStringLiteral("startTicks")})
        || inspection.value(QStringLiteral("streamable")).toBool(true))
        return fail(errorId, QStringLiteral("presentation-invalid"));

    const QStringList expectedViews = {
        QStringLiteral("processes"), QStringLiteral("performance"),
        QStringLiteral("services"), QStringLiteral("startup"),
        QStringLiteral("connections"), QStringLiteral("information")};
    const QHash<QString, QString> expectedSchemas = {
        {QStringLiteral("processes"), QStringLiteral("synapse.monitor.snapshot/v1")},
        {QStringLiteral("performance"), QStringLiteral("synapse.monitor.performance/v2")},
        {QStringLiteral("services"), QStringLiteral("synapse.monitor.services/v1")},
        {QStringLiteral("startup"), QStringLiteral("synapse.monitor.startup/v1")},
        {QStringLiteral("connections"), QStringLiteral("synapse.monitor.connections/v1")},
        {QStringLiteral("information"), QStringLiteral("synapse.monitor.information/v1")}};
    const QHash<QString, QStringList> expectedIdentities = {
        {QStringLiteral("processes"), {QStringLiteral("pid"), QStringLiteral("startTicks")}},
        {QStringLiteral("performance"), {QStringLiteral("cpu.index"),
                                          QStringLiteral("gpus.card"),
                                          QStringLiteral("thermals.class+source+label"),
                                          QStringLiteral("fans.source+label"),
                                          QStringLiteral("disks.name"),
                                          QStringLiteral("network.name")}},
        {QStringLiteral("services"), {QStringLiteral("name")}},
        {QStringLiteral("startup"), {QStringLiteral("id")}},
        {QStringLiteral("connections"), {QStringLiteral("socketInode")}},
        {QStringLiteral("information"), {}}};
    const QJsonArray views = root.value(QStringLiteral("views")).toArray();
    if (views.size() != expectedViews.size())
        return fail(errorId, QStringLiteral("presentation-invalid"));

    MonitorPresentationContract candidate;
    candidate.maximumLineBytes = maximumLine;
    for (qsizetype index = 0; index < views.size(); ++index) {
        if (!views.at(index).isObject())
            return fail(errorId, QStringLiteral("presentation-invalid"));
        const QJsonObject view = views.at(index).toObject();
        const QString id = view.value(QStringLiteral("id")).toString();
        qint64 ordinal = 0;
        if (id != expectedViews.at(index)
            || view.value(QStringLiteral("schema")).toString() != expectedSchemas.value(id)
            || !view.value(QStringLiteral("streamable")).toBool()
            || !integerValue(view.value(QStringLiteral("ordinal")), 1, &ordinal)
            || ordinal != index + 1)
            return fail(errorId, QStringLiteral("presentation-invalid"));
        MonitorViewCapability capability;
        capability.id = id;
        capability.schema = expectedSchemas.value(id);
        QStringList rowIdentity;
        if (!stringArray(view.value(QStringLiteral("sort")), &capability.sortIds)
            || !stringArray(view.value(QStringLiteral("group")), &capability.groupIds)
            || !stringArray(view.value(QStringLiteral("columns")), &capability.columnIds)
            || !stringArray(view.value(QStringLiteral("rowIdentity")), &rowIdentity)
            || rowIdentity != expectedIdentities.value(id)
            || !view.value(QStringLiteral("filter")).isObject())
            return fail(errorId, QStringLiteral("presentation-invalid"));
        const QJsonObject filter = view.value(QStringLiteral("filter")).toObject();
        capability.filterSupported = filter.value(QStringLiteral("supported")).toBool(false);
        if (capability.filterSupported) {
            qint64 maximumFilter = 0;
            if (!integerValue(filter.value(QStringLiteral("maximumBytes")), 1,
                              &maximumFilter)
                || maximumFilter != 64
                || filter.value(QStringLiteral("characterSet")).toString()
                    != QStringLiteral("printable-ascii"))
                return fail(errorId, QStringLiteral("presentation-invalid"));
        }
        candidate.views.insert(id, capability);
        candidate.orderedViews.append(id);
    }

    const QJsonObject sampling = root.value(QStringLiteral("sampling")).toObject();
    const auto range = [&sampling](const QString &name, int *minimum, int *maximum) {
        if (!sampling.value(name).isObject()) return false;
        const QJsonObject object = sampling.value(name).toObject();
        qint64 low = 0;
        qint64 high = 0;
        return integerValue(object.value(QStringLiteral("minimum")), 1, &low)
            && integerValue(object.value(QStringLiteral("maximum")), low, &high)
            && high <= 1000000 && ((*minimum = static_cast<int>(low)), true)
            && ((*maximum = static_cast<int>(high)), true);
    };
    if (!range(QStringLiteral("sampleMilliseconds"), &candidate.sampleMinimum,
               &candidate.sampleMaximum)
        || !range(QStringLiteral("intervalMilliseconds"), &candidate.intervalMinimum,
                  &candidate.intervalMaximum)
        || !range(QStringLiteral("rowLimit"), &candidate.rowLimitMinimum,
                  &candidate.rowLimitMaximum))
        return fail(errorId, QStringLiteral("presentation-invalid"));

    *contract = std::move(candidate);
    if (errorId) errorId->clear();
    return true;
}

bool decodeFrame(const QByteArray &payload, const MonitorPresentationContract &presentation,
                 const QString &expectedView, qint64 expectedSequence,
                 int expectedIntervalMilliseconds, int rowLimit,
                 QVariantMap *frame, QVariantList *rows, QStringList *identities,
                 QString *errorId) {
    if (!frame || !rows || !identities || !presentation.views.contains(expectedView))
        return fail(errorId, QStringLiteral("stream-invalid"));
    QJsonObject root;
    if (!oneJsonObject(payload, presentation.maximumLineBytes, &root, errorId))
        return fail(errorId, QStringLiteral("stream-invalid"));
    const MonitorViewCapability capability = presentation.views.value(expectedView);
    if (root.value(QStringLiteral("schema")).toString() != capability.schema
        || !root.value(QStringLiteral("readOnly")).toBool()
        || root.value(QStringLiteral("view")).toString() != expectedView
        || !root.value(QStringLiteral("stream")).isObject())
        return fail(errorId, QStringLiteral("stream-invalid"));
    const QJsonObject stream = root.value(QStringLiteral("stream")).toObject();
    qint64 sequence = -1;
    qint64 interval = 0;
    if (stream.value(QStringLiteral("schema")).toString()
            != QStringLiteral("synapse.monitor.stream-frame/v1")
        || !integerValue(stream.value(QStringLiteral("sequence")), 0, &sequence)
        || sequence != expectedSequence
        || !integerValue(stream.value(QStringLiteral("intervalMilliseconds")), 1, &interval)
        || interval != expectedIntervalMilliseconds)
        return fail(errorId, QStringLiteral("stream-sequence-invalid"));

    if (!validFrameSemantics(root, expectedView))
        return fail(errorId, QStringLiteral("stream-invalid"));

    QVariantList variantRows;
    QStringList rowIdentities;
    if (expectedView == QStringLiteral("performance")) {
        if (!validPerformance(root)) return fail(errorId, QStringLiteral("stream-invalid"));
    } else if (expectedView == QStringLiteral("information")) {
        if (!root.value(QStringLiteral("information")).isObject())
            return fail(errorId, QStringLiteral("stream-invalid"));
    } else {
        if (!root.value(QStringLiteral("rows")).isArray()
            || !uniqueRows(root.value(QStringLiteral("rows")).toArray(), expectedView,
                           rowLimit, &variantRows, &rowIdentities))
            return fail(errorId, QStringLiteral("stream-invalid"));
    }
    *frame = root.toVariantMap();
    *rows = std::move(variantRows);
    *identities = std::move(rowIdentities);
    if (errorId) errorId->clear();
    return true;
}

bool decodeInspection(const QByteArray &payload, qint64 expectedPid,
                      qint64 expectedStartTicks, QVariantMap *inspection,
                      QString *errorId) {
    if (!inspection) return fail(errorId, QStringLiteral("inspection-invalid"));
    QJsonObject root;
    if (!oneJsonObject(payload, qsizetype{256} * 1024, &root, errorId))
        return fail(errorId, QStringLiteral("inspection-invalid"));
    if (root.value(QStringLiteral("schema")).toString()
            != QStringLiteral("synapse.monitor.process-inspection/v1")
        || !root.value(QStringLiteral("readOnly")).toBool()
        || !root.value(QStringLiteral("identity")).isObject()
        || !root.value(QStringLiteral("modules")).isObject()
        || !root.value(QStringLiteral("descriptors")).isObject()
        || !root.value(QStringLiteral("semantics")).isObject())
        return fail(errorId, QStringLiteral("inspection-invalid"));
    const QJsonObject identity = root.value(QStringLiteral("identity")).toObject();
    qint64 pid = 0;
    qint64 startTicks = 0;
    if (!integerValue(identity.value(QStringLiteral("pid")), 1, &pid)
        || !integerValue(identity.value(QStringLiteral("startTicks")), 1, &startTicks)
        || pid != expectedPid || startTicks != expectedStartTicks)
        return fail(errorId, QStringLiteral("inspection-identity-changed"));
    const QJsonObject semantics = root.value(QStringLiteral("semantics")).toObject();
    if (!allFalse(semantics, {QStringLiteral("processControl"),
                              QStringLiteral("processDump"),
                              QStringLiteral("commandLineExposed"),
                              QStringLiteral("environmentExposed")})
        || root.value(QStringLiteral("modules")).toObject()
               .value(QStringLiteral("pathsExposed")).toBool(true)
        || root.value(QStringLiteral("descriptors")).toObject()
               .value(QStringLiteral("targetsExposed")).toBool(true))
        return fail(errorId, QStringLiteral("inspection-invalid"));
    *inspection = root.toVariantMap();
    if (errorId) errorId->clear();
    return true;
}

} // namespace MonitorContracts

MonitorRowsModel::MonitorRowsModel(QObject *parent) : QAbstractListModel(parent) {}

int MonitorRowsModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant MonitorRowsModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) return {};
    if (role == RowRole) return rows_.at(index.row());
    if (role == IdentityRole) return identities_.at(index.row());
    return {};
}

QHash<int, QByteArray> MonitorRowsModel::roleNames() const {
    return {{RowRole, QByteArrayLiteral("row")},
            {IdentityRole, QByteArrayLiteral("stableIdentity")}};
}

void MonitorRowsModel::replace(QVariantList rows, QStringList identities) {
    beginResetModel();
    rows_ = std::move(rows);
    identities_ = std::move(identities);
    endResetModel();
}

void MonitorRowsModel::clear() { replace({}, {}); }

MonitorAdapter::MonitorAdapter(QString backendPath, QObject *parent)
    : QObject(parent), backendPath_(std::move(backendPath)), rows_(this) {
    stream_.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&stream_, &QProcess::readyReadStandardOutput,
            this, &MonitorAdapter::consumeStreamOutput);
    connect(&stream_, &QProcess::readyReadStandardError,
            this, &MonitorAdapter::consumeStreamError);
    connect(&stream_, &QProcess::finished, this,
            [this](int, QProcess::ExitStatus) {
                if (stopping_) return;
                consumeStreamOutput();
                consumeStreamError();
                if (!errorId_.isEmpty()) return;
                failStream(QStringLiteral("stream-exited"));
            });
    connect(&stream_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (!stopping_ && error == QProcess::FailedToStart)
            failStream(QStringLiteral("backend-unavailable"));
    });

    inspectionProcess_.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&inspectionProcess_, &QProcess::readyReadStandardOutput, this, [this]() {
        inspectionOutput_.append(inspectionProcess_.readAllStandardOutput());
        if (inspectionOutput_.size() > kMaximumInspectionBytes)
            failInspection(QStringLiteral("inspection-output-too-large"));
    });
    connect(&inspectionProcess_, &QProcess::readyReadStandardError, this, [this]() {
        inspectionError_.append(inspectionProcess_.readAllStandardError());
        if (inspectionError_.size() > kMaximumErrorBytes)
            failInspection(QStringLiteral("inspection-failed"));
    });
    connect(&inspectionProcess_, &QProcess::finished,
            this, &MonitorAdapter::finishInspection);
    connect(&inspectionProcess_, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                if (error == QProcess::FailedToStart)
                    failInspection(QStringLiteral("inspection-unavailable"));
            });
}

MonitorAdapter::~MonitorAdapter() {
    stopStream();
    closeInspection();
}

bool MonitorAdapter::initialize(const QString &initialView) {
    if (initialized_) return false;
    const QFileInfo backend(backendPath_);
    if (!backend.isAbsolute() || !backend.isFile() || !backend.isExecutable()
        || backend.canonicalFilePath().isEmpty()) {
        errorId_ = QStringLiteral("backend-unavailable");
        emit stateChanged();
        return false;
    }
    backendPath_ = backend.canonicalFilePath();
    if (!loadPresentation()) return false;
    if (!presentation_.views.contains(initialView)) {
        errorId_ = QStringLiteral("view-invalid");
        emit stateChanged();
        return false;
    }
    initialized_ = true;
    currentView_ = initialView;
    sortId_ = defaultSort(currentView_);
    groupId_ = defaultGroup(currentView_);
    rowLimit_ = std::clamp(128, presentation_.rowLimitMinimum,
                           presentation_.rowLimitMaximum);
    sampleMilliseconds_ = std::clamp(250, presentation_.sampleMinimum,
                                     presentation_.sampleMaximum);
    intervalMilliseconds_ = std::clamp(1000, presentation_.intervalMinimum,
                                       presentation_.intervalMaximum);
    emit capabilitiesChanged();
    emit selectionChanged();
    return startStream();
}

bool MonitorAdapter::ready() const { return ready_; }
bool MonitorAdapter::streaming() const { return streaming_; }
QString MonitorAdapter::currentView() const { return currentView_; }
QVariantList MonitorAdapter::views() const { return views_; }
QVariantMap MonitorAdapter::payload() const { return payload_; }
QAbstractItemModel *MonitorAdapter::rows() { return &rows_; }
qint64 MonitorAdapter::sequence() const { return sequence_; }
QString MonitorAdapter::errorId() const { return errorId_; }
QString MonitorAdapter::filter() const { return filter_; }
QString MonitorAdapter::sortId() const { return sortId_; }
QString MonitorAdapter::groupId() const { return groupId_; }
QStringList MonitorAdapter::sortIds() const { return currentCapability().sortIds; }
QStringList MonitorAdapter::groupIds() const { return currentCapability().groupIds; }
bool MonitorAdapter::filterSupported() const { return currentCapability().filterSupported; }
int MonitorAdapter::intervalMilliseconds() const { return intervalMilliseconds_; }
QVariantMap MonitorAdapter::inspection() const { return inspection_; }
bool MonitorAdapter::inspectionBusy() const { return inspectionBusy_; }
QString MonitorAdapter::inspectionErrorId() const { return inspectionErrorId_; }

bool MonitorAdapter::loadPresentation() {
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.setProgram(backendPath_);
    process.setArguments({QStringLiteral("describe"), QStringLiteral("--format"),
                          QStringLiteral("json")});
    process.start(QIODevice::ReadOnly);
    if (!process.waitForStarted(500)) {
        errorId_ = QStringLiteral("backend-unavailable");
        emit stateChanged();
        return false;
    }
    QByteArray output;
    QByteArray errors;
    QElapsedTimer elapsed;
    elapsed.start();
    while (process.state() != QProcess::NotRunning && elapsed.elapsed() < 2000) {
        process.waitForReadyRead(50);
        output.append(process.readAllStandardOutput());
        errors.append(process.readAllStandardError());
        if (output.size() > kMaximumDescribeBytes || errors.size() > kMaximumErrorBytes) {
            process.kill();
            process.waitForFinished(200);
            errorId_ = QStringLiteral("presentation-invalid");
            emit stateChanged();
            return false;
        }
    }
    if (process.state() != QProcess::NotRunning) {
        process.kill();
        process.waitForFinished(200);
        errorId_ = QStringLiteral("presentation-timeout");
        emit stateChanged();
        return false;
    }
    output.append(process.readAllStandardOutput());
    errors.append(process.readAllStandardError());
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0
        || !errors.isEmpty()
        || !MonitorContracts::decodePresentation(output, &presentation_, &errorId_)) {
        if (errorId_.isEmpty()) errorId_ = QStringLiteral("presentation-invalid");
        emit stateChanged();
        return false;
    }
    views_.clear();
    for (qsizetype index = 0; index < presentation_.orderedViews.size(); ++index) {
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), presentation_.orderedViews.at(index));
        entry.insert(QStringLiteral("ordinal"), index + 1);
        views_.append(entry);
    }
    errorId_.clear();
    return true;
}

bool MonitorAdapter::startStream() {
    if (!initialized_ || !presentation_.views.contains(currentView_)) return false;
    stopStream();
    streamBuffer_.clear();
    streamErrorBuffer_.clear();
    payload_.clear();
    rows_.clear();
    sequence_ = -1;
    ready_ = false;
    streaming_ = false;
    errorId_.clear();
    emit frameChanged();
    emit stateChanged();

    QStringList arguments = {QStringLiteral("stream"), QStringLiteral("--view"), currentView_,
                             QStringLiteral("--format"), QStringLiteral("ndjson")};
    if (currentView_ == QStringLiteral("processes")
        || currentView_ == QStringLiteral("performance"))
        arguments.append({QStringLiteral("--sample-ms"),
                          QString::number(sampleMilliseconds_)});
    arguments.append({QStringLiteral("--interval-ms"),
                      QString::number(intervalMilliseconds_),
                      QStringLiteral("--limit"), QString::number(rowLimit_)});
    const MonitorViewCapability capability = currentCapability();
    if (!sortId_.isEmpty())
        arguments.append({QStringLiteral("--sort"), sortId_});
    if (!groupId_.isEmpty())
        arguments.append({QStringLiteral("--group"), groupId_});
    if (capability.filterSupported && !filter_.isEmpty())
        arguments.append({QStringLiteral("--filter"), filter_});

    stream_.setProgram(backendPath_);
    stream_.setArguments(arguments);
    stream_.start(QIODevice::ReadOnly);
    if (!stream_.waitForStarted(500)) {
        errorId_ = QStringLiteral("backend-unavailable");
        emit stateChanged();
        return false;
    }
    streaming_ = true;
    emit stateChanged();
    return true;
}

void MonitorAdapter::stopStream() {
    if (stream_.state() == QProcess::NotRunning) {
        streaming_ = false;
        return;
    }
    stopping_ = true;
    stream_.terminate();
    if (!stream_.waitForFinished(500)) {
        stream_.kill();
        stream_.waitForFinished(500);
    }
    stopping_ = false;
    streaming_ = false;
}

void MonitorAdapter::consumeStreamOutput() {
    if (stopping_) return;
    streamBuffer_.append(stream_.readAllStandardOutput());
    for (;;) {
        const qsizetype newline = streamBuffer_.indexOf('\n');
        if (newline < 0) break;
        if (newline == 0 || newline > kMaximumContractLine) {
            failStream(newline > kMaximumContractLine
                           ? QStringLiteral("stream-line-too-large")
                           : QStringLiteral("stream-invalid"));
            return;
        }
        const QByteArray line = streamBuffer_.left(newline);
        streamBuffer_.remove(0, newline + 1);
        QVariantMap frame;
        QVariantList rows;
        QStringList identities;
        QString parseError;
        if (!MonitorContracts::decodeFrame(line, presentation_, currentView_, sequence_ + 1,
                                           intervalMilliseconds_, rowLimit_, &frame, &rows,
                                           &identities, &parseError)) {
            failStream(parseError.isEmpty() ? QStringLiteral("stream-invalid") : parseError);
            return;
        }
        payload_ = std::move(frame);
        rows_.replace(std::move(rows), std::move(identities));
        ++sequence_;
        const bool wasReady = ready_;
        ready_ = true;
        errorId_.clear();
        emit frameChanged();
        if (!wasReady) emit stateChanged();
        emit frameAccepted();
    }
    if (streamBuffer_.size() > kMaximumContractLine)
        failStream(QStringLiteral("stream-line-too-large"));
}

void MonitorAdapter::consumeStreamError() {
    if (stopping_) return;
    const QByteArray error = stream_.readAllStandardError();
    if (error.isEmpty()) return;
    streamErrorBuffer_.append(error);
    if (streamErrorBuffer_.size() > kMaximumErrorBytes)
        streamErrorBuffer_.truncate(kMaximumErrorBytes);
    failStream(QStringLiteral("stream-failed"));
}

void MonitorAdapter::failStream(const QString &errorId) {
    if (stopping_) return;
    stopStream();
    ready_ = false;
    streaming_ = false;
    errorId_ = errorId;
    emit stateChanged();
}

bool MonitorAdapter::selectView(const QString &viewId) {
    if (!initialized_ || !presentation_.views.contains(viewId)) {
        errorId_ = QStringLiteral("view-invalid");
        emit stateChanged();
        return false;
    }
    if (viewId == currentView_) return true;
    closeInspection();
    currentView_ = viewId;
    filter_.clear();
    sortId_ = defaultSort(viewId);
    groupId_ = defaultGroup(viewId);
    emit selectionChanged();
    return startStream();
}

bool MonitorAdapter::setFilter(const QString &filter) {
    if (!initialized_ || (!filter.isEmpty() && !filterSupported())
        || !printableFilter(filter)) {
        errorId_ = QStringLiteral("filter-invalid");
        emit stateChanged();
        return false;
    }
    if (filter == filter_) return true;
    filter_ = filter;
    emit selectionChanged();
    return startStream();
}

bool MonitorAdapter::setSortId(const QString &sortId) {
    if (!initialized_ || (!sortId.isEmpty() && !sortIds().contains(sortId))) {
        errorId_ = QStringLiteral("sort-invalid");
        emit stateChanged();
        return false;
    }
    if (sortId == sortId_) return true;
    sortId_ = sortId;
    emit selectionChanged();
    return startStream();
}

bool MonitorAdapter::setGroupId(const QString &groupId) {
    if (!initialized_ || (!groupId.isEmpty() && !groupIds().contains(groupId))) {
        errorId_ = QStringLiteral("group-invalid");
        emit stateChanged();
        return false;
    }
    if (groupId == groupId_) return true;
    groupId_ = groupId;
    emit selectionChanged();
    return startStream();
}

bool MonitorAdapter::setIntervalMilliseconds(int milliseconds) {
    if (!initialized_ || milliseconds < presentation_.intervalMinimum
        || milliseconds > presentation_.intervalMaximum) {
        errorId_ = QStringLiteral("interval-invalid");
        emit stateChanged();
        return false;
    }
    if (milliseconds == intervalMilliseconds_) return true;
    intervalMilliseconds_ = milliseconds;
    emit selectionChanged();
    return startStream();
}

bool MonitorAdapter::inspectProcess(qint64 pid, qint64 startTicks) {
    if (!initialized_ || currentView_ != QStringLiteral("processes")
        || pid <= 0 || startTicks <= 0) {
        failInspection(QStringLiteral("inspection-invalid"));
        return false;
    }
    closeInspection();
    inspectionPid_ = pid;
    inspectionStartTicks_ = startTicks;
    inspectionOutput_.clear();
    inspectionError_.clear();
    inspection_.clear();
    inspectionErrorId_.clear();
    inspectionBusy_ = true;
    emit inspectionChanged();
    inspectionProcess_.setProgram(backendPath_);
    inspectionProcess_.setArguments({QStringLiteral("inspect"), QStringLiteral("--pid"),
                                     QString::number(pid), QStringLiteral("--format"),
                                     QStringLiteral("json")});
    inspectionProcess_.start(QIODevice::ReadOnly);
    if (!inspectionProcess_.waitForStarted(500)) {
        failInspection(QStringLiteral("inspection-unavailable"));
        return false;
    }
    return true;
}

void MonitorAdapter::finishInspection(int exitCode, QProcess::ExitStatus status) {
    if (!inspectionBusy_) return;
    inspectionOutput_.append(inspectionProcess_.readAllStandardOutput());
    inspectionError_.append(inspectionProcess_.readAllStandardError());
    if (status != QProcess::NormalExit || exitCode != 0 || !inspectionError_.isEmpty()
        || inspectionOutput_.size() > kMaximumInspectionBytes) {
        failInspection(QStringLiteral("inspection-unavailable"));
        return;
    }
    QVariantMap decoded;
    QString decodeError;
    if (!MonitorContracts::decodeInspection(inspectionOutput_, inspectionPid_,
                                            inspectionStartTicks_, &decoded, &decodeError)) {
        failInspection(decodeError.isEmpty() ? QStringLiteral("inspection-invalid")
                                             : decodeError);
        return;
    }
    inspection_ = std::move(decoded);
    inspectionBusy_ = false;
    inspectionErrorId_.clear();
    emit inspectionChanged();
}

void MonitorAdapter::failInspection(const QString &errorId) {
    inspectionBusy_ = false;
    if (inspectionProcess_.state() != QProcess::NotRunning) {
        inspectionProcess_.kill();
        inspectionProcess_.waitForFinished(200);
    }
    inspection_.clear();
    inspectionErrorId_ = errorId;
    emit inspectionChanged();
}

void MonitorAdapter::closeInspection() {
    if (inspectionProcess_.state() != QProcess::NotRunning) {
        inspectionBusy_ = false;
        inspectionProcess_.terminate();
        if (!inspectionProcess_.waitForFinished(200)) {
            inspectionProcess_.kill();
            inspectionProcess_.waitForFinished(200);
        }
    }
    inspectionBusy_ = false;
    inspection_.clear();
    inspectionErrorId_.clear();
    inspectionOutput_.clear();
    inspectionError_.clear();
    emit inspectionChanged();
}

MonitorViewCapability MonitorAdapter::currentCapability() const {
    return presentation_.views.value(currentView_);
}

QString MonitorAdapter::defaultSort(const QString &viewId) const {
    const QStringList values = presentation_.views.value(viewId).sortIds;
    return values.isEmpty() ? QString() : values.first();
}

QString MonitorAdapter::defaultGroup(const QString &viewId) const {
    const QStringList values = presentation_.views.value(viewId).groupIds;
    return values.isEmpty() ? QString() : values.first();
}
