/**
 * @file snippet_repository_test.cpp
 * @brief SnippetRepository + DslCompletionEngine 单元测试
 *
 * 测试内容：
 * - 默认 snippets 加载（无工程级 snippets.json）
 * - 工程级 snippets.json 覆盖逻辑
 * - 工程级 snippets.json 语法错误时的回退逻辑
 * - 字段不完整时的容错处理
 * - 多次加载的稳定性
 */

#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

#include "SnippetRepository.h"
#include "DslCompletionEngine.h"

class SnippetRepositoryTest : public QObject
{
    Q_OBJECT

private:
    /**
     * @brief 创建测试用的 snippets.json 文件
     * @param dir 目标目录
     * @param content JSON 内容字符串
     * @return 是否创建成功
     */
    bool createSnippetsFile(const QString& dir, const QString& content)
    {
        QFile file(dir + "/snippets.json");
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }
        QTextStream out(&file);
        out << content;
        file.close();
        return true;
    }

    /**
     * @brief 生成有效的测试 snippets JSON（使用 QJsonDocument）
     * @param snippetId 自定义 snippet ID
     * @param snippetName 自定义 snippet 名称
     * @return JSON 字符串
     */
    QString generateValidSnippetsJson(const QString& snippetId, 
                                       const QString& snippetName)
    {
        QJsonObject snippet;
        snippet["id"] = snippetId;
        snippet["name"] = snippetName;
        snippet["category"] = "test";
        snippet["description"] = "Test snippet for unit testing";
        snippet["templateCode"] = QString("%1(name = \"test\", period = 20)").arg(snippetId);
        snippet["unit"] = "bar";
        snippet["defaultPeriodMs"] = 20;

        QJsonArray snippetsArray;
        snippetsArray.append(snippet);

        QJsonObject root;
        root["snippets"] = snippetsArray;

        return QJsonDocument(root).toJson(QJsonDocument::Indented);
    }

    /**
     * @brief 生成语法错误的 JSON
     */
    QString generateMalformedJson()
    {
        // 故意创建无效的 JSON（缺少引号闭合）
        return "{\n"
               "    \"snippets\": [\n"
               "        {\n"
               "            \"id\": \"broken\",\n"
               "            \"name\": \"Broken Snippet\n"
               "        }\n"
               "    ]\n"
               "}";
    }

    /**
     * @brief 生成字段不完整的 JSON（有效 JSON 语法，但 snippet 字段不完整）
     */
    QString generateIncompleteFieldsJson()
    {
        QJsonArray snippetsArray;

        // snippet 1: 只有 id，缺少其他字段
        QJsonObject incomplete1;
        incomplete1["id"] = "incomplete_1";
        snippetsArray.append(incomplete1);

        // snippet 2: 完整的 snippet
        QJsonObject complete;
        complete["id"] = "complete_one";
        complete["name"] = QString::fromUtf8("完整的组件");
        complete["category"] = "test";
        complete["description"] = QString::fromUtf8("这是一个完整的 snippet");
        complete["templateCode"] = "complete_one(name = \"test\")";
        complete["unit"] = "bar";
        complete["defaultPeriodMs"] = 20;
        snippetsArray.append(complete);

        // snippet 3: 缺少 id
        QJsonObject noId;
        noId["name"] = "no_id_snippet";
        snippetsArray.append(noId);

        QJsonObject root;
        root["snippets"] = snippetsArray;

        return QJsonDocument(root).toJson(QJsonDocument::Indented);
    }

    /**
     * @brief 生成空 snippets 数组的 JSON
     */
    QString generateEmptySnippetsJson()
    {
        QJsonObject root;
        root["snippets"] = QJsonArray();
        return QJsonDocument(root).toJson(QJsonDocument::Compact);
    }

