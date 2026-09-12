#include <QPointer>
// File: src/communication/ControllerDeviceBackendDownload.cpp

#include "ControllerDeviceBackend.h"

#include "ControllerDebugProtocol.h"

#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "BackendOperationGuard.h"
#include "common/PathSecurityUtils.h"

using namespace PathSecurityUtils;

#include <QMutexLocker>
#include <QSet>
#include <QThread>
#include <QtGlobal>

using Communication::BackendOperationGuard;

namespace {

QVector<quint16> variantToRegisters(const QVariant& value)
{
    QVector<quint16> registers;
    if (value.type() == QVariant::List || value.type() == QVariant::StringList) {
        const QVariantList list = value.toList();
        registers.reserve(list.size());
        for (const QVariant& item : list) {
            registers.append(static_cast<quint16>(item.toUInt()));
        }
        return registers;
    }

    if (value.canConvert<QVector<quint16>>()) {
        return value.value<QVector<quint16>>();
    }

    registers.append(static_cast<quint16>(value.toUInt()));
    return registers;
}

QVector<quint16> bytesToRegisters(const QByteArray& payload, bool littleEndian = false)
{
    QVector<quint16> words;
    words.reserve((payload.size() + 1) / 2);
    for (int i = 0; i < payload.size(); i += 2) {
        const quint8 first = static_cast<quint8>(payload.at(i));
        const quint8 second = (i + 1 < payload.size()) ? static_cast<quint8>(payload.at(i + 1)) : 0;
        const quint8 hi = littleEndian ? second : first;
        const quint8 lo = littleEndian ? first : second;
        words.append(static_cast<quint16>((hi << 8) | lo));
    }
    return words;
}

quint16 calcCrc16Modbus(const QByteArray& bytes)
{
    quint16 crc = 0xFFFF;
    for (char value : bytes) {
        crc ^= static_cast<quint8>(value);
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 0x0001)
                    ? static_cast<quint16>((crc >> 1) ^ 0xA001)
                    : static_cast<quint16>(crc >> 1);
        }
    }
    return crc;
}

