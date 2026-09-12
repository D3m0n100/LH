/**
 * @file SnippetRepository.cpp
 * @brief Snippet 仓库实现
 */

#include "SnippetRepository.h"
#include "DslCompletionEngine.h"  // 需要 FunctionSnippet 定义
#include "Common.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>

// ================= 常量定义 =================

namespace {
    const QString DEFAULT_SNIPPETS_RESOURCE = QStringLiteral(":/snippets/default_snippets.json");
    const QString PROJECT_SNIPPETS_FILENAME = QStringLiteral("snippets.json");
}

// ================= 构造 / 析构 =================

SnippetRepository::SnippetRepository(QObject* parent)
    : QObject(parent)
{
    LOG_DEBUG("SnippetRepository 已创建");
}

SnippetRepository::~SnippetRepository()
{
    LOG_DEBUG("SnippetRepository 已销毁");
}

// ================= 静态方法 =================

QString SnippetRepository::defaultSnippetsResourcePath()
{
    return DEFAULT_SNIPPETS_RESOURCE;
}

QString SnippetRepository::projectSnippetsFileName()
{
    return PROJECT_SNIPPETS_FILENAME;
}

// ================= 加载接口 =================

bool SnippetRepository::loadSnippets(const QString& projectPath)
{
    clear();
    
    // 1. 加载默认 snippets（必须成功）
    QList<FunctionSnippet> defaultSnippets;
    QString errorMessage;
    
    if (!loadSnippetsFromFile(DEFAULT_SNIPPETS_RESOURCE, defaultSnippets, errorMessage)) {
        LOG_ERROR(QString("加载默认 Snippets 失败: %1").arg(errorMessage));
        emit loadError(QString("加载默认 Snippets 失败: %1").arg(errorMessage));
        return false;
    }
    
    LOG_DEBUG(QString("已加载 %1 个默认 Snippets").arg(defaultSnippets.size()));
    
    // 2. 尝试加载工程级 snippets（可选）
    QList<FunctionSnippet> projectSnippets;
    bool hasProjectSnippets = false;
    
    if (!projectPath.isEmpty()) {
        QString projectSnippetsPath = QDir(projectPath).filePath(PROJECT_SNIPPETS_FILENAME);
        QFileInfo fileInfo(projectSnippetsPath);
        
        if (fileInfo.exists() && fileInfo.isFile()) {
            QString projectError;
            if (loadSnippetsFromFile(projectSnippetsPath, projectSnippets, projectError)) {
                hasProjectSnippets = true;
                LOG_DEBUG(QString("已加载 %1 个工程级 Snippets: %2")
                          .arg(projectSnippets.size())
                          .arg(projectSnippetsPath));
            } else {
                // 工程级 snippets 解析失败，仅记录日志，回退到默认 snippets
                LOG_WARN(QString("工程级 Snippets 解析失败，使用默认配置: %1").arg(projectError));
                emit loadError(QString("工程级 Snippets 解析失败: %1").arg(projectError));
                // 继续使用默认 snippets，不中断加载
            }
        }
    }
    
    // 3. 合并 snippets
    if (hasProjectSnippets) {
        m_snippets = mergeSnippets(defaultSnippets, projectSnippets);
        LOG_DEBUG(QString("合并后共 %1 个 Snippets").arg(m_snippets.size()));
    } else {
        m_snippets = defaultSnippets;
    }
    
    // 4. 重建索引
    rebuildIndex();
    
    m_currentProjectPath = projectPath;
    m_loaded = true;
    
    emit snippetsReloaded();
    return true;
}

bool SnippetRepository::reloadSnippets(const QString& projectPath)
{
    return loadSnippets(projectPath);
}

// ================= 数据访问 =================

FunctionSnippet SnippetRepository::snippetById(const QString& id) const
{
    if (m_idToIndex.contains(id)) {
        int index = m_idToIndex.value(id);
        if (index >= 0 && index < m_snippets.size()) {
            return m_snippets.at(index);
        }
    }
    return FunctionSnippet();  // 返回无效 snippet
}

QList<FunctionSnippet> SnippetRepository::snippetsByCategory(const QString& category) const
{
    QList<FunctionSnippet> result;
    for (const auto& snippet : m_snippets) {
        if (snippet.category == category) {
            result.append(snippet);
        }
    }
    return result;
}

QStringList SnippetRepository::snippetNames() const
{
    QStringList names;
    names.reserve(m_snippets.size());
    for (const auto& snippet : m_snippets) {
        names.append(snippet.name);
    }
    return names;
}

// ================= 内部加载方法 =================

