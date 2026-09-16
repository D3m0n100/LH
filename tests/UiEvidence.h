#pragma once

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>
#include <QProcess>
#include <QScreen>
#include <QGuiApplication>
#include <QSysInfo>
#include <QUuid>
#include <QWidget>
#include <algorithm>

// Each test process owns a unique directory, including its own QtTest log.
namespace UiEvidence {
inline QString git(const QStringList& arguments)
{
    QProcess process;
    process.start(QStringLiteral("git"), arguments);
    if (!process.waitForFinished(5000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return {};
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

inline QString repositoryRoot()
{
    static const QString root = git({QStringLiteral("rev-parse"), QStringLiteral("--show-toplevel")});
    return root;
}

inline QString fingerprint()
{
    if (repositoryRoot().isEmpty())
        return {};
    QProcess process;
    process.start(QStringLiteral("git"), {QStringLiteral("-C"), repositoryRoot(), QStringLiteral("ls-files"),
        QStringLiteral("-co"), QStringLiteral("--exclude-standard"), QStringLiteral("-z"), QStringLiteral("--"),
        QStringLiteral("src"), QStringLiteral("tests"), QStringLiteral("resources"), QStringLiteral("cmake"),
        QStringLiteral("CMakeLists.txt")});
    if (!process.waitForFinished(5000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return {};
    auto paths = process.readAllStandardOutput().split('\0');
    paths.removeAll(QByteArray());
    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
    if (paths.isEmpty())
        return {};
    QCryptographicHash digest(QCryptographicHash::Sha256);
    for (const auto& relative : paths) {
        QFile file(QDir(repositoryRoot()).filePath(QString::fromUtf8(relative)));
        if (!file.open(QIODevice::ReadOnly))
            return {};
        // Length-prefixing content makes the framing unambiguous, including empty files.
        digest.addData(relative);
        digest.addData("\0", 1);
        digest.addData(QByteArray::number(file.size()));
        digest.addData("\0", 1);
        if (!digest.addData(&file) || file.error() != QFileDevice::NoError)
            return {};
    }
    return QString::fromLatin1(digest.result().toHex());
}

inline QString runId()
{
    static const QString id = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"))
        + QStringLiteral("_%1_").arg(QCoreApplication::applicationPid())
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    return id;
}

inline QString directory()
{
    static const QString path = QDir(qEnvironmentVariable("LH_UI_EVIDENCE_ROOT",
        QDir(repositoryRoot()).filePath(QStringLiteral(".md/acceptance_evidence/ui_20260913/runs"))))
        .absoluteFilePath(runId());
    return path;
}

inline bool writeJson(const QString& name, const QJsonObject& object)
{
    QFile file(QDir(directory()).filePath(name));
    // Never silently overwrite an earlier page or record, even in the same run.
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        return false;
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    return file.write(bytes) == bytes.size() && file.flush();
}

inline bool initialize()
{
    const QString sourceHash = fingerprint();
    const QString head = git({QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
    if (sourceHash.size() != 64 || head.isEmpty() || !QDir().mkpath(directory()))
        return false;
    QJsonObject meta{{QStringLiteral("runId"), runId()},
        {QStringLiteral("parentRunId"), qEnvironmentVariable("LH_UI_PARENT_RUN")},
        {QStringLiteral("gitHead"), head}, {QStringLiteral("workspaceFingerprint"), sourceHash},
        {QStringLiteral("fingerprintAlgorithm"), QStringLiteral("SHA256(sorted unique git src/tests/resources/cmake/CMakeLists.txt: UTF8 path,NUL,decimal byte size,NUL,content)")},
        {QStringLiteral("timestampUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("qtVersion"), QString::fromLatin1(qVersion())},
        {QStringLiteral("os"), QSysInfo::prettyProductName()},
        {QStringLiteral("platformPlugin"), QGuiApplication::platformName()},
        {QStringLiteral("testLog"), QStringLiteral("test.log")},
        {QStringLiteral("scaleFactorEnvironment"), qEnvironmentVariable("QT_SCALE_FACTOR")},
        {QStringLiteral("matrixCase"), qEnvironmentVariable("LH_DPI_CASE")}};
    if (auto* screen = QGuiApplication::primaryScreen()) {
        meta.insert(QStringLiteral("screenLogicalWidth"), screen->size().width());
        meta.insert(QStringLiteral("screenLogicalHeight"), screen->size().height());
        meta.insert(QStringLiteral("screenDevicePixelRatio"), screen->devicePixelRatio());
    }
    return writeJson(QStringLiteral("run_environment.json"), meta);
}

inline bool save(const QPixmap& pixmap, const QString& requestedPath, const QString& page = {}, QWidget* widget = nullptr)
{
    const QString name = QFileInfo(requestedPath).fileName();
    const QString imagePath = QDir(directory()).filePath(name);
    if (name.isEmpty() || pixmap.isNull() || QFile::exists(imagePath) || !pixmap.save(imagePath))
        return false;
    const qreal dpr = widget ? widget->devicePixelRatioF() : pixmap.devicePixelRatioF();
    QJsonObject meta{{QStringLiteral("runId"), runId()},
        {QStringLiteral("pageState"), page.isEmpty() ? QFileInfo(name).completeBaseName() : page},
        {QStringLiteral("screenshot"), name},
        {QStringLiteral("logicalWidth"), widget ? widget->width() : qRound(pixmap.width() / dpr)},
        {QStringLiteral("logicalHeight"), widget ? widget->height() : qRound(pixmap.height() / dpr)},
        {QStringLiteral("pixelWidth"), pixmap.width()}, {QStringLiteral("pixelHeight"), pixmap.height()},
        {QStringLiteral("devicePixelRatio"), dpr},
        {QStringLiteral("timestampUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    return writeJson(QFileInfo(name).completeBaseName() + QStringLiteral(".json"), meta);
}
} // namespace UiEvidence