QString profilePathFromOptions(const QVariantMap& options, const ProjectRuntimeConfig& config)
{
    const QStringList keys = {
        QStringLiteral("downloadProfilePath"),
        QStringLiteral("profileJsonPath"),
        QStringLiteral("profilePath")
    };
    for (const QString& key : keys) {
        const QString value = options.value(key).toString().trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    const QStringList configKeys = {
        QStringLiteral("downloadProfilePath"),
        QStringLiteral("profileJsonPath"),
        QStringLiteral("profilePath"),
        QStringLiteral("downloadProfileSourcePath"),
        QStringLiteral("profileSourcePath"),
        QStringLiteral("sourceProfilePath")
    };
    for (const QString& key : configKeys) {
        const QString value = config.downloadArtifact.metadata.value(key).toString().trimmed();
        if (!value.isEmpty())
            return value;
    }
    return QString();
}

bool validateRequiredArtifactBinding(const ProjectRuntimeConfig& config,
                                     QString* errorMessage)
{
    const QString generationId = config.downloadArtifact.metadata
                                         .value(QStringLiteral("generationId"))
                                         .toString().trimmed();
    const QString manifestPath = config.downloadArtifact.metadata
                                         .value(QStringLiteral("runtimeManifestPath"))
                                         .toString().trimmed().isEmpty()
            ? config.downloadArtifact.metadata.value(QStringLiteral("manifestPath"))
                    .toString().trimmed()
            : config.downloadArtifact.metadata.value(QStringLiteral("runtimeManifestPath"))
                    .toString().trimmed();
    const QString checksum = config.downloadArtifact.checksum.trimmed();

    QStringList missing;
    if (generationId.isEmpty())
        missing << QStringLiteral("generationId");
    if (manifestPath.isEmpty())
        missing << QStringLiteral("runtimeManifestPath");
    if (checksum.isEmpty())
        missing << QStringLiteral("code checksum");
    const QStringList requiredChecksums = {
        QStringLiteral("downloadProfileChecksum"),
        QStringLiteral("runtimePointsChecksum"),
        QStringLiteral("runtimeManifestChecksum")
    };
    for (const QString& key : requiredChecksums) {
        if (config.downloadArtifact.metadata.value(key).toString().trimmed().isEmpty())
            missing << key;
    }
    if (missing.isEmpty())
        return true;

    if (errorMessage) {
        *errorMessage = QStringLiteral("物理控制器下载要求已发布 generation、manifest 和 code checksum（%1）。")
                .arg(QStringLiteral("缺少：%1").arg(missing.join(QStringLiteral(", "))));
    }
    return false;
}

bool validatePublishedProfileBinding(const QString& artifactPath,
                                     const QVariantMap& options,
                                     const ProjectRuntimeConfig& config,
                                     QString* profilePath,
                                     QString* errorMessage)
{
    const QString selectedProfile = profilePathFromOptions(options, config).trimmed();
    if (options.value(QStringLiteral("profileOverrideConflict")).toBool()) {
        if (errorMessage) {
            *errorMessage = options.value(QStringLiteral("profileOverrideError"))
                                    .toString().trimmed();
        }
        return false;
    }

    const QString generationId = config.downloadArtifact.metadata
                                         .value(QStringLiteral("generationId"))
                                         .toString().trimmed();
    const QString configuredManifest = config.downloadArtifact.metadata
                                               .value(QStringLiteral("runtimeManifestPath"))
                                               .toString().trimmed();
    const bool requiresPublishedBinding = !generationId.isEmpty() || !configuredManifest.isEmpty();
    if (!requiresPublishedBinding) {
        if (profilePath)
            *profilePath = selectedProfile;
        return !selectedProfile.isEmpty();
    }

    const QFileInfo codeInfo(artifactPath);
    const QDir generationDir(codeInfo.absolutePath());
    if (generationDir.dirName().isEmpty()
            || QFileInfo(generationDir.absolutePath()).dir().dirName()
                   != QStringLiteral("generations")) {
        if (errorMessage)
            *errorMessage = QStringLiteral("下载产物不在已发布 generation 目录中。");
        return false;
    }
    if (codeInfo.suffix().compare(QStringLiteral("code"), Qt::CaseInsensitive) != 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("下载产物必须是 .code 文件。");
        return false;
    }
    const QString actualGenerationId = generationDir.dirName();
    if (!generationId.isEmpty() && generationId != actualGenerationId) {
        if (errorMessage)
            *errorMessage = QStringLiteral("下载产物 generationId 与项目配置不一致。");
        return false;
    }

    const QString manifestPath = generationDir.filePath(QStringLiteral("runtime_manifest.json"));
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("已发布 generation 缺少 runtime_manifest.json。");
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument manifestDocument = QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !manifestDocument.isObject()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("runtime_manifest.json 无效。");
        return false;
    }
    const QJsonObject manifest = manifestDocument.object();
    if (!manifest.value(QStringLiteral("complete")).toBool(false)
            || manifest.value(QStringLiteral("generationId")).toString() != actualGenerationId) {
        if (errorMessage)
            *errorMessage = QStringLiteral("runtime_manifest 未提交为当前 generation。");
        return false;
    }

    if (!configuredManifest.isEmpty()) {
        const QString normalized = QDir::fromNativeSeparators(configuredManifest);
        const QString expectedSuffix = QStringLiteral("generations/%1/runtime_manifest.json")
                .arg(actualGenerationId);
        if ((QFileInfo(configuredManifest).isAbsolute()
             && comparablePath(configuredManifest) != comparablePath(manifestPath))
                || (QFileInfo(configuredManifest).isRelative()
                    && !normalized.endsWith(expectedSuffix))) {
            if (errorMessage)
                *errorMessage = QStringLiteral("项目配置 runtime_manifest 与下载产物不一致。");
            return false;
        }
    }

    const QString codeRelative = manifest.value(QStringLiteral("codePath")).toString().trimmed();
    const QString profileRelative = manifest.value(QStringLiteral("downloadProfilePath")).toString().trimmed();
    const QString manifestRelative = manifest.value(QStringLiteral("runtimeManifestPath"))
            .toString().trimmed();
    if (!safeManifestRelativePath(codeRelative)
            || !safeManifestRelativePath(profileRelative)
            || manifestRelative != QStringLiteral("runtime_manifest.json")) {
        if (errorMessage)
            *errorMessage = QStringLiteral("runtime_manifest mandatory path 无效。");
        return false;
    }
    const QString manifestCode = QDir(generationDir).cleanPath(
            QDir(generationDir).absoluteFilePath(codeRelative));
    const QString manifestProfile = QDir(generationDir).cleanPath(
            QDir(generationDir).absoluteFilePath(profileRelative));
    const QString pointsRelative = manifest.value(QStringLiteral("runtimePointsPath"))
                                           .toString().trimmed();
    const QString manifestPoints = QDir(generationDir).cleanPath(
            QDir(generationDir).absoluteFilePath(pointsRelative));
    if (manifestRelative != QStringLiteral("runtime_manifest.json")
            || !safeManifestRelativePath(pointsRelative)
            || !pathWithinDirectory(generationDir.absolutePath(), manifestCode)
            || !pathWithinDirectory(generationDir.absolutePath(), manifestProfile)
            || !pathWithinDirectory(generationDir.absolutePath(), manifestPoints)
            || comparablePath(manifestCode) != comparablePath(codeInfo.absoluteFilePath())
            || comparablePath(QFileInfo(manifestProfile).absolutePath())
                       != comparablePath(generationDir.absolutePath())
            || comparablePath(QFileInfo(manifestPoints).absolutePath())
                       != comparablePath(generationDir.absolutePath())
            || !QFileInfo(manifestProfile).isFile()
            || !QFileInfo(manifestPoints).isFile()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("runtime_manifest code/profile 不属于同一 generation。");
        return false;
    }

    const QString actualCodeChecksum = sha256ForFile(codeInfo.absoluteFilePath());
    const QString expectedCodeChecksum = config.downloadArtifact.checksum.trimmed();
    const QString manifestCodeChecksum = manifest.value(QStringLiteral("codeChecksum"))
                                                 .toString().trimmed();
    const QString manifestProfileChecksum = manifest.value(
            QStringLiteral("downloadProfileChecksum")).toString().trimmed();
    const QString manifestPointsChecksum = manifest.value(
            QStringLiteral("runtimePointsChecksum")).toString().trimmed();
    const QString configuredProfileChecksum = config.downloadArtifact.metadata
                                                      .value(QStringLiteral("downloadProfileChecksum"))
                                                      .toString().trimmed();
    const QString configuredPointsChecksum = config.downloadArtifact.metadata
                                                     .value(QStringLiteral("runtimePointsChecksum"))
                                                     .toString().trimmed();
    const QString configuredManifestChecksum = config.downloadArtifact.metadata
                                                       .value(QStringLiteral("runtimeManifestChecksum"))
                                                       .toString().trimmed();
    const QString actualProfileChecksum = sha256ForFile(manifestProfile);
    const QString actualPointsChecksum = sha256ForFile(manifestPoints);
    const QString actualManifestChecksum = sha256ForFile(manifestPath);
    if (actualCodeChecksum.isEmpty() || expectedCodeChecksum.isEmpty()
            || manifestCodeChecksum.isEmpty()
            || actualCodeChecksum.compare(expectedCodeChecksum, Qt::CaseInsensitive) != 0
            || actualCodeChecksum.compare(manifestCodeChecksum, Qt::CaseInsensitive) != 0
            || manifestProfileChecksum.isEmpty()
            || actualProfileChecksum.compare(manifestProfileChecksum, Qt::CaseInsensitive) != 0
            || actualProfileChecksum.compare(configuredProfileChecksum, Qt::CaseInsensitive) != 0
            || manifestPointsChecksum.isEmpty()
            || actualPointsChecksum.compare(manifestPointsChecksum, Qt::CaseInsensitive) != 0
            || actualPointsChecksum.compare(configuredPointsChecksum, Qt::CaseInsensitive) != 0
            || actualManifestChecksum.isEmpty()
            || actualManifestChecksum.compare(configuredManifestChecksum, Qt::CaseInsensitive) != 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("runtime_manifest mandatory artifact checksum 不一致。");
        return false;
    }

    const QJsonArray artifactPaths = manifest.value(QStringLiteral("artifactPaths")).toArray();
    const QJsonObject artifactChecksums = manifest.value(QStringLiteral("artifactChecksums")).toObject();
    if (artifactPaths.isEmpty() || artifactChecksums.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("runtime_manifest 缺少 artifact checksum 清单。");
        return false;
    }
    QSet<QString> listedArtifacts;
    for (const QJsonValue& value : artifactPaths) {
        const QString relative = value.toString().trimmed();
        const QString artifact = QDir(generationDir).cleanPath(
                QDir(generationDir).absoluteFilePath(relative));
        const QString expected = artifactChecksums.value(relative).toString().trimmed();
        if (!safeManifestRelativePath(relative)
                || !pathWithinDirectory(generationDir.absolutePath(), artifact)
                || !QFileInfo(artifact).isFile()
                || expected.isEmpty()
                || sha256ForFile(artifact).compare(expected, Qt::CaseInsensitive) != 0) {
            if (errorMessage)
                *errorMessage = QStringLiteral("runtime_manifest artifact 路径或 checksum 无效：%1")
                        .arg(relative);
            return false;
        }
        listedArtifacts.insert(relative);
    }
    if (!listedArtifacts.contains(codeRelative)
            || !listedArtifacts.contains(profileRelative)
            || !listedArtifacts.contains(pointsRelative)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("runtime_manifest 未列出全部 mandatory artifact。");
        return false;
    }

    const QString configuredProfile = [&config]() {
        const QStringList keys = {
            QStringLiteral("downloadProfilePath"),
            QStringLiteral("profileJsonPath"),
            QStringLiteral("profilePath")
        };
        for (const QString& key : keys) {
            const QString value = config.downloadArtifact.metadata.value(key).toString().trimmed();
            if (!value.isEmpty())
                return value;
        }
        return QString();
    }();
    if (!configuredProfile.isEmpty()) {
        const QString normalized = QDir::fromNativeSeparators(configuredProfile);
        const QString expectedSuffix = QStringLiteral("generations/%1/download_profile.json")
                .arg(actualGenerationId);
        if ((QFileInfo(configuredProfile).isAbsolute()
             && comparablePath(configuredProfile) != comparablePath(manifestProfile))
                || (QFileInfo(configuredProfile).isRelative()
                    && !normalized.endsWith(expectedSuffix))) {
            if (errorMessage)
                *errorMessage = QStringLiteral("项目配置 Profile 与已发布 generation 不一致。");
            return false;
        }
    }

    QString selectedComparable = selectedProfile;
    if (QFileInfo(selectedProfile).isRelative()
            && QDir::fromNativeSeparators(selectedProfile)
                       .endsWith(QStringLiteral("generations/%1/download_profile.json")
                                         .arg(actualGenerationId))) {
        selectedComparable = manifestProfile;
    }
    if (selectedProfile.isEmpty()
            || comparablePath(selectedComparable) != comparablePath(manifestProfile)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("下载 Profile override 与已发布 generation 不一致。");
        return false;
    }
    if (profilePath)
        *profilePath = manifestProfile;
    return true;
}

