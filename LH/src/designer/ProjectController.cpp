/**
 * @file ProjectController.cpp
 * @brief 项目控制器实现
 */

#include "ProjectController.h"
#include "DslScriptEditor.h"
#include "Common.h"
#include "TextEncoding.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSettings>
#include <QTextStream>
#include <QUuid>
#include <QDateTime>
#include <QSet>
#include <QMap>
#include <QRegularExpression>
#include <algorithm>

// ================= 鏋勯€?/ 鏋愭瀯 =================

ProjectController::ProjectController(QObject* parent)
    : QObject(parent)
    , m_defaultProjectDir(QDir::homePath())
{
    LOG_DEBUG("ProjectController 已创建");
}

ProjectController::~ProjectController()
{
    LOG_DEBUG("ProjectController 已销毁");
}


// ================= DSL 编辑器绑定 =================

void ProjectController::setDslEditor(DslScriptEditor* editor)
{
    m_dslEditor = editor;
    // Sync mappings to editor once the editor is available.
    syncDslMappingsToEditor();
}

void ProjectController::setCurrentScriptFile(const QString& scriptFile)
{
    if (m_currentScriptFile == scriptFile) {
        return;
    }

    m_currentScriptFile = scriptFile;
    syncScriptConfigFields();
}

// ================= 椤圭洰鐘舵€?=================

void ProjectController::setModified(bool modified)
{
    if (m_modified != modified) {
        m_modified = modified;
        emit modifiedChanged(modified);
    }
}

// ================= 鏈€杩戦」鐩垪琛?=================

void ProjectController::loadRecentProjects()
{
    QSettings settings("ServoValve", "ControlPlatform");
    m_recentProjects = settings.value("recentProjects").toStringList();
    LOG_DEBUG(QString("已加载 %1 个最近项目").arg(m_recentProjects.size()));
}

void ProjectController::saveRecentProjects()
{
    QSettings settings("ServoValve", "ControlPlatform");
    settings.setValue("recentProjects", m_recentProjects);
    LOG_DEBUG("已保存最近项目列表");
}

void ProjectController::addToRecentProjects(const QString& path)
{
    m_recentProjects.removeAll(path);
    m_recentProjects.prepend(path);
    
    while (m_recentProjects.size() > MAX_RECENT_PROJECTS) {
        m_recentProjects.removeLast();
    }
    
    emit recentProjectsChanged(m_recentProjects);
}

// ================= 椤圭洰鎿嶄綔 =================

void ProjectController::createNewProject()
{
    QString projectName;
    bool accepted = false;
    emit projectNameRequired(projectName, accepted);

    if (!accepted || projectName.isEmpty()) {
        return;
    }

    QString projectDir;
    emit directorySelectionRequired("选择项目保存位置", m_defaultProjectDir,
                                    projectDir, accepted);

    if (!accepted || projectDir.isEmpty()) {
        return;
    }

    const QString fullPath = projectDir + "/" + projectName;
    QDir dir;
    if (!dir.mkpath(fullPath)) {
        emit errorOccurred("错误", "无法创建项目目录");
        return;
    }

    m_runtimeConfig.clear();
    m_runtimeConfig.projectName = projectName;
    m_runtimeConfig.applyEmptyTemplates();
    m_runtimeConfig.lastModified = QDateTime::currentDateTime();

    const QString scriptPath = fullPath + "/main.dsl";
    QFile scriptFile(scriptPath);
    if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&scriptFile);
        TextEncoding::setUtf8(stream);
        stream << QString::fromUtf8(u8"// ") << projectName << QString::fromUtf8(u8" - DSL脚本\n");
        stream << QString::fromUtf8(u8"// 创建时间: ")
               << QDateTime::currentDateTime().toString(Qt::ISODate)
               << QString::fromUtf8(u8"\n");
        stream << "\n";
        stream << QString::fromUtf8(u8"// 在此编写 DSL 组态代码，或从左侧函数列表拖拽组件\n");
        scriptFile.close();
    }

    m_runtimeConfig.dslScriptPath = scriptPath;
    m_runtimeConfig.mainScriptPath = scriptPath;
    m_runtimeConfig.scriptFiles = QStringList{scriptPath};
    saveProjectConfig(fullPath);

    m_currentProject = fullPath;
    m_currentScriptFile = scriptPath;

    QFile file(scriptPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString content = TextEncoding::decodeUtf8WithLocalFallback(file.readAll());
        file.close();
        emit scriptLoadRequired(scriptPath, content);
    }

    setModified(false);
    addToRecentProjects(fullPath);

    emit logMessage(timestampedMessage(QString("已创建新项目: %1").arg(projectName)));
    emit projectCreated(fullPath, projectName);
}
void ProjectController::openProject()
{
    QString projectPath;
    bool accepted = false;
    emit directorySelectionRequired("打开项目", m_defaultProjectDir,
                                     projectPath, accepted);
    
    if (!accepted || projectPath.isEmpty()) {
        return;
    }
    
    openProjectFromPath(projectPath);
}