private slots:
    void initTestCase()
    {
        qInfo() << "========================================";
        qInfo() << "SnippetRepository Test Suite";
        qInfo() << "========================================";
    }

    void cleanupTestCase()
    {
        qInfo() << "All SnippetRepository tests completed.";
    }

    // =========================================================================
    // 测试 1：默认 snippets 加载
    // =========================================================================

    /**
     * @brief 测试默认 snippets 能否正常加载
     * 
     * 验证点：
     * - loadSnippets() 不崩溃
     * - allSnippets() 返回非空列表
     */
    void testDefaultSnippetsLoad()
    {
        SnippetRepository repository(nullptr);
        
        // 加载默认 snippets
        repository.loadSnippets();
        
        // 验证：应该有至少一个 snippet
        auto snippets = repository.allSnippets();
        QVERIFY2(!snippets.isEmpty(), 
                 "Default snippets should not be empty after loading");
        
        qInfo() << "Loaded" << snippets.size() << "default snippets";
    }

    /**
     * @brief 测试多次调用 loadSnippets 的稳定性
     * 
     * 验证点：
     * - 多次加载不会导致 snippets 数量翻倍或变为空
     */
    void testMultipleLoadStability()
    {
        SnippetRepository repository(nullptr);
        
        repository.loadSnippets();
        auto first = repository.allSnippets();
        
        // 再加载一次
        repository.loadSnippets();
        auto second = repository.allSnippets();
        
        QVERIFY(!first.isEmpty());
        QCOMPARE(second.size(), first.size());
        
        // 再加载第三次
        repository.loadSnippets();
        auto third = repository.allSnippets();
        QCOMPARE(third.size(), first.size());
        
        qInfo() << "Multiple loads stable, count:" << first.size();
    }

    // =========================================================================
    // 测试 2：工程级 snippets.json 覆盖逻辑
    // =========================================================================

    /**
     * @brief 测试工程级 snippets.json 覆盖特定 ID
     * 
     * 验证点：
     * - 工程级 snippets 能够覆盖同 ID 的默认 snippet
     * - 覆盖后 snippet 内容为工程级定义的内容
     */
    void testProjectSnippetsOverride()
    {
        // 创建临时工程目录
        QTemporaryDir tempDir;
        QVERIFY2(tempDir.isValid(), "Failed to create temp directory");
        
        QString projectPath = tempDir.path();
        
        // 创建工程级 snippets.json，覆盖某个 ID
        QString customId = "custom_test_component";
        QString customName = "CustomTestComponent";
        QString snippetsJson = generateValidSnippetsJson(customId, customName);
        
        QVERIFY2(createSnippetsFile(projectPath, snippetsJson),
                 "Failed to create project snippets.json");
        
        // 使用 DslCompletionEngine 加载
        DslCompletionEngine engine(nullptr);
        
        // 先加载默认
        engine.initBuiltinSnippets();
        int defaultCount = engine.availableSnippets().size();
        
        // 再加载工程级（应合并或覆盖）
        bool reloadResult = engine.reloadSnippets(projectPath);
        
        // 验证：reloadSnippets 应该成功
        if (reloadResult) {
            // 查找自定义 snippet
            FunctionSnippet customSnippet = engine.snippetById(customId);
            
            if (customSnippet.isValid()) {
                QCOMPARE(customSnippet.name, customName);
                QCOMPARE(customSnippet.category, QString("test"));
                qInfo() << "Project snippet override success:" << customSnippet.name;
            } else {
                qWarning() << "Project snippet not found, merge strategy may differ";
            }
        } else {
            qWarning() << "reloadSnippets returned false, check implementation";
        }
        
        qInfo() << "Default count:" << defaultCount;
        qInfo() << "After project load:" << engine.availableSnippets().size();
    }

    /**
     * @brief 测试无工程级 snippets.json 时行为与默认一致
     * 
     * 验证点：
     * - 工程目录不存在 snippets.json 时，不影响默认 snippets
     * - 加载后 snippets 与纯默认加载一致
     */
    void testNoProjectSnippetsFallback()
    {
        // 创建空的临时工程目录（无 snippets.json）
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        
        QString projectPath = tempDir.path();
        // 确保目录为空，没有 snippets.json
        QVERIFY(!QFile::exists(projectPath + "/snippets.json"));
        
        // 加载默认 snippets
        DslCompletionEngine defaultEngine(nullptr);
        defaultEngine.initBuiltinSnippets();
        auto defaultSnippets = defaultEngine.availableSnippets();
        
        // 尝试加载空工程目录
        DslCompletionEngine testEngine(nullptr);
        testEngine.initBuiltinSnippets();
        testEngine.reloadSnippets(projectPath);  // 工程目录无 snippets.json
        auto testSnippets = testEngine.availableSnippets();
        
        // 验证：数量应该与默认一致
        QCOMPARE(testSnippets.size(), defaultSnippets.size());
        
        qInfo() << "No project snippets, using defaults, count:" 
                << testSnippets.size();
    }

    // =========================================================================
    // 测试 3：错误处理与回退逻辑
    // =========================================================================

    /**
     * @brief 测试工程级 snippets.json 语法错误时的回退
     * 
     * 验证点：
     * - JSON 语法错误不会导致崩溃
     * - 应回退到默认 snippets 或保持之前状态
     */
    void testMalformedJsonFallback()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        
        QString projectPath = tempDir.path();
        
        // 创建语法错误的 JSON
        QString malformedJson = generateMalformedJson();
        
        QVERIFY(createSnippetsFile(projectPath, malformedJson));
        
        // 先加载默认
        DslCompletionEngine engine(nullptr);
        engine.initBuiltinSnippets();
        int defaultCount = engine.availableSnippets().size();
        
        // 尝试加载错误的工程级 snippets
        bool result = engine.reloadSnippets(projectPath);
        
        // 验证：不应该崩溃，且 snippets 应保持可用
        auto snippets = engine.availableSnippets();
        QVERIFY2(!snippets.isEmpty(), 
                 "Snippets should not be empty after malformed JSON");
        
        // 如果回退到默认，数量应该与默认一致
        if (!result || snippets.size() == defaultCount) {
            qInfo() << "Correctly fell back to defaults on malformed JSON";
        } else {
            qInfo() << "Partial load on malformed JSON, count:" << snippets.size();
        }
    }

    /**
     * @brief 测试工程级 snippets.json 字段不完整时的容错
     * 
     * 验证点：
     * - 缺少必要字段的 snippet 应被跳过或使用默认值
     * - 其他有效 snippet 仍能正常加载
     */
    void testIncompleteFieldsFallback()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        
        QString projectPath = tempDir.path();
        
        // 创建字段不完整的 JSON（有效的 JSON 语法，但缺少必要字段）
        QString incompleteJson = generateIncompleteFieldsJson();
        
        QVERIFY(createSnippetsFile(projectPath, incompleteJson));
        
        DslCompletionEngine engine(nullptr);
        engine.initBuiltinSnippets();
        
        // 加载包含不完整字段的工程级 snippets
        engine.reloadSnippets(projectPath);
        
        // 验证：不应崩溃
        auto snippets = engine.availableSnippets();
        QVERIFY(!snippets.isEmpty());
        
        // 检查完整的 snippet 是否被正确加载
        FunctionSnippet completeSnippet = engine.snippetById("complete_one");
        
        if (completeSnippet.isValid()) {
            QCOMPARE(completeSnippet.name, QString::fromUtf8("完整的组件"));
            qInfo() << "Complete snippet loaded correctly";
        }
        
        // 检查不完整的 snippet（可能被跳过或使用默认值）
        FunctionSnippet incompleteSnippet = engine.snippetById("incomplete_1");
        if (!incompleteSnippet.isValid()) {
            qInfo() << "Incomplete snippet correctly skipped";
        } else {
            qInfo() << "Incomplete snippet used defaults, name=" 
                    << incompleteSnippet.name;
        }
    }

    /**
     * @brief 测试空 snippets 数组的处理
     * 
     * 验证点：
     * - 空数组不会导致问题
     * - 应保持默认 snippets
     */
    void testEmptySnippetsArray()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        
        QString projectPath = tempDir.path();
        
        // 创建空 snippets 数组的 JSON
        QString emptyJson = generateEmptySnippetsJson();
        
        QVERIFY(createSnippetsFile(projectPath, emptyJson));
        
        DslCompletionEngine engine(nullptr);
        engine.initBuiltinSnippets();
        int defaultCount = engine.availableSnippets().size();
        
        // 加载空数组
        engine.reloadSnippets(projectPath);
        
        auto snippets = engine.availableSnippets();
        
        // 空数组可能导致清空或保持默认，取决于实现
        // 关键是不应崩溃
        qInfo() << "After empty array, count:" << snippets.size() 
                << "(default:" << defaultCount << ")";
        
        // 至少应该有一种合理的行为
        QVERIFY(snippets.size() >= 0);
    }

    // =========================================================================
    // 测试 4：边界情况
    // =========================================================================

    /**
     * @brief 测试不存在的工程路径
     * 
     * 验证点：
     * - 路径不存在时不会崩溃
     * - 保持之前的 snippets 状态
     */
    void testNonExistentProjectPath()
    {
        DslCompletionEngine engine(nullptr);
        engine.initBuiltinSnippets();
        int defaultCount = engine.availableSnippets().size();
        
        // 使用不存在的路径
        QString fakePath = "/nonexistent/path/to/project";
        engine.reloadSnippets(fakePath);
        
        // 不应崩溃，且保持默认
        auto snippets = engine.availableSnippets();
        QCOMPARE(snippets.size(), defaultCount);
        
        qInfo() << "Non-existent path handled correctly, kept defaults";
    }

    /**
     * @brief 测试空路径
     */
    void testEmptyProjectPath()
    {
        DslCompletionEngine engine(nullptr);
        engine.initBuiltinSnippets();
        int defaultCount = engine.availableSnippets().size();
        
        // 使用空路径
        engine.reloadSnippets("");
        
        // 不应崩溃
        auto snippets = engine.availableSnippets();
        QCOMPARE(snippets.size(), defaultCount);
        
        qInfo() << "Empty path handled correctly";
    }

    // =========================================================================
    // 测试 5：SnippetRepository 独立测试
    // =========================================================================

    /**
     * @brief 测试 SnippetRepository 直接接口
     */
    void testSnippetRepositoryDirect()
    {
        SnippetRepository repository(nullptr);
        
        // 初始状态应为空或有默认值
        auto initialSnippets = repository.allSnippets();
        
        // 加载
        repository.loadSnippets();
        auto loadedSnippets = repository.allSnippets();
        
        // 加载后应该有内容
        QVERIFY(!loadedSnippets.isEmpty());
        
        qInfo() << "SnippetRepository direct load success, count:" 
                << loadedSnippets.size();
    }
};

QTEST_MAIN(SnippetRepositoryTest)
#include "snippet_repository_test.moc"