QVariantList stringsToVariantList(const QStringList& values)
{
    QVariantList list;
    list.reserve(values.size());
    for (const QString& value : values) {
        list.append(value);
    }
    return list;
}
} // namespace

void ControllerDeviceBackend::cancelDownload()
{
    m_downloadCancelled.store(true, std::memory_order_release);
}

void ControllerDeviceBackend::resetDownloadCancellation()
{
    m_downloadCancelled.store(false, std::memory_order_release);
}

void ControllerDeviceBackend::downloadArtifactAsync(const QString& path, const QVariantMap& options,
    QObject* context, std::function<void(bool, QString, CommError)> completed)
{
    if (!supportsAsyncDownload() || m_asyncDownloadActive.exchange(true)) {
        completed(false, QStringLiteral("Controller is busy or has no asynchronous client"),
                  CommError(CommProtocolType::ModbusRTU, CommErrorCode::DeviceBusy, QStringLiteral("Download admission rejected")));
        return;
    }
    QPointer<QObject> guard(context);
    m_client->runAsync([this, path, options, guard, completed]() {
        QString message; CommError error;
        if (options.value(QStringLiteral("reconnectBeforeAttempt")).toBool()) {
            disconnectBackend();
            if (!m_downloadCancelled) connectBackend();
        }
        const bool ok = downloadArtifact(path, options, &message, &error);
        m_asyncDownloadActive = false;
        if (guard) QMetaObject::invokeMethod(guard, [=]() { completed(ok, message, error); }, Qt::QueuedConnection);
    });
}