bool SnippetRepository::parseSnippetsFromJson(const QString& jsonText,
                                               QList<FunctionSnippet>& outSnippets,
                                               QString& errorMessage) const
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        errorMessage = QString("JSON 解析错误: %1 (位置 %2)")
                       .arg(parseError.errorString())
                       .arg(parseError.offset);
        return false;
    }
    
    QJsonArray array;
    if (doc.isArray()) {
        array = doc.array();
    } else if (doc.isObject()) {
        const QJsonObject root = doc.object();
        const QJsonValue snippetsValue = root.value("snippets");
        if (!snippetsValue.isArray()) {
            errorMessage = "JSON 根对象必须包含数组字段 'snippets'";
            return false;
        }
        array = snippetsValue.toArray();
    } else {
        errorMessage = "JSON 根元素必须是数组或对象";
        return false;
    }
    outSnippets.clear();
    outSnippets.reserve(array.size());
    
    for (int i = 0; i < array.size(); ++i) {
        if (!array[i].isObject()) {
            errorMessage = QString("第 %1 个元素不是对象").arg(i + 1);
            return false;
        }
        
        QJsonObject obj = array[i].toObject();
        FunctionSnippet snippet;
        
        // 必需字段
        if (!obj.contains("id") || !obj["id"].isString()) {
            errorMessage = QString("第 %1 个元素缺少有效的 'id' 字段").arg(i + 1);
            return false;
        }
        snippet.id = obj["id"].toString();
        
        if (!obj.contains("name") || !obj["name"].isString()) {
            errorMessage = QString("第 %1 个元素缺少有效的 'name' 字段").arg(i + 1);
            return false;
        }
        snippet.name = obj["name"].toString();
        
        // 可选字段（带默认值）
        snippet.category = obj.value("category").toString();
        snippet.description = obj.value("description").toString();
        snippet.templateCode = obj.value("templateCode").toString();
        snippet.unit = obj.value("unit").toString();
        snippet.defaultPeriodMs = obj.value("defaultPeriodMs").toInt(20);
        
        // metadata（可选）
        if (obj.contains("metadata") && obj["metadata"].isObject()) {
            snippet.metadata = obj["metadata"].toObject().toVariantMap();
        }
        
        // tags 存入 metadata（可选）
        if (obj.contains("tags") && obj["tags"].isArray()) {
            QStringList tags;
            QJsonArray tagsArray = obj["tags"].toArray();
            for (const auto& tag : tagsArray) {
                if (tag.isString()) {
                    tags.append(tag.toString());
                }
            }
            snippet.metadata["tags"] = tags;
        }
        
        outSnippets.append(snippet);
    }
    
    return true;
}

bool SnippetRepository::loadSnippetsFromFile(const QString& filePath,
                                              QList<FunctionSnippet>& outSnippets,
                                              QString& errorMessage) const
{
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMessage = QString("无法打开文件: %1 (%2)")
                       .arg(filePath)
                       .arg(file.errorString());
        return false;
    }
    
    QString jsonText = QString::fromUtf8(file.readAll());
    file.close();
    
    return parseSnippetsFromJson(jsonText, outSnippets, errorMessage);
}

QList<FunctionSnippet> SnippetRepository::mergeSnippets(
    const QList<FunctionSnippet>& base,
    const QList<FunctionSnippet>& overlay) const
{
    // 构建 overlay 的 id -> snippet 映射
    QHash<QString, FunctionSnippet> overlayMap;
    for (const auto& snippet : overlay) {
        overlayMap.insert(snippet.id, snippet);
    }
    
    // 记录已处理的 overlay id
    QSet<QString> processedIds;
    
    // 结果列表
    QList<FunctionSnippet> result;
    result.reserve(base.size() + overlay.size());
    
    // 遍历 base，保持顺序
    for (const auto& baseSnippet : base) {
        if (overlayMap.contains(baseSnippet.id)) {
            // 同 id：使用 overlay 的定义
            result.append(overlayMap.value(baseSnippet.id));
            processedIds.insert(baseSnippet.id);
        } else {
            // 仅 base 有：保留
            result.append(baseSnippet);
        }
    }
    
    // 追加 overlay 中新增的（不在 base 中的）
    for (const auto& overlaySnippet : overlay) {
        if (!processedIds.contains(overlaySnippet.id)) {
            result.append(overlaySnippet);
        }
    }
    
    return result;
}

void SnippetRepository::rebuildIndex()
{
    m_idToIndex.clear();
    for (int i = 0; i < m_snippets.size(); ++i) {
        m_idToIndex.insert(m_snippets[i].id, i);
    }
}

void SnippetRepository::clear()
{
    m_snippets.clear();
    m_idToIndex.clear();
    m_currentProjectPath.clear();
    m_loaded = false;
}
