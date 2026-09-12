/**
 * @file SnippetRepository.h
 * @brief Snippet 仓库 - 负责从配置文件加载和管理 DSL Snippets
 *
 * ============== 职责说明 ==============
 *
 * SnippetRepository 封装了 Snippet 配置化加载的所有逻辑，包括：
 * - 从 Qt 资源中加载默认 snippets（随软件发布）
 * - 从工程目录加载工程级 snippets.json（可选覆盖）
 * - 合并默认与工程级定义
 * - 保持加载顺序（与原硬编码顺序一致）
 *
 * ============== 两级配置 ==============
 *
 * 1. 全局默认：:/snippets/default_snippets.json（Qt 资源文件）
 * 2. 工程级别：<projectPath>/snippets.json（可选）
 *
 * 合并策略：
 * - 同 id：工程级覆盖默认定义
 * - 不同 id：视为新增，追加到列表末尾
 *
 * ============== UI 零侵入 ==============
 *
 * - 当没有工程级 snippets.json 时，返回的列表与原硬编码完全一致
 * - 不新增任何 UI 控件、菜单、按钮
 * - 仅在内部生命周期中被调用
 */

#ifndef SNIPPETREPOSITORY_H
#define SNIPPETREPOSITORY_H

#include <QObject>
#include <QString>
#include <QList>
#include <QHash>

// 前向声明
struct FunctionSnippet;

/**
 * @brief Snippet 仓库类
 *
 * 负责从配置文件加载 DSL Snippets，支持默认配置与工程级覆盖。
 */
class SnippetRepository : public QObject
{
    Q_OBJECT

public:
    explicit SnippetRepository(QObject* parent = nullptr);
    ~SnippetRepository() override;

    // ===== 加载接口 =====

    /**
     * @brief 加载 Snippets
     * @param projectPath 工程路径（可为空，表示仅加载默认 snippets）
     * @return 是否加载成功
     *
     * 加载逻辑：
     * 1. 首先加载 Qt 资源中的默认 snippets
     * 2. 如果 projectPath 非空且存在 snippets.json，加载并合并
     * 3. 合并策略：同 id 覆盖，不同 id 追加
     */
    bool loadSnippets(const QString& projectPath = QString());

    /**
     * @brief 重新加载 Snippets
     * @param projectPath 工程路径
     * @return 是否加载成功
     */
    bool reloadSnippets(const QString& projectPath = QString());

    // ===== 数据访问 =====

    /**
     * @brief 获取所有已加载的 Snippets
     * @return Snippet 列表（保持加载顺序）
     */
    QList<FunctionSnippet> allSnippets() const { return m_snippets; }

    /**
     * @brief 根据 ID 获取 Snippet
     * @param id Snippet ID
     * @return 对应的 Snippet（如果不存在则返回无效 Snippet）
     */
    FunctionSnippet snippetById(const QString& id) const;

    /**
     * @brief 获取指定分类的 Snippets
     * @param category 分类名称
     * @return 该分类下的所有 Snippets
     */
    QList<FunctionSnippet> snippetsByCategory(const QString& category) const;

    /**
     * @brief 获取所有 Snippet 名称列表
     * @return 名称列表（保持顺序）
     */
    QStringList snippetNames() const;

    /**
     * @brief 检查是否已加载
     * @return 是否已加载 Snippets
     */
    bool isLoaded() const { return m_loaded; }

    // ===== 静态工具方法 =====

    /**
     * @brief 获取默认 Snippets 资源路径
     * @return Qt 资源路径
     */
    static QString defaultSnippetsResourcePath();

    /**
     * @brief 获取工程级 Snippets 文件名
     * @return 文件名
     */
    static QString projectSnippetsFileName();

signals:
    /**
     * @brief Snippets 已重新加载
     */
    void snippetsReloaded();

    /**
     * @brief 加载出错（仅记录日志，不弹出 UI）
     * @param errorMessage 错误信息
     */
    void loadError(const QString& errorMessage);

private:
    // ===== 内部加载方法 =====

    /**
     * @brief 从 JSON 字符串解析 Snippets
     * @param jsonText JSON 字符串
     * @param outSnippets 输出的 Snippet 列表
     * @param errorMessage 错误信息（如果失败）
     * @return 是否解析成功
     */
    bool parseSnippetsFromJson(const QString& jsonText,
                                QList<FunctionSnippet>& outSnippets,
                                QString& errorMessage) const;

    /**
     * @brief 从文件加载 Snippets
     * @param filePath 文件路径（可以是资源路径如 :/snippets/...）
     * @param outSnippets 输出的 Snippet 列表
     * @param errorMessage 错误信息
     * @return 是否加载成功
     */
    bool loadSnippetsFromFile(const QString& filePath,
                               QList<FunctionSnippet>& outSnippets,
                               QString& errorMessage) const;

    /**
     * @brief 合并 Snippets 列表
     * @param base 基础列表（默认 snippets）
     * @param overlay 覆盖列表（工程级 snippets）
     * @return 合并后的列表
     *
     * 合并策略：
     * - 保持 base 的顺序
     * - 同 id：使用 overlay 中的定义替换
     * - overlay 中新增的 id：追加到末尾
     */
    QList<FunctionSnippet> mergeSnippets(const QList<FunctionSnippet>& base,
                                          const QList<FunctionSnippet>& overlay) const;

    /**
     * @brief 重建内部索引
     */
    void rebuildIndex();

    /**
     * @brief 清空已加载数据
     */
    void clear();

private:
    QList<FunctionSnippet> m_snippets;       ///< 已加载的 Snippet 列表
    QHash<QString, int> m_idToIndex;         ///< ID 到索引的映射
    QString m_currentProjectPath;            ///< 当前工程路径
    bool m_loaded = false;                   ///< 是否已加载
};

#endif // SNIPPETREPOSITORY_H