bool ControllerDeviceBackend::downloadArtifact(const QString& artifactPath,
                                               const QVariantMap& options,
                                               QString* errorMessage,
                                               CommError* operationError)
{
    if (m_downloadCancelled.load(std::memory_order_acquire)) {
        const QString message = QStringLiteral("下载已取消。");
        if (errorMessage) {
            *errorMessage = message;
        }
        if (operationError) {
            *operationError = CommError(CommProtocolType::ModbusRTU,
                                        CommErrorCode::OperationCancelled,
                                        message);
        }
        setFailure(CommErrorCode::OperationCancelled, message);
        return false;
    }
    const QString path = artifactPath.trimmed();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        const QString message = path.isEmpty()
                ? QStringLiteral("下载产物路径为空。")
                : QStringLiteral("下载产物不存在：%1").arg(path);
        if (errorMessage) {
            *errorMessage = message;
        }
        if (operationError) {
            *operationError = CommError(CommProtocolType::ModbusRTU,
                                        CommErrorCode::InvalidParameter,
                                        message);
        }
        setFailure(CommErrorCode::InvalidParameter, message);
        return false;
    }

    if (!options.value(QStringLiteral("dryRun")).toBool()
            && !options.value(QStringLiteral("validateOnly")).toBool()) {
        QString bindingError;
        if (!validateRequiredArtifactBinding(m_config, &bindingError)) {
            return setOperationError(CommErrorCode::InvalidConfig,
                                     bindingError,
                                     operationError,
                                     errorMessage);
        }
    }

    QString downloadProfilePath;
    QString profileBindingError;
    if (!validatePublishedProfileBinding(path,
                                         options,
                                         m_config,
                                         &downloadProfilePath,
                                         &profileBindingError)) {
        return setOperationError(CommErrorCode::InvalidConfig,
                                 profileBindingError.isEmpty()
                                         ? QStringLiteral("未配置或未绑定已发布下载 Profile，已阻止下载。")
                                         : profileBindingError,
                                 operationError,
                                 errorMessage);
    }

    if (options.value(QStringLiteral("dryRun")).toBool()
            || options.value(QStringLiteral("validateOnly")).toBool()) {
        return dryRunDownloadArtifact(path, options, errorMessage, operationError);
    }

    m_client->setRequestBudget(-1, &m_downloadCancelled);
    struct CancellationReset { ControllerDebugClient* client; ~CancellationReset() { client->setRequestBudget(-1, nullptr); } } cancellationReset{m_client};
    BackendOperationGuard operation(&m_operationMutex);
    if (!operation.tryLock()) {
        return setOperationError(CommErrorCode::DeviceBusy,
                                 QStringLiteral("控制器后端正忙，下载被拒绝。"),
                                 operationError,
                                 errorMessage);
    }

    if (!ensureOnline(errorMessage)) {
        if (operationError) {
            *operationError = lastError();
        }
        return false;
    }

    {
        DownloadProfile profile;
        QString profileError;
        if (!DownloadProfile::fromJsonFile(downloadProfilePath, profile, &profileError)) {
            return setOperationError(CommErrorCode::InvalidConfig,
                                     QStringLiteral("下载配置读取失败：%1").arg(profileError),
                                     operationError,
                                     errorMessage,
                                     downloadProfilePath);
        }

        QFile payloadFile(path);
        if (!payloadFile.open(QIODevice::ReadOnly)) {
            return setOperationError(CommErrorCode::InvalidParameter,
                                     QStringLiteral("下载产物无法读取：%1").arg(path),
                                     operationError,
                                     errorMessage);
        }

        QMutexLocker lock(&m_mutex);
        m_downloading = true;
        m_downloadPercent = 10;
        m_lastDownloadError.clear();
        lock.unlock();

        const bool ok = executeDownloadProfile(profile, payloadFile.readAll(), errorMessage, operationError);
        lock.relock();
        m_downloading = false;
        m_downloadPercent = ok ? 100 : 0;
        if (!ok && errorMessage) {
            m_lastDownloadError = *errorMessage;
        }
        lock.unlock();

        if (ok) {
            ControllerDebugStatus status;
            if (m_client->readStatus(&status)) {
                QMutexLocker statusLock(&m_mutex);
                updateStatusCache(status);
            }
            if (operationError) {
                *operationError = CommError();
            }
            clearFailure();
        }
        return ok;
    }
}

bool ControllerDeviceBackend::dryRunDownloadArtifact(const QString& artifactPath,
                                                     const QVariantMap& options,
                                                     QString* errorMessage,
                                                     CommError* operationError,
                                                     QVariantMap* report) const
{
    const QString path = artifactPath.trimmed();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        const QString message = path.isEmpty()
                ? QStringLiteral("下载 dry-run 失败：产物路径为空。")
                : QStringLiteral("下载 dry-run 失败：产物不存在：%1").arg(path);
        if (errorMessage) {
            *errorMessage = message;
        }
        if (operationError) {
            *operationError = CommError(CommProtocolType::ModbusRTU,
                                        CommErrorCode::InvalidParameter,
                                        message);
        }
        return false;
    }

    QFile payloadFile(path);
    if (!payloadFile.open(QIODevice::ReadOnly)) {
        const QString message = QStringLiteral("下载 dry-run 失败：产物无法读取：%1").arg(path);
        if (errorMessage) {
            *errorMessage = message;
        }
        if (operationError) {
            *operationError = CommError(CommProtocolType::ModbusRTU,
                                        CommErrorCode::InvalidParameter,
                                        message);
        }
        return false;
    }
    const QByteArray payload = payloadFile.readAll();

    QVariantMap localReport;
    localReport.insert(QStringLiteral("artifactPath"), path);
    localReport.insert(QStringLiteral("payloadBytes"), payload.size());
    localReport.insert(QStringLiteral("payloadRegisters"), bytesToRegisters(payload).size());

    QString profilePath;
    QString profileBindingError;
    if (!validatePublishedProfileBinding(path,
                                         options,
                                         m_config,
                                         &profilePath,
                                         &profileBindingError)) {
        const QString message = QStringLiteral("下载 dry-run 失败：%1")
                .arg(profileBindingError.isEmpty()
                             ? QStringLiteral("未配置或未绑定已发布下载 Profile。")
                             : profileBindingError);
        localReport.insert(QStringLiteral("mode"), QStringLiteral("profile"));
        localReport.insert(QStringLiteral("valid"), false);
        localReport.insert(QStringLiteral("errors"), QVariantList{message});
        if (report)
            *report = localReport;
        if (errorMessage)
            *errorMessage = message;
        if (operationError)
            *operationError = CommError(CommProtocolType::ModbusRTU,
                                        CommErrorCode::InvalidConfig,
                                        message);
        return false;
    }
    localReport.insert(QStringLiteral("profilePath"), profilePath);

    DownloadProfile profile;
    QString profileError;
    if (!DownloadProfile::fromJsonFile(profilePath, profile, &profileError)) {
        const QString message = QStringLiteral("下载 dry-run 失败：profile 读取失败：%1").arg(profileError);
        if (errorMessage) {
            *errorMessage = message;
        }
        if (operationError) {
            *operationError = CommError(CommProtocolType::ModbusRTU,
                                        CommErrorCode::InvalidConfig,
                                        message,
                                        profilePath);
        }
        return false;
    }

    QStringList errors;
    const bool valid = validateDownloadProfilePlan(profile, payload, &localReport, &errors);
    localReport.insert(QStringLiteral("valid"), valid);
    localReport.insert(QStringLiteral("errors"), stringsToVariantList(errors));
    if (report) {
        *report = localReport;
    }
    if (!valid) {
        const QString message = QStringLiteral("下载 dry-run 失败：%1").arg(errors.join(QStringLiteral("; ")));
        if (errorMessage) {
            *errorMessage = message;
        }
        if (operationError) {
            *operationError = CommError(CommProtocolType::ModbusRTU,
                                        CommErrorCode::InvalidConfig,
                                        message,
                                        profilePath);
        }
        return false;
    }

    if (operationError) {
        *operationError = CommError();
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("下载 dry-run 通过。");
    }
    return true;
}

