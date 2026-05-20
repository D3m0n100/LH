/**
 * @file DslCompletionEngine.cpp
 * @brief DSL 代码补全引擎实现
 *
 * ============== 重构说明 ==============
 *
 * 本次重构将原硬编码的 Snippets 改造为配置化加载：
 * - 删除了 createInputSnippets / createOutputSnippets / createControlSnippets 硬编码
 * - 改为通过 SnippetRepository 从配置文件加载
 * - 保持对外接口和 UI 行为完全不变
 */

#include "DslCompletionEngine.h"
#include "SnippetRepository.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QStringListModel>
#include <QDebug>

// ================= 构造 / 析构 =================

DslCompletionEngine::DslCompletionEngine(QObject* parent)
    : QObject(parent)
{
    m_completer = new QCompleter(this);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    
    // 创建 Snippet 仓库
    m_repository = new SnippetRepository(this);
    
    // 连接仓库信号
    connect(m_repository, &SnippetRepository::snippetsReloaded,
            this, &DslCompletionEngine::syncFromRepository);
}

DslCompletionEngine::~DslCompletionEngine()
{
    // QCompleter 和 SnippetRepository 由 QObject 父子关系自动管理
}

// ================= Snippet 查询 =================

FunctionSnippet DslCompletionEngine::snippetById(const QString& id) const
{
    return m_snippetMap.value(id);
}

FunctionSnippet DslCompletionEngine::snippetByName(const QString& name) const
{
    for (const auto& snippet : m_snippets) {
        if (snippet.name == name) {
            return snippet;
        }
    }
    return FunctionSnippet();
}

QList<FunctionSnippet> DslCompletionEngine::snippetsByCategory(const QString& category) const
{
    QList<FunctionSnippet> result;
    for (const auto& snippet : m_snippets) {
        if (snippet.category == category) {
            result.append(snippet);
        }
    }
    return result;
}

QStringList DslCompletionEngine::searchComponents(const QString& prefix, int maxResults) const
{
    QStringList results;
    
    if (prefix.isEmpty()) {
        // 返回所有组件名（最多 maxResults 个）
        for (int i = 0; i < qMin(m_componentNames.size(), maxResults); ++i) {
            results.append(m_componentNames[i]);
        }
    } else {
        // 搜索匹配前缀的组件
        QString lowerPrefix = prefix.toLower();
        for (const QString& name : m_componentNames) {
            if (name.toLower().startsWith(lowerPrefix)) {
                results.append(name);
                if (results.size() >= maxResults) {
                    break;
                }
            }
        }
    }
    
    return results;
}

// ================= 数据加载 =================

void DslCompletionEngine::initBuiltinSnippets()
{
    // 配置化改造：从仓库加载默认 Snippets
    // 此方法保持与原实现相同的接口，内部改为配置化加载
    clearSnippets();
    
    if (m_repository->loadSnippets()) {
        syncFromRepository();
    } else {
        qWarning() << "DslCompletionEngine: 加载默认 Snippets 失败";
    }
}

bool DslCompletionEngine::reloadSnippets(const QString& projectPath)
{
    // 配置化加载接口：支持工程级覆盖
    if (m_repository->reloadSnippets(projectPath)) {
        // syncFromRepository 会在 snippetsReloaded 信号中被调用
        return true;
    }
    return false;
}

void DslCompletionEngine::syncFromRepository()
{
    // 从仓库同步数据到本地缓存
    clearSnippets();
    
    m_snippets = m_repository->allSnippets();
    
    // 重建索引
    for (const auto& snippet : m_snippets) {
        m_snippetMap.insert(snippet.id, snippet);
        m_componentNames.append(snippet.name);
    }
    
    updateCompletionModel();
    emit snippetsChanged();
}

bool DslCompletionEngine::loadFromJson(const QString& jsonText)
{
    // 保留原接口：直接解析 JSON 并添加 Snippets
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "DslCompletionEngine: JSON 解析错误:" << parseError.errorString();
        return false;
    }
    
    if (!doc.isArray()) {
        qWarning() << "DslCompletionEngine: JSON 根元素必须是数组";
        return false;
    }
    
    QJsonArray array = doc.array();
    
    for (int i = 0; i < array.size(); ++i) {
        if (!array[i].isObject()) {
            continue;
        }
        
        QJsonObject obj = array[i].toObject();
        FunctionSnippet snippet;
        
        snippet.id = obj.value("id").toString();
        snippet.name = obj.value("name").toString();
        snippet.category = obj.value("category").toString();
        snippet.description = obj.value("description").toString();
        snippet.templateCode = obj.value("templateCode").toString();
        snippet.unit = obj.value("unit").toString();
        snippet.defaultPeriodMs = obj.value("defaultPeriodMs").toInt(20);
        
        if (snippet.isValid()) {
            addSnippet(snippet);
        }
    }
    
    updateCompletionModel();
    emit snippetsChanged();
    return true;
}

bool DslCompletionEngine::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "DslCompletionEngine: 无法打开文件:" << filePath;
        return false;
    }
    
    QString jsonText = QString::fromUtf8(file.readAll());
    file.close();
    
    return loadFromJson(jsonText);
}

void DslCompletionEngine::addSnippet(const FunctionSnippet& snippet)
{
    if (!snippet.isValid()) {
        return;
    }
    
    // 检查是否已存在
    if (m_snippetMap.contains(snippet.id)) {
        // 更新现有 Snippet
        for (int i = 0; i < m_snippets.size(); ++i) {
            if (m_snippets[i].id == snippet.id) {
                m_snippets[i] = snippet;
                break;
            }
        }
    } else {
        // 添加新 Snippet
        m_snippets.append(snippet);
        m_componentNames.append(snippet.name);
    }
    
    m_snippetMap.insert(snippet.id, snippet);
}

void DslCompletionEngine::clearSnippets()
{
    m_snippets.clear();
    m_snippetMap.clear();
    m_componentNames.clear();
}

// ================= 补全器 =================

void DslCompletionEngine::setCompletionPrefix(const QString& prefix)
{
    if (m_completer) {
        m_completer->setCompletionPrefix(prefix);
    }
}

QString DslCompletionEngine::completionPrefix() const
{
    return m_completer ? m_completer->completionPrefix() : QString();
}

void DslCompletionEngine::updateCompletionModel()
{
    if (m_completer) {
        QStringListModel* model = new QStringListModel(m_componentNames, m_completer);
        m_completer->setModel(model);
    }
}

// ================= 代码生成 =================

QString DslCompletionEngine::generateInsertCode(const FunctionSnippet& snippet,
                                                 const QString& indentation) const
{
    if (!snippet.isValid()) {
        return QString();
    }
    
    QString code = snippet.templateCode;
    
    // 如果模板代码为空，返回空字符串
    if (code.isEmpty()) {
        return QString();
    }
    
    // 处理缩进：在每行开头添加缩进（除了第一行）
    if (!indentation.isEmpty()) {
        code.replace("\n", "\n" + indentation);
    }
    
    return code;
}

void DslCompletionEngine::recordInsertion(const DslInsertRecord& record)
{
    m_insertRecords.append(record);
}
