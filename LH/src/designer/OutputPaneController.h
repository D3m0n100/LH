/**
 * @file OutputPaneController.h
 * @brief 输出窗口控制器
 *
 * 封装输出窗口（日志面板）的所有逻辑，包括：
 * - 日志追加和格式化
 * - 自动滚动控制
 * - 清空操作
 * - 右键菜单
 * - 保存到文件
 *
 * 此类从 MainWindow 中抽离，降低 MainWindow 的复杂度。
 */

#ifndef OUTPUTPANECONTROLLER_H
#define OUTPUTPANECONTROLLER_H

#include <QObject>
#include <QString>
#include <QTextEdit>
#include <QMenu>

class QDockWidget;
class QAction;

/**
 * @brief 输出窗口控制器
 *
 * 使用方式：
 * @code
 * QDockWidget* dock = new QDockWidget("输出", this);
 * m_outputController = new OutputPaneController(dock, this);
 * 
 * // 追加日志
 * m_outputController->appendMessage("Hello");
 * m_outputController->appendInfo("信息消息");
 * m_outputController->appendWarning("警告消息");
 * m_outputController->appendError("错误消息");
 * @endcode
 */
class OutputPaneController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param dockWidget 承载输出窗口的 Dock 控件
     * @param parent 父对象
     */
    explicit OutputPaneController(QDockWidget* dockWidget, QObject* parent = nullptr);
    ~OutputPaneController() override;

    /**
     * @brief 获取内部的 QTextEdit 控件
     * @return 文本编辑控件指针
     */
    QTextEdit* textEdit() const { return m_textEdit; }

    /**
     * @brief 获取 Dock 控件
     * @return Dock 控件指针
     */
    QDockWidget* dockWidget() const { return m_dockWidget; }

    // ===== 日志输出 =====

    /**
     * @brief 追加普通消息
     * @param message 消息内容
     */
    void appendMessage(const QString& message);

    /**
     * @brief 追加带时间戳的消息
     * @param message 消息内容
     */
    void appendTimestampedMessage(const QString& message);

    /**
     * @brief 追加信息级别消息（蓝色）
     * @param message 消息内容
     */
    void appendInfo(const QString& message);

    /**
     * @brief 追加警告级别消息（橙色）
     * @param message 消息内容
     */
    void appendWarning(const QString& message);

    /**
     * @brief 追加错误级别消息（红色）
     * @param message 消息内容
     */
    void appendError(const QString& message);

    /**
     * @brief 追加成功级别消息（绿色）
     * @param message 消息内容
     */
    void appendSuccess(const QString& message);

    // ===== 控制 =====

    /**
     * @brief 清空输出
     */
    void clear();

    /**
     * @brief 设置自动滚动
     * @param enabled 是否启用
     */
    void setAutoScroll(bool enabled);

    /**
     * @brief 获取自动滚动状态
     * @return 是否启用自动滚动
     */
    bool autoScroll() const { return m_autoScroll; }

    /**
     * @brief 设置字体大小
     * @param pointSize 字号
     */
    void setFontSize(int pointSize);

    /**
     * @brief 获取全部文本
     * @return 输出窗口的全部文本
     */
    QString allText() const;

    /**
     * @brief 获取选中文本
     * @return 选中的文本
     */
    QString selectedText() const;

public slots:
    /**
     * @brief 复制选中内容
     */
    void copySelected();

    /**
     * @brief 复制全部内容
     */
    void copyAll();

    /**
     * @brief 保存到文件
     */
    void saveToFile();

signals:
    /**
     * @brief 可见性变化信号
     * @param visible 是否可见
     */
    void visibilityChanged(bool visible);

private slots:
    /**
     * @brief 显示右键菜单
     * @param pos 位置
     */
    void showContextMenu(const QPoint& pos);

private:
    /**
     * @brief 初始化 UI
     */
    void setupUI();

    /**
     * @brief 滚动到底部
     */
    void scrollToBottom();

    /**
     * @brief 获取当前时间戳字符串
     * @return 格式化的时间戳
     */
    QString currentTimestamp() const;

private:
    QDockWidget* m_dockWidget;      ///< Dock 控件
    QTextEdit* m_textEdit;          ///< 文本编辑控件
    bool m_autoScroll;              ///< 自动滚动标志
};

#endif // OUTPUTPANECONTROLLER_H