bool ControllerDeviceBackend::validateDownloadProfilePlan(const DownloadProfile& profile,
                                                          const QByteArray& payload,
                                                          QVariantMap* report,
                                                          QStringList* errors) const
{
    QStringList localErrors;
    QVariantList stepReports;
    int sendChunkCount = 0;
    int totalPackets = 0;
    const int payloadRegisters = bytesToRegisters(payload).size();

    QStringList structuralErrors;
    profile.validate(&structuralErrors);
    localErrors.append(structuralErrors);

    if (!ControllerDebugProtocol::canReadDeviceId(profile.slaveId)) {
        localErrors << QStringLiteral("profile.slaveId 必须为 1..63，当前为 %1").arg(profile.slaveId);
    }
    if (profile.steps.isEmpty()) {
        localErrors << QStringLiteral("profile.steps 为空");
    }
    if (payload.isEmpty()) {
        localErrors << QStringLiteral("下载产物为空，拒绝执行");
    }

    for (int i = 0; i < profile.steps.size(); ++i) {
        const DownloadProfile::Step& step = profile.steps.at(i);
        const QVariantMap params = profile.resolvedParams(step);
        QStringList stepErrors;
        QVariantMap stepReport;
        stepReport.insert(QStringLiteral("index"), i);
        stepReport.insert(QStringLiteral("type"), DownloadProfile::stepTypeToString(step.type));

        const QString layer = params.value(QStringLiteral("layer"), QStringLiteral("target"))
                                      .toString().trimmed().toLower();
        const int defaultSlave = (layer == QStringLiteral("controller") || layer == QStringLiteral("ctrl"))
                ? deviceId() : targetDeviceId();
        const int slaveId = params.value(QStringLiteral("slaveId"), defaultSlave).toInt();
        if (!ControllerDebugProtocol::canReadDeviceId(slaveId)) {
            stepErrors << QStringLiteral("slaveId 必须为 1..63，当前为 %1").arg(slaveId);
        }
        const QString addressingMode = m_config.bridge.parameters
                                               .value(QStringLiteral("addressing")).toMap()
                                               .value(QStringLiteral("mode")).toString();
        if (layer == QStringLiteral("target")
                && addressingMode.compare(QStringLiteral("ControllerOnlyWithTargetSelectReg"),
                                          Qt::CaseInsensitive) == 0) {
            stepErrors << QStringLiteral("控制器后端不支持目标选择寄存器寻址，请使用 Bridge 下载入口");
        }

        switch (step.type) {
        case DownloadProfile::StepType::Enter:
        case DownloadProfile::StepType::Finalize: {
            const QString op = params.value(QStringLiteral("op"), QStringLiteral("writeRegs"))
                                       .toString().trimmed();
            if (op.compare(QStringLiteral("writeRegs"), Qt::CaseInsensitive) != 0) {
                stepErrors << QStringLiteral("控制器后端仅支持 op=writeRegs");
            }
            if (!params.value(QStringLiteral("needResponse"), true).toBool()) {
                stepErrors << QStringLiteral("控制器后端不支持 needResponse=false");
            }
            const int stepAddress = params.value(QStringLiteral("address"), -1).toInt();
            if (stepAddress < 0 || stepAddress > 65535) {
                stepErrors << QStringLiteral("缺少 address");
            }
            if (!params.contains(QStringLiteral("values")) || variantToRegisters(params.value(QStringLiteral("values"))).isEmpty()) {
                stepErrors << QStringLiteral("缺少 values");
            }
            const int valueCount = variantToRegisters(params.value(QStringLiteral("values"))).size();
            if (stepAddress >= 0 && valueCount > 0 && stepAddress + valueCount > 65536) {
                stepErrors << QStringLiteral("address 与 values 长度超出 16 位地址空间");
            }
            break;
        }
        case DownloadProfile::StepType::Poll: {
            const int stepAddress = params.value(QStringLiteral("address"), -1).toInt();
            if (stepAddress < 0 || stepAddress > 65535) {
                stepErrors << QStringLiteral("缺少 address");
            }
            if (!params.contains(QStringLiteral("expected")) || variantToRegisters(params.value(QStringLiteral("expected"))).isEmpty()) {
                stepErrors << QStringLiteral("缺少 expected");
            }
            if (params.value(QStringLiteral("count"), 1).toInt() <= 0
                    || params.value(QStringLiteral("count"), 1).toInt() > 125) {
                stepErrors << QStringLiteral("count 必须为 1..125");
            }
            if (stepAddress >= 0 && stepAddress + params.value(QStringLiteral("count"), 1).toInt() > 65536) {
                stepErrors << QStringLiteral("address+count 超出 16 位地址空间");
            }
            const QVariant expected = params.value(QStringLiteral("expected"));
            if (expected.type() == QVariant::List
                    && expected.toList().size() != params.value(QStringLiteral("count"), 1).toInt()) {
                stepErrors << QStringLiteral("expected 长度必须等于 count");
            }
            if (params.contains(QStringLiteral("timeoutMs"))) {
                const QVariant timeoutVal = params.value(QStringLiteral("timeoutMs"));
                if (timeoutVal.type() == QVariant::Bool || timeoutVal.toInt() < 10 || timeoutVal.toInt() > 600000) {
                    stepErrors << QStringLiteral("timeoutMs 必须为 10..600000 ms 的有效整数");
                }
            }
            if (params.contains(QStringLiteral("pollIntervalMs"))) {
                const QVariant intervalVal = params.value(QStringLiteral("pollIntervalMs"));
                if (intervalVal.type() == QVariant::Bool || intervalVal.toInt() < 1 || intervalVal.toInt() > 60000) {
                    stepErrors << QStringLiteral("pollIntervalMs 必须为 1..60000 ms 的有效整数");
                }
            }
            break;
        }
        case DownloadProfile::StepType::SendChunk: {
            ++sendChunkCount;
            const int dataAddress = params.value(QStringLiteral("dataAddress"), -1).toInt();
            const int chunkWords = params.value(QStringLiteral("chunkWords"), 60).toInt();
            if (dataAddress < 0) {
                stepErrors << QStringLiteral("缺少 dataAddress");
            }
            if (dataAddress > 65535) {
                stepErrors << QStringLiteral("dataAddress 超出 16 位范围");
            }
            if (chunkWords <= 0 || chunkWords > 125) {
                stepErrors << QStringLiteral("chunkWords 必须为 1..125");
            } else if (dataAddress >= 0 && dataAddress + chunkWords > 65536) {
                stepErrors << QStringLiteral("dataAddress 与 chunkWords 超出 16 位地址空间");
            } else {
                const int bytesPerChunk = chunkWords * 2;
                const int packets = payload.isEmpty()
                        ? 0
                        : ((payload.size() + bytesPerChunk - 1) / bytesPerChunk);
                const int packetIndexBase = params.value(QStringLiteral("packetIndexBase"), 0).toInt();
                if (packetIndexBase < 0 || packetIndexBase + qMax(0, packets - 1) > 65535) {
                    stepErrors << QStringLiteral("packetIndex 超出 16 位范围");
                }
                if (params.contains(QStringLiteral("packetOffsetAddress")) && payload.size() > 65535) {
                    stepErrors << QStringLiteral("packetOffset 超出 16 位范围");
                }
                totalPackets += packets;
                stepReport.insert(QStringLiteral("packetCount"), packets);
            }
            if (!params.value(QStringLiteral("needResponse"), true).toBool()) {
                stepErrors << QStringLiteral("控制器后端不支持 needResponse=false");
            }
            break;
        }
        case DownloadProfile::StepType::QueryResult: {
            const int queryAddress = params.value(QStringLiteral("address"), -1).toInt();
            if (queryAddress < 0 || queryAddress > 65535) {
                stepErrors << QStringLiteral("缺少 address");
            }
            if (params.value(QStringLiteral("count"), 1).toInt() <= 0
                    || params.value(QStringLiteral("count"), 1).toInt() > 125) {
                stepErrors << QStringLiteral("count 必须为 1..125");
            }
            if (queryAddress >= 0 && queryAddress + params.value(QStringLiteral("count"), 1).toInt() > 65536) {
                stepErrors << QStringLiteral("address+count 超出 16 位地址空间");
            }
            break;
        }
        }

        if (!stepErrors.isEmpty()) {
            localErrors << QStringLiteral("step %1(%2)：%3")
                           .arg(i + 1)
                           .arg(DownloadProfile::stepTypeToString(step.type))
                           .arg(stepErrors.join(QStringLiteral(", ")));
        }
        stepReport.insert(QStringLiteral("errors"), stringsToVariantList(stepErrors));
        stepReports.append(stepReport);
    }

    if (sendChunkCount == 0) localErrors << QStringLiteral("profile 至少需要一个 sendChunk 步骤");
    if (report) {
        report->insert(QStringLiteral("profileName"), profile.name);
        report->insert(QStringLiteral("profileSlaveId"), profile.slaveId);
        report->insert(QStringLiteral("stepCount"), profile.steps.size());
        report->insert(QStringLiteral("sendChunkStepCount"), sendChunkCount);
        report->insert(QStringLiteral("payloadRegisters"), payloadRegisters);
        report->insert(QStringLiteral("estimatedPacketCount"), totalPackets);
        report->insert(QStringLiteral("steps"), stepReports);
    }
    if (errors) {
        *errors = localErrors;
    }
    return localErrors.isEmpty();
}