bool ProjectController::openProjectFromPath(const QString& projectPath)
{
    // Check whether project config exists.
    QString configPath = projectPath + "/project_config.json";
    if (!QFile::exists(configPath)) {
        emit warningOccurred("警告", 
            "所选目录不是有效项目目录（缺少 project_config.json）。");
        return false;
    }
    
    // 鍔犺浇椤圭洰閰嶇疆
    if (!loadProjectConfig(projectPath)) {
        return false;
    }
    
    m_currentProject = projectPath;
    
    // 鍔犺浇 DSL 鑴氭湰
    syncScriptConfigFields();
    if (!m_runtimeConfig.mainScriptPath.isEmpty()) {
        m_currentScriptFile = m_runtimeConfig.mainScriptPath;
        
        QFile scriptFile(m_currentScriptFile);
        if (scriptFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = TextEncoding::decodeUtf8WithLocalFallback(scriptFile.readAll());
            scriptFile.close();
            emit scriptLoadRequired(m_currentScriptFile, content);
        }
    }
    
    setModified(false);
    addToRecentProjects(projectPath);
    
    emit logMessage(timestampedMessage(QString("已打开项目: %1").arg(m_runtimeConfig.projectName)));
    emit projectOpened(m_runtimeConfig);
    
    return true;
}

bool ProjectController::saveProject()
{
    if (m_currentProject.isEmpty()) {
        emit warningOccurred("警告", "没有打开的项目。");
        return false;
    }
    
    // Sync mappings from editor first.
    syncDslMappingsFromEditor();
    syncDslMappingsToEditor();
    
    // 淇濆瓨 DSL 鑴氭湰锛堥渶瑕佷粠缂栬緫鍣ㄨ幏鍙栧唴瀹癸紝閫氳繃淇″彿锛?    // 杩欓儴鍒嗙敱 MainWindow 澶勭悊锛屽洜涓洪渶瑕佽闂紪杈戝櫒
    
    // 淇濆瓨椤圭洰閰嶇疆
    if (!saveProjectConfig(m_currentProject)) {
        return false;
    }
    
    setModified(false);
    
    emit logMessage(timestampedMessage("项目已保存"));
    emit projectSaved();
    
    return true;
}

void ProjectController::closeProject()
{
    if (m_modified) {
        bool shouldSave = false;
        bool cancelled = false;
        emit saveConfirmationRequired(shouldSave, cancelled);
        
        if (cancelled) {
            return;
        }
        
        if (shouldSave) {
            saveProject();
        }
    }
    
    m_currentProject.clear();
    m_currentScriptFile.clear();
    m_runtimeConfig.clear();
    
    emit editorClearRequired();
    
    setModified(false);
    
    emit logMessage(timestampedMessage("项目已关闭"));
    emit projectClosed();
}

