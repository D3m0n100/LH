/**
 * @file DslCompletionEngine.h
 * @brief DSL 代码补全引擎
 *
 * ============== 职责说明 ==============
 *
 * DslCompletionEngine 负责管理 DSL 函数/组件的补全功能，包括：
 * - 管理函数片段（snippets）列表
 * - 提供搜索/匹配接口
 * - 管理 QCompleter 的数据模型
 * - 支持从配置文件加载 Snippets（配置化重构）
 *
 * ============== 重构说明（配置化改造）==============
 *
 * 本次重构将原硬编码的 Snippets 改造为配置化加载：
 * - 默认 Snippets 从 Qt 资源 :/snippets/default_snippets.json 加载
 * - 支持工程级 snippets.json 覆盖
 * - 通过 SnippetRepository 统一管理加载逻辑
 * - 对外接口和 UI 行为保持完全不变
 *
 * ============== 与 DslScriptEditor 的关系 ==============
 *
 * - DslCompletionEngine 由 DslScriptEditor 创建并管理
 * - 为 FunctionListWidget 提供数据源
 * - 为自动补全提供候选列表
 * - 不直接持有 UI 控件，通过接口与 DslScriptEditor 交互
 */

#ifndef DSLCOMPLETIONENGINE_H
#define DSLCOMPLETIONENGINE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QList>
#include <QCompleter>
#include <QVariantMap>
#include <QDateTime>

// 前向声明
class SnippetRepository;

/**
 * @brief FunctionSnippet
 * 函数/组件片段描述结构体
 */
struct FunctionSnippet
{
    QString id;             ///< 组件唯一标识
    QString name;           ///< 显示名称
    QString category;       ///< 分类（如 input, output, control）
    QString description;    ///< 描述说明
    QString templateCode;   ///< DSL 模板代码
    QString unit;           ///< 默认单位
    int defaultPeriodMs = 20; ///< 默认采样周期（毫秒）
    QVariantMap metadata;   ///< 额外元数据
    
    bool isValid() const { return !id.isEmpty(); }
};

/**
 * @brief DslInsertRecord
 * 记录一次拖拽插入的信息，用于追踪组态结果
 */
struct DslInsertRecord
{
    QString snippetId;      ///< 插入的组件ID
    QString snippetName;    ///< 组件名称
    QString mappingId;     ///< 映射ID（用于脚本内标记稳定追踪）
    int lineNumber = 0;     ///< 插入位置行号
    QString generatedCode;  ///< 实际生成的代码
    QDateTime insertTime;   ///< 插入时间
};

/**
 * @brief DSL 代码补全引擎
 *
 * 管理函数片段列表和自动补全功能。
 */
class DslCompletionEngine : public QObject
{
    Q_OBJECT

public:
    explicit DslCompletionEngine(QObject* parent = nullptr);
    ~DslCompletionEngine() override;

    // ===== Snippet 管理 =====
    
    /// 获取所有可用的函数片段
    QList<FunctionSnippet> availableSnippets() const { return m_snippets; }
    
    /// 根据 ID 获取函数片段
    FunctionSnippet snippetById(const QString& id) const;
    
    /// 根据名称获取函数片段
    FunctionSnippet snippetByName(const QString& name) const;
    
    /// 获取所有组件名称列表
    QStringList componentNames() const { return m_componentNames; }
    
    /// 获取指定分类的组件列表
    QList<FunctionSnippet> snippetsByCategory(const QString& category) const;
    
    /// 搜索匹配的组件（用于补全）
    QStringList searchComponents(const QString& prefix, int maxResults = 10) const;

    // ===== 数据加载 =====
    
    /**
     * @brief 初始化内置函数片段
     * 
     * 从配置文件加载默认 Snippets。
     * 此方法保持与原实现相同的接口，内部改为配置化加载。
     */
    void initBuiltinSnippets();
    
    /**
     * @brief 重新加载 Snippets（配置化接口）
     * @param projectPath 工程路径（可为空）
     * @return 是否加载成功
     *
     * 配置化加载接口：
     * - 如果 projectPath 非空，尝试加载工程级 snippets.json
     * - 工程级配置可覆盖/新增默认 Snippets
     * - 加载失败时回退到默认 Snippets
     */
    bool reloadSnippets(const QString& projectPath = QString());
    
    /// 从 JSON 字符串加载组件列表（保留原接口）
    bool loadFromJson(const QString& jsonText);
    
    /// 从 JSON 文件加载组件列表（保留原接口）
    bool loadFromFile(const QString& filePath);
    
    /// 添加自定义函数片段
    void addSnippet(const FunctionSnippet& snippet);
    
    /// 清空所有片段
    void clearSnippets();

    // ===== 补全器 =====
    
    /// 获取 QCompleter（用于编辑器）
    QCompleter* completer() const { return m_completer; }
    
    /// 设置补全前缀
    void setCompletionPrefix(const QString& prefix);
    
    /// 获取当前补全前缀
    QString completionPrefix() const;

    // ===== 代码生成 =====
    
    /// 生成带缩进的插入代码
    QString generateInsertCode(const FunctionSnippet& snippet, 
                               const QString& indentation) const;
    
    /// 记录一次插入
    void recordInsertion(const DslInsertRecord& record);
    
    /// 获取所有插入记录
    QList<DslInsertRecord> insertRecords() const { return m_insertRecords; }
    
    /// 清空插入记录
    void clearInsertRecords() { m_insertRecords.clear(); }

signals:
    /// Snippets 列表已变化
    void snippetsChanged();

private:
    // ===== 内部方法 =====
    
    /// 更新补全模型
    void updateCompletionModel();
    
    /// 从 Repository 同步数据
    void syncFromRepository();

private:
    // ===== Snippet 仓库 =====
    SnippetRepository* m_repository = nullptr;
    
    // ===== Snippet 数据（本地缓存）=====
    QList<FunctionSnippet> m_snippets;
    QHash<QString, FunctionSnippet> m_snippetMap;
    QStringList m_componentNames;
    
    // ===== 补全器 =====
    QCompleter* m_completer = nullptr;
    
    // ===== 插入记录 =====
    QList<DslInsertRecord> m_insertRecords;
};

#endif // DSLCOMPLETIONENGINE_H