bool ControllerDeviceBackend::executeDownloadProfile(const DownloadProfile& profile,
                                                     const QByteArray& payload,
                                                     QString* errorMessage,
                                                     CommError* operationError)
{
    QStringList planErrors;
    if (!validateDownloadProfilePlan(profile, payload, nullptr, &planErrors)) {
        return setOperationError(CommErrorCode::InvalidConfig,
                                 QStringLiteral("下载配置无效：%1")
                                         .arg(planErrors.join(QStringLiteral("; "))),
                                 operationError,
                                 errorMessage);
    }

    QString probeError;
    if (!probeTarget(&probeError)) {
        return setOperationError(CommErrorCode::DeviceNotFound,
                                 QStringLiteral("下载前目标侧探测失败：%1").arg(probeError),
                                 operationError,
                                 errorMessage);
    }

    for (int i = 0; i < profile.steps.size(); ++i) {
        if (m_downloadCancelled.load(std::memory_order_acquire)) {
            return setOperationError(CommErrorCode::OperationCancelled,
                                     QStringLiteral("下载已取消。"),
                                     operationError,
                                     errorMessage);
        }

        if (!executeDownloadStep(profile, profile.steps.at(i), payload, i, errorMessage, operationError)) {
            return false;
        }
        if (m_downloadCancelled.load(std::memory_order_acquire)) {
            return setOperationError(CommErrorCode::OperationCancelled,
                                     QStringLiteral("下载已取消。"),
                                     operationError,
                                     errorMessage);
        }

        QMutexLocker lock(&m_mutex);
        m_downloadPercent = qMin(95, 10 + ((i + 1) * 80 / qMax(1, profile.steps.size())));
    }

    return true;
}