void ProjectController::openRecentProject(const QString& path)
{
    if (!QDir(path).exists()) {
        emit warningOccurred("警告", "项目目录不存在: " + path);
        m_recentProjects.removeAll(path);
        emit recentProjectsChanged(m_recentProjects);
        return;
    }
    
    openProjectFromPath(path);
}

// ================= 閰嶇疆璇诲啓 =================

bool ProjectController::loadProjectConfig(const QString& projectDir)
{
    QString configPath = projectDir + "/project_config.json";
    QFile file(configPath);
    
    if (!file.exists()) {
        emit logMessage(timestampedMessage(QString("项目配置文件不存在: %1").arg(configPath)));
        return false;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        emit logMessage(timestampedMessage(QString("无法打开项目配置文件: %1").arg(file.errorString())));
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        emit logMessage(timestampedMessage(QString("解析项目配置文件失败: %1").arg(error.errorString())));
        return false;
    }
    
    m_runtimeConfig = ProjectRuntimeConfig::fromJson(doc.object());
    
    emit logMessage(timestampedMessage(
        QString("已加载项目配置: %1 (包含 %2 个监控通道, %3 个组态映射)")
            .arg(m_runtimeConfig.projectName)
            .arg(m_runtimeConfig.providers.size())
            .arg(m_runtimeConfig.dslMappings.size())));
    
    // Sync mappings to editor.
    syncDslMappingsToEditor();
    
    return true;
}