bool ControllerDeviceBackend::executeDownloadStep(const DownloadProfile& profile,
                                                  const DownloadProfile::Step& step,
                                                  const QByteArray& payload,
                                                  int stepIndex,
                                                  QString* errorMessage,
                                                  CommError* operationError)
{
    const QVariantMap params = profile.resolvedParams(step);
    const QString layer = params.value(QStringLiteral("layer"), QStringLiteral("target"))
                                  .toString().trimmed().toLower();
    const int defaultSlave = (layer == QStringLiteral("controller") || layer == QStringLiteral("ctrl"))
            ? deviceId() : targetDeviceId();
    const int slaveId = params.value(QStringLiteral("slaveId"), defaultSlave).toInt();
    const int address = params.value(QStringLiteral("address"), -1).toInt();

    auto failStep = [&](CommErrorCode code, const QString& message, const QString& details = QString()) {
        return setOperationError(code,
                                 QStringLiteral("下载步骤 %1 失败：%2").arg(stepIndex + 1).arg(message),
                                 operationError,
                                 errorMessage,
                                 details);
    };

    switch (step.type) {
    case DownloadProfile::StepType::Enter:
    case DownloadProfile::StepType::Finalize: {
        const QString op = params.value(QStringLiteral("op"), QStringLiteral("writeRegs"))
                                   .toString().trimmed();
        const QVector<quint16> values = variantToRegisters(params.value(QStringLiteral("values")));
        if (op.compare(QStringLiteral("writeRegs"), Qt::CaseInsensitive) != 0
                || !params.value(QStringLiteral("needResponse"), true).toBool()) {
            return failStep(CommErrorCode::InvalidConfig,
                            QStringLiteral("控制器后端仅支持需要响应的 writeRegs。"));
        }
        if (address < 0 || address > 65535 || values.isEmpty()
                || address + values.size() > 65536) {
            return failStep(CommErrorCode::InvalidConfig, QStringLiteral("缺少 address 或 values。"));
        }
        if (!m_client->writeHoldingRegisters(slaveId, address, values)) {
            const CommError err = currentDebugError(QStringLiteral("写控制寄存器失败。"));
            return failStep(err.code, err.message, err.details);
        }
        return true;
    }
    case DownloadProfile::StepType::Poll: {
        const int count = params.value(QStringLiteral("count"), 1).toInt();
        const int timeoutMs = qBound(10, params.value(QStringLiteral("timeoutMs"), 3000).toInt(), 600000);
        const int intervalMs = qBound(1, params.value(QStringLiteral("pollIntervalMs"), 100).toInt(), 60000);
        const QVector<quint16> expected = variantToRegisters(params.value(QStringLiteral("expected")));
        if (address < 0 || address > 65535 || count <= 0 || count > 125
                || address + count > 65536 || expected.isEmpty()) {
            return failStep(CommErrorCode::InvalidConfig, QStringLiteral("缺少 address 或 expected。"));
        }

        QElapsedTimer timer;
        timer.start();
        while (true) {
            if (m_downloadCancelled.load(std::memory_order_acquire)) {
                return failStep(CommErrorCode::OperationCancelled, QStringLiteral("操作已取消。"));
            }
            if (timer.elapsed() > timeoutMs) {
                return failStep(CommErrorCode::ReceiveTimeout, QStringLiteral("轮询等待超时。"));
            }

            m_client->setRequestBudget(qMax(0, timeoutMs - static_cast<int>(timer.elapsed())), &m_downloadCancelled);
            struct BudgetReset { ControllerDebugClient* client; const std::atomic_bool* cancelled; ~BudgetReset() { client->setRequestBudget(-1, cancelled); } } reset{m_client, &m_downloadCancelled};
            QVector<quint16> values;
            if (!m_client->readHoldingRegisters(slaveId, address, count, &values)) {
                const CommError err = currentDebugError(QStringLiteral("轮询寄存器失败。"));
                return failStep(err.code, err.message, err.details);
            }
            if (m_downloadCancelled.load()) return failStep(CommErrorCode::OperationCancelled, QStringLiteral("Download cancelled"));
            if (timer.elapsed() >= timeoutMs) return failStep(CommErrorCode::ReceiveTimeout, QStringLiteral("Poll deadline exceeded"));
            bool matched = values.size() >= expected.size();
            for (int i = 0; matched && i < expected.size(); ++i) {
                matched = (values.at(i) == expected.at(i));
            }
            if (matched) {
                return true;
            }

            const qint64 remaining = timeoutMs - timer.elapsed();
            if (remaining <= 0) {
                return failStep(CommErrorCode::ReceiveTimeout, QStringLiteral("轮询等待超时。"));
            }

            const int waitTargetMs = static_cast<int>(qMin<qint64>(intervalMs, remaining));
            const int sliceMs = 20;
            int slept = 0;
            while (slept < waitTargetMs) {
                if (m_downloadCancelled.load(std::memory_order_acquire)) {
                    return failStep(CommErrorCode::OperationCancelled, QStringLiteral("操作已取消。"));
                }
                const int currentSlice = qMin(sliceMs, waitTargetMs - slept);
                QThread::msleep(static_cast<unsigned long>(currentSlice));
                slept += currentSlice;
            }
        }
    }
    case DownloadProfile::StepType::SendChunk: {
        const int dataAddress = params.value(QStringLiteral("dataAddress"), -1).toInt();
        const int chunkWords = params.value(QStringLiteral("chunkWords"), 60).toInt();
        const bool needResponse = params.value(QStringLiteral("needResponse"), true).toBool();
        const QString configuredByteOrder = m_config.bridge.parameters
                                                    .value(QStringLiteral("transfer")).toMap()
                                                    .value(QStringLiteral("byteOrder"),
                                                           QStringLiteral("BigEndian")).toString();
        const QString byteOrder = params.value(QStringLiteral("byteOrder"),
                                               configuredByteOrder).toString();
        const bool littleEndian = byteOrder.compare(QStringLiteral("LittleEndian"),
                                                    Qt::CaseInsensitive) == 0;
        if (dataAddress < 0 || dataAddress > 65535 || chunkWords <= 0 || chunkWords > 125
                || dataAddress + chunkWords > 65536 || !needResponse) {
            return failStep(CommErrorCode::InvalidConfig,
                            QStringLiteral("dataAddress/chunkWords/needResponse 配置无效。"));
        }
        if (payload.isEmpty()) {
            return true;
        }

        const int packetIndexAddress = params.value(QStringLiteral("packetIndexAddress"), -1).toInt();
        const int packetLengthAddress = params.value(QStringLiteral("packetLengthAddress"), -1).toInt();
        const int packetCrcAddress = params.value(QStringLiteral("packetCrcAddress"), -1).toInt();
        const int packetOffsetAddress = params.value(QStringLiteral("packetOffsetAddress"), -1).toInt();
        const int packetIndexBase = params.value(QStringLiteral("packetIndexBase"), 0).toInt();
        const int bytesPerChunk = chunkWords * 2;
        const int packetCount = (payload.size() + bytesPerChunk - 1) / bytesPerChunk;

        if (packetIndexBase < 0 || packetIndexBase + qMax(0, packetCount - 1) > 65535
                || (packetOffsetAddress >= 0 && payload.size() > 65535)) {
            return failStep(CommErrorCode::InvalidConfig,
                            QStringLiteral("数据块头字段超出 16 位范围。"));
        }

        auto writeHeader = [&](int headerAddress, quint16 value, const QString& label) {
            if (headerAddress < 0) {
                return true;
            }
            if (headerAddress > 65535) {
                return failStep(CommErrorCode::InvalidConfig,
                                QStringLiteral("数据块头地址超出 16 位范围。"));
            }
            if (m_client->writeHoldingRegisters(slaveId, headerAddress, {value})) {
                return true;
            }
            const CommError err = currentDebugError(QStringLiteral("写%1失败。").arg(label));
            return failStep(err.code, err.message, err.details);
        };

        for (int packetIndex = 0; packetIndex < packetCount; ++packetIndex) {
            if (m_downloadCancelled.load(std::memory_order_acquire)) {
                return failStep(CommErrorCode::OperationCancelled, QStringLiteral("下载已取消。"));
            }
            const int offsetBytes = packetIndex * bytesPerChunk;
            const QByteArray chunkBytes = payload.mid(offsetBytes, bytesPerChunk);
            if (!writeHeader(packetIndexAddress,
                             static_cast<quint16>(packetIndexBase + packetIndex),
                             QStringLiteral("包序号"))
                    || !writeHeader(packetLengthAddress,
                                    static_cast<quint16>(chunkBytes.size()),
                                    QStringLiteral("包长度"))
                    || !writeHeader(packetCrcAddress,
                                    calcCrc16Modbus(chunkBytes),
                                    QStringLiteral("包 CRC"))
                    || !writeHeader(packetOffsetAddress,
                                    static_cast<quint16>(offsetBytes),
                                    QStringLiteral("包偏移"))) {
                return false;
            }

            const QVector<quint16> chunk = bytesToRegisters(chunkBytes, littleEndian);
            if (dataAddress + chunk.size() > 65536) {
                return failStep(CommErrorCode::InvalidConfig,
                                QStringLiteral("数据块地址范围超出 16 位地址空间。"));
            }
            if (!m_client->writeHoldingRegisters(slaveId, dataAddress, chunk)) {
                const CommError err = currentDebugError(QStringLiteral("发送数据块失败。"));
                return failStep(err.code, err.message, err.details);
            }
        }
        return true;
    }
    case DownloadProfile::StepType::QueryResult: {
        const int count = params.value(QStringLiteral("count"), 1).toInt();
        const QVector<quint16> expected = variantToRegisters(params.value(QStringLiteral("expected")));
        if (address < 0 || address > 65535 || count <= 0 || count > 125
                || address + count > 65536) {
            return failStep(CommErrorCode::InvalidConfig, QStringLiteral("缺少 address。"));
        }
        QVector<quint16> values;
        if (!m_client->readHoldingRegisters(slaveId, address, count, &values)) {
            const CommError err = currentDebugError(QStringLiteral("读取结果寄存器失败。"));
            return failStep(err.code, err.message, err.details);
        }
        if (!expected.isEmpty()) {
            bool matched = values.size() >= expected.size();
            for (int i = 0; matched && i < expected.size(); ++i) {
                matched = values.at(i) == expected.at(i);
            }
            if (!matched) {
                return failStep(CommErrorCode::ProtocolError, QStringLiteral("结果寄存器不符合预期。"));
            }
        }
        return true;
    }
    }

    return failStep(CommErrorCode::NotImplemented, QStringLiteral("未知下载步骤类型。"));
}