bool ProjectController::saveProjectConfig(const QString& projectDir)
{
    syncScriptConfigFields();
    m_runtimeConfig.lastModified = QDateTime::currentDateTime();
    
    QString configPath = projectDir + "/project_config.json";
    QFile file(configPath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        emit logMessage(timestampedMessage(QString("无法保存项目配置文件: %1").arg(file.errorString())));
        return false;
    }
    
    QJsonDocument doc(m_runtimeConfig.toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    emit logMessage(timestampedMessage("项目配置已保存"));
    
    return true;
}

void ProjectController::syncScriptConfigFields()
{
    if (!m_currentScriptFile.isEmpty()) {
        m_runtimeConfig.mainScriptPath = m_currentScriptFile;
    }

    if (m_runtimeConfig.mainScriptPath.isEmpty()) {
        m_runtimeConfig.mainScriptPath = m_runtimeConfig.dslScriptPath;
    }

    if (m_runtimeConfig.scriptFiles.isEmpty() && !m_runtimeConfig.mainScriptPath.isEmpty()) {
        m_runtimeConfig.scriptFiles.append(m_runtimeConfig.mainScriptPath);
    }

    if (!m_runtimeConfig.mainScriptPath.isEmpty()) {
        m_runtimeConfig.scriptFiles.removeAll(m_runtimeConfig.mainScriptPath);
        m_runtimeConfig.scriptFiles.prepend(m_runtimeConfig.mainScriptPath);
    }

    m_runtimeConfig.dslScriptPath = m_runtimeConfig.mainScriptPath;
}

// ================= DSL 鏄犲皠鍚屾 =================

void ProjectController::syncDslMappingsFromEditor()
{
    if (!m_dslEditor) {
        return;
    }

    // Get insert records from editor (including mappingId).
    QList<DslInsertRecord> records = m_dslEditor->insertRecords();
    if (records.isEmpty()) {
        return;
    }

    for (const auto& record : records) {
        const QString rid = record.mappingId;

        auto it = std::find_if(m_runtimeConfig.dslMappings.begin(), m_runtimeConfig.dslMappings.end(),
                               [&](const DslMappingEntry& m) {
                                   if (!rid.isEmpty()) {
                                       return m.id == rid;
                                   }
                                   return m.snippetId == record.snippetId && m.lineNumber == record.lineNumber;
                               });

        if (it == m_runtimeConfig.dslMappings.end()) {
            m_runtimeConfig.dslMappings.append(createMappingFromInsertRecord(record));
        } else {
            // 鏇存柊宸叉湁鏄犲皠瀛楁锛堥槻姝俊鎭繃鏈燂級
            if (!rid.isEmpty()) {
                it->id = rid;
            }
            if (!record.snippetId.isEmpty()) {
                it->snippetId = record.snippetId;
            }
            if (!record.snippetName.isEmpty()) {
                it->snippetName = record.snippetName;
            }
            if (record.lineNumber > 0) {
                it->lineNumber = record.lineNumber;
            }
            if (!record.generatedCode.isEmpty()) {
                it->generatedCode = record.generatedCode;
            }
        }
    }

    // De-duplicate by mapping id.
    QSet<QString> seen;
    QList<DslMappingEntry> dedup;
    dedup.reserve(m_runtimeConfig.dslMappings.size());

    for (const auto& m : m_runtimeConfig.dslMappings) {
        DslMappingEntry copy = m;
        if (copy.id.isEmpty()) {
            copy.id = DslMappingEntry::generateId();
        }
        if (seen.contains(copy.id)) {
            continue;
        }
        seen.insert(copy.id);
        dedup.append(copy);
    }
    m_runtimeConfig.dslMappings = dedup;
}

void ProjectController::syncDslMappingsToEditor()
{
    if (!m_dslEditor) {
        return;
    }

    // 1) Scan script markers: mappingId -> marker line (1-based).
    QMap<QString, int> markerMap = m_dslEditor->scanDslMappingMarkers();

    // 2) Repair/align runtimeConfig.dslMappings and ensure ids.
    QSet<QString> mappingIds;
    mappingIds.reserve(m_runtimeConfig.dslMappings.size());

    for (auto& mapping : m_runtimeConfig.dslMappings) {
        if (mapping.id.isEmpty()) {
            mapping.id = DslMappingEntry::generateId();
        }
        mappingIds.insert(mapping.id);

        // 鑻ヨ剼鏈腑瀛樺湪 marker锛屽垯浠?marker 琛屽彿涓哄噯鏍℃ lineNumber锛堜唬鐮侀€氬父鍦?marker 涓嬩竴琛岋級
        if (markerMap.contains(mapping.id)) {
            const int markerLine = markerMap.value(mapping.id);
            mapping.lineNumber = markerLine + 1;
            continue;
        }

        // Marker comments are legacy-only and must not be written back to the DSL script.
    }

    // 3) Recover minimal mappings from markers found in script.
    const QStringList lines = m_dslEditor->currentScript().split('\n');

    auto parseDslCall = [&](int codeLine, DslMappingEntry& out) {
        if (codeLine < 1 || codeLine > lines.size()) {
            return;
        }
        const QString line = lines.at(codeLine - 1);

        // 杞婚噺瑙ｆ瀽锛歺xx = analog_input( ... )
        static const QRegularExpression re(R"(^\s*([A-Za-z_]\w*)\s*=\s*([A-Za-z_]\w*)\s*\()");
        QRegularExpressionMatch m = re.match(line);
        if (m.hasMatch()) {
            out.channelName = m.captured(1);
            out.snippetId = m.captured(2);
            out.snippetName = out.snippetId;
        }
    };

    for (auto it = markerMap.begin(); it != markerMap.end(); ++it) {
        const QString id = it.key();
        const int markerLine = it.value();
        if (mappingIds.contains(id)) {
            continue;
        }

        DslMappingEntry entry;
        entry.id = id;
        entry.lineNumber = markerLine + 1;
        entry.createTime = QDateTime::currentDateTime();
        entry.metadata["recoveredFromScript"] = true;

        parseDslCall(entry.lineNumber, entry);

        m_runtimeConfig.dslMappings.append(entry);
        mappingIds.insert(id);
    }

    // 4) Sync mappings back to editor for highlighting/navigation.
    m_dslEditor->setDslMappings(m_runtimeConfig.dslMappings);
}

DslMappingEntry ProjectController::createMappingFromInsertRecord(const DslInsertRecord& record)
{
    DslMappingEntry entry;
        entry.id = record.mappingId.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : record.mappingId;
    entry.snippetId = record.snippetId;
    entry.snippetName = record.snippetName;
    entry.lineNumber = record.lineNumber;
    entry.generatedCode = record.generatedCode;
    entry.createTime = record.insertTime;
    
    // 浠?snippet 鑾峰彇棰濆淇℃伅
    if (m_dslEditor) {
        FunctionSnippet snippet = m_dslEditor->snippetById(record.snippetId);
        entry.unit = snippet.unit;
        entry.periodMs = snippet.defaultPeriodMs;
        
        // 鏍规嵁缁勪欢绫诲瀷鎺ㄦ柇閫氶亾鍚嶅拰淇″彿璺緞
        if (!snippet.name.isEmpty()) {
            entry.channelName = QString("%1_%2").arg(snippet.name).arg(record.lineNumber);
            entry.signalPath = QString("%1.%2").arg(snippet.category).arg(snippet.name);
        }
    }
    
    return entry;
}

// ================= 缁勬€佸悎娉曟€ф牎楠?=================

bool ProjectController::validateConfiguration(QStringList& errors)
{
    errors.clear();

    bool valid = true;

    if (!checkRequiredFields(m_runtimeConfig, errors)) {
        valid = false;
    }

    if (!checkDuplicateChannelBindings(m_runtimeConfig, errors)) {
        valid = false;
    }

    if (!checkHardwareResourceLimits(m_runtimeConfig, errors)) {
        valid = false;
    }

    if (!valid) {
        emit validationFailed(errors);
    }

    return valid;
}

bool ProjectController::checkRequiredFields(const ProjectRuntimeConfig& cfg, QStringList& errors) const
{
    bool valid = true;
    
    if (cfg.projectName.isEmpty()) {
        errors.append("项目名称不能为空");
        valid = false;
    }
    
    for (int i = 0; i < cfg.providers.size(); ++i) {
        const auto& provider = cfg.providers[i];
        
        if (provider.id.isEmpty()) {
            errors.append(QString("监控通道 #%1: ID 不能为空").arg(i + 1));
            valid = false;
        }
        
        if (provider.channelName.isEmpty()) {
            errors.append(QString("监控通道 #%1: 通道名称不能为空").arg(i + 1));
            valid = false;
        }
        
        if (provider.periodMs < MIN_SAMPLE_PERIOD_MS || provider.periodMs > MAX_SAMPLE_PERIOD_MS) {
            errors.append(QString("监控通道 '%1': 采样周期必须在 %2-%3 ms 之间")
                          .arg(provider.channelName)
                          .arg(MIN_SAMPLE_PERIOD_MS)
                          .arg(MAX_SAMPLE_PERIOD_MS));
            valid = false;
        }
    }
    
    for (int i = 0; i < cfg.dslMappings.size(); ++i) {
        const auto& mapping = cfg.dslMappings[i];
        
        if (mapping.snippetId.isEmpty()) {
            errors.append(QString("组态映射 #%1: 组件 ID 不能为空").arg(i + 1));
            valid = false;
        }
    }

    QSet<QString> variableNames;
    QSet<QString> variableIds;
    for (int i = 0; i < cfg.variables.size(); ++i) {
        const auto& variable = cfg.variables[i];

        if (variable.id.isEmpty()) {
            errors.append(QString("变量 #%1: ID 不能为空").arg(i + 1));
            valid = false;
        } else if (variableIds.contains(variable.id)) {
            errors.append(QString("重复的变量 ID: '%1'").arg(variable.id));
            valid = false;
        } else {
            variableIds.insert(variable.id);
        }

        if (variable.name.isEmpty()) {
            errors.append(QString("变量 #%1: 名称不能为空").arg(i + 1));
            valid = false;
        } else if (variableNames.contains(variable.name)) {
            errors.append(QString("重复的变量名称: '%1'").arg(variable.name));
            valid = false;
        } else {
            variableNames.insert(variable.name);
        }

        if (variable.dataType.isEmpty()) {
            errors.append(QString("变量 '%1': 数据类型不能为空").arg(variable.name.isEmpty() ? QString::number(i + 1) : variable.name));
            valid = false;
        }
    }

    QSet<QString> parameterNames;
    QSet<QString> parameterIds;
    for (int i = 0; i < cfg.parameters.size(); ++i) {
        const auto& parameter = cfg.parameters[i];

        if (parameter.id.isEmpty()) {
            errors.append(QString("参数 #%1: ID 不能为空").arg(i + 1));
            valid = false;
        } else if (parameterIds.contains(parameter.id)) {
            errors.append(QString("重复的参数 ID: '%1'").arg(parameter.id));
            valid = false;
        } else {
            parameterIds.insert(parameter.id);
        }

        if (parameter.name.isEmpty()) {
            errors.append(QString("参数 #%1: 名称不能为空").arg(i + 1));
            valid = false;
        } else if (parameterNames.contains(parameter.name)) {
            errors.append(QString("重复的参数名称: '%1'").arg(parameter.name));
            valid = false;
        } else {
            parameterNames.insert(parameter.name);
        }

        if (parameter.dataType.isEmpty()) {
            errors.append(QString("参数 '%1': 数据类型不能为空").arg(parameter.name.isEmpty() ? QString::number(i + 1) : parameter.name));
            valid = false;
        }
    }

    QSet<QString> resourceKeys;
    QSet<QString> resourceIds;
    for (int i = 0; i < cfg.resources.size(); ++i) {
        const auto& resource = cfg.resources[i];

        if (resource.id.isEmpty()) {
            errors.append(QString("资源绑定 #%1: ID 不能为空").arg(i + 1));
            valid = false;
        } else if (resourceIds.contains(resource.id)) {
            errors.append(QString("重复的资源绑定 ID: '%1'").arg(resource.id));
            valid = false;
        } else {
            resourceIds.insert(resource.id);
        }

        if (resource.resourceType.isEmpty()) {
            errors.append(QString("资源绑定 #%1: 资源类型不能为空").arg(i + 1));
            valid = false;
        }

        if (resource.channel.isEmpty()) {
            errors.append(QString("资源绑定 #%1: 通道不能为空").arg(i + 1));
            valid = false;
        }

        const QString resourceKey = resource.resourceType + ":" + resource.channel;
        if (resourceKeys.contains(resourceKey)) {
            errors.append(QString("重复的资源占用: '%1'").arg(resourceKey));
            valid = false;
        } else {
            resourceKeys.insert(resourceKey);
        }
    }
    
    return valid;
}

bool ProjectController::checkDuplicateChannelBindings(const ProjectRuntimeConfig& cfg, QStringList& errors) const
{
    bool valid = true;
    
    QSet<QString> channelNames;
    for (const auto& provider : cfg.providers) {
        if (channelNames.contains(provider.channelName)) {
            errors.append(QString("重复的监控通道名称: '%1'").arg(provider.channelName));
            valid = false;
        } else {
            channelNames.insert(provider.channelName);
        }
    }
    
    QSet<QString> providerIds;
    for (const auto& provider : cfg.providers) {
        if (providerIds.contains(provider.id)) {
            errors.append(QString("重复的监控通道 ID: '%1'").arg(provider.id));
            valid = false;
        } else {
            providerIds.insert(provider.id);
        }
    }
    
    QSet<QString> mappingIds;
    for (const auto& mapping : cfg.dslMappings) {
        if (mappingIds.contains(mapping.id)) {
            errors.append(QString("重复的组态映射 ID: '%1'").arg(mapping.id));
            valid = false;
        } else {
            mappingIds.insert(mapping.id);
        }
    }
    
    QMap<QString, int> signalPathCount;
    for (const auto& mapping : cfg.dslMappings) {
        if (!mapping.signalPath.isEmpty()) {
            signalPathCount[mapping.signalPath]++;
        }
    }
    
    for (auto it = signalPathCount.begin(); it != signalPathCount.end(); ++it) {
        if (it.value() > 1) {
            errors.append(QString("信号路径 '%1' 被绑定了 %2 次，可能导致冲突")
                          .arg(it.key())
                          .arg(it.value()));
        }
    }
    
    return valid;
}

bool ProjectController::checkHardwareResourceLimits(const ProjectRuntimeConfig& cfg, QStringList& errors) const
{
    bool valid = true;
    
    int analogInputCount = 0;
    int analogOutputCount = 0;
    int digitalInputCount = 0;
    int digitalOutputCount = 0;
    
    for (const auto& mapping : cfg.dslMappings) {
        const QString snippetId = mapping.snippetId.trimmed();
        if (snippetId.contains("analog_input", Qt::CaseInsensitive)
            || snippetId.compare(QStringLiteral("_DrvAI"), Qt::CaseInsensitive) == 0) {
            analogInputCount++;
        } else if (snippetId.contains("analog_output", Qt::CaseInsensitive)
                   || snippetId.compare(QStringLiteral("_DrvAO"), Qt::CaseInsensitive) == 0) {
            analogOutputCount++;
        } else if (snippetId.contains("digital_input", Qt::CaseInsensitive)
                   || snippetId.compare(QStringLiteral("_DrvDI"), Qt::CaseInsensitive) == 0) {
            digitalInputCount++;
        } else if (snippetId.contains("digital_output", Qt::CaseInsensitive)
                   || snippetId.compare(QStringLiteral("_DrvDO"), Qt::CaseInsensitive) == 0) {
            digitalOutputCount++;
        }
    }

    for (const auto& resource : cfg.resources) {
        const QString type = resource.resourceType.trimmed();
        if (type.compare(QStringLiteral("AI"), Qt::CaseInsensitive) == 0) {
            analogInputCount++;
        } else if (type.compare(QStringLiteral("AO"), Qt::CaseInsensitive) == 0) {
            analogOutputCount++;
        } else if (type.compare(QStringLiteral("DI"), Qt::CaseInsensitive) == 0) {
            digitalInputCount++;
        } else if (type.compare(QStringLiteral("DO"), Qt::CaseInsensitive) == 0) {
            digitalOutputCount++;
        }
    }
    
    if (analogInputCount + analogOutputCount > MAX_ANALOG_CHANNELS) {
        errors.append(QString("模拟量通道数量 (%1) 超出限制 (%2)")
                      .arg(analogInputCount + analogOutputCount)
                      .arg(MAX_ANALOG_CHANNELS));
        valid = false;
    }
    
    if (digitalInputCount + digitalOutputCount > MAX_DIGITAL_CHANNELS) {
        errors.append(QString("数字量通道数量 (%1) 超出限制 (%2)")
                      .arg(digitalInputCount + digitalOutputCount)
                      .arg(MAX_DIGITAL_CHANNELS));
        valid = false;
    }
    
    if (cfg.providers.size() > MAX_MONITOR_CHANNELS) {
        errors.append(QString("监控通道总数 (%1) 超出限制 (%2)")
                      .arg(cfg.providers.size())
                      .arg(MAX_MONITOR_CHANNELS));
        valid = false;
    }
    
    return valid;
}

// ================= 杈呭姪鏂规硶 =================

QString ProjectController::timestampedMessage(const QString& msg) const
{
    return QString("[%1] %2")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
        .arg(msg);
}



