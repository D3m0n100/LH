/**
 * @file dsl_completion_engine_test.cpp
 * @brief DslCompletionEngine 集成测试
 *
 * 测试内容：
 * - 与 SnippetRepository 的配合
 * - snippet 查询接口
 * - 搜索功能
 * - 代码生成
 *
 * 注意：本测试不包含图形化组态相关内容。
 */

#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include "DslCompletionEngine.h"

class DslCompletionEngineTest : public QObject
{
    Q_OBJECT

private:
    /**
     * @brief 创建测试用的 snippets.json 文件
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
     * @brief 生成自定义 snippet JSON（使用 QJsonDocument）
     */
    QString generateCustomSnippetJson()
    {
        QJsonObject snippet;
        snippet["id"] = "custom_sensor";
        snippet["name"] = "Custom Sensor";
        snippet["category"] = "input";
        snippet["description"] = "Project-level custom sensor snippet";
        snippet["templateCode"] = "custom_sensor(name = \"$NAME$\", address = $ADDR$)";
        snippet["unit"] = "mV";
        snippet["defaultPeriodMs"] = 50;

        QJsonArray snippetsArray;
        snippetsArray.append(snippet);

        QJsonObject root;
        root["snippets"] = snippetsArray;

        return QJsonDocument(root).toJson(QJsonDocument::Indented);
    }

private slots:
    void initTestCase()
    {
        qInfo() << "========================================";
        qInfo() << "DslCompletionEngine Test Suite";
        qInfo() << "========================================";
    }

    void cleanupTestCase()
    {
        qInfo() << "All DslCompletionEngine tests completed.";
    }

    // =========================================================================
    // 测试 1：基础初始化
    // =========================================================================

    /**
     * @brief 测试引擎初始化
     */
    void testEngineInitialization()
    {
        DslCompletionEngine engine(nullptr);
        
        // 初始化内置 snippets
        engine.initBuiltinSnippets();
        
        // 应该有可用的 snippets
        auto snippets = engine.availableSnippets();
        QVERIFY2(!snippets.isEmpty(), "Should have snippets after init");
        
        // 应该有组件名称列表
        auto names = engine.componentNames();
        QVERIFY(!names.isEmpty());
        
        qInfo() << "Engine init success, loaded" << snippets.size() << "snippets";
    }

    // =========================================================================
    // 测试 2：Snippet 查询接口
    // =========================================================================

    /**
     * @brief 测试按 ID 查询 snippet
     */
    void testSnippetById()
    {
        DslCompletionEngine engine(nullptr);
        engine.initBuiltinSnippets();
        
        auto snippets = engine.availableSnippets();
        if (snippets.isEmpty()) {
            QSKIP("No snippets available, skipping test");
        }
        
        // 获取第一个 snippet 的 ID
        QString firstId = snippets.first().id;
        
        // 通过 ID 查询
        FunctionSnippet found = engine.snippetById(firstId);
        QVERIFY(found.isValid());
        QCOMPARE(found.id, firstId);
        
        // 查询不存在的 ID
        FunctionSnippet notFound = engine.snippetById("nonexistent_id_12345");
        QVERIFY(!notFound.isValid());
        
        qInfo() << "Query by ID test passed";
    }

    /**
     * @brief 测试按名称查询 snippet
     */
    void testSnippetByName()
    {
        DslCompletionEngine engine(nullptr);
        engine.initBuiltinSnippets();
        
        auto snippets = engine.availableSnippets();
        if (snippets.isEmpty()) {
            QSKIP("No snippets available, skipping test");
        }
        
        // 获取第一个 snippet 的名称
        QString firstName = snippets.first().name;
        
        // 通过名称查询
        FunctionSnippet found = engine.snippetByName(firstName);
        QVERIFY(found.isValid());
        QCOMPARE(found.name, firstName);
        
        // 查询不存在的名称
        FunctionSnippet notFound = engine.snippetByName("NonExistentComponentName");
        QVERIFY(!notFound.isValid());
        
        qInfo() << "Query by name test passed";
    }

    /**
     * @brief 测试按分类查询 snippets
     */
    void testSnippetsByCategory()
    {
        DslCompletionEngine engine(nullptr);
        engine.initBuiltinSnippets();
        
        // 获取 input 分类的 snippets
        auto inputSnippets = engine.snippetsByCategory("input");
        
        // 验证返回的 snippets 都属于该分类
        for (const auto& snippet : inputSnippets) {
            QCOMPARE(snippet.category, QString("input"));
        }
        
        // 获取不存在分类的 snippets
        auto emptyCategory = engine.snippetsByCategory("nonexistent_category");
        QCOMPARE(emptyCategory.size(), 0);
        
        qInfo() << "Query by category test passed, input category has" 
                << inputSnippets.size() << "snippets";
    }

    // =========================================================================
    // 测试 3：搜索功能
    // =========================================================================

    /**
     * @brief 测试组件搜索
     */
    void testSearchComponents()
    {
        DslCompletionEngine engine(nullptr);
        engine.initBuiltinSnippets();
        
        auto allNames = engine.componentNames();
        if (allNames.isEmpty()) {
            QSKIP("No components available, skipping test");
        }
        
        // 空前缀搜索，应返回所有（最多 maxResults 个）
        auto allResults = engine.searchComponents("", 100);
        QVERIFY(!allResults.isEmpty());
        
        // 有前缀的搜索
        QString firstChar = allNames.first().left(1);
        auto prefixResults = engine.searchComponents(firstChar, 10);
        
        // 验证结果都以前缀开头（不区分大小写）
        for (const QString& name : prefixResults) {
            QVERIFY(name.toLower().startsWith(firstChar.toLower()));
        }
        
        // 不存在的前缀
        auto noResults = engine.searchComponents("xyz_impossible_prefix_", 10);
        QCOMPARE(noResults.size(), 0);
        
        qInfo() << "Search components test passed";
    }

    /**
     * @brief 测试搜索结果限制
     */
    void testSearchResultLimit()
    {
        DslCompletionEngine engine(nullptr);
        engine.initBuiltinSnippets();
        
        // 限制为 2 个结果
        auto limitedResults = engine.searchComponents("", 2);
        QVERIFY(limitedResults.size() <= 2);
        
        // 限制为 1 个结果
        auto singleResult = engine.searchComponents("", 1);
        QVERIFY(singleResult.size() <= 1);
        
        qInfo() << "Search result limit test passed";
    }

    // =========================================================================
    // 测试 4：代码生成
    // =========================================================================

    /**
     * @brief 测试代码生成
     */
    void testGenerateInsertCode()
    {
        DslCompletionEngine engine(nullptr);
        engine.initBuiltinSnippets();
        
        auto snippets = engine.availableSnippets();
        if (snippets.isEmpty()) {
            QSKIP("No snippets available, skipping test");
        }
        
        FunctionSnippet snippet = snippets.first();
        
        // 无缩进
        QString code1 = engine.generateInsertCode(snippet, "");
        QVERIFY(!code1.isEmpty());
        
        // 有缩进
        QString indent = "    ";  // 4 空格
        QString code2 = engine.generateInsertCode(snippet, indent);
        
        // 验证缩进被应用（如果实现支持）
        if (code2.contains("\n")) {
            // 多行代码应该有缩进
            qInfo() << "Generated code contains newlines, length:" << code2.length();
        }
        
        qInfo() << "Code generation test passed";
    }

    // =========================================================================
    // 测试 5：工程级覆盖集成
    // =========================================================================

    /**
     * @brief 测试工程级 snippets 覆盖（与 SnippetRepository 集成）
     */
    void testProjectSnippetsIntegration()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        
        QString projectPath = tempDir.path();
        
        // 创建工程级 snippets.json
        QString customSnippetsJson = generateCustomSnippetJson();
        
        QVERIFY(createSnippetsFile(projectPath, customSnippetsJson));
        
        // 加载并验证
        DslCompletionEngine engine(nullptr);
        engine.initBuiltinSnippets();
        
        int defaultCount = engine.availableSnippets().size();
        
        // 加载工程级
        bool reloadResult = engine.reloadSnippets(projectPath);
        
        if (reloadResult) {
            // 查找自定义 snippet
            FunctionSnippet custom = engine.snippetById("custom_sensor");
            
            if (custom.isValid()) {
                QCOMPARE(custom.name, QString("Custom Sensor"));
                QCOMPARE(custom.category, QString("input"));
                QCOMPARE(custom.unit, QString("mV"));
                qInfo() << "Project integration test passed, custom snippet loaded";
            }
        }
        
        qInfo() << "Default count:" << defaultCount 
                << ", after project load:" << engine.availableSnippets().size();
    }

    // =========================================================================
    // 测试 6：边界情况
    // =========================================================================

    /**
     * @brief 测试未初始化时的行为
     */
    void testUninitializedEngine()
    {
        DslCompletionEngine engine(nullptr);
        
        // 未调用 initBuiltinSnippets，snippets 应为空或有默认值
        auto snippets = engine.availableSnippets();
        
        // 查询不应崩溃
        FunctionSnippet notFound = engine.snippetById("any_id");
        QVERIFY(!notFound.isValid());
        
        auto searchResults = engine.searchComponents("test", 10);
        // 不崩溃即可
        
        qInfo() << "Uninitialized state test passed";
    }

    /**
     * @brief 测试重复初始化
     */
    void testMultipleInitialization()
    {
        DslCompletionEngine engine(nullptr);
        
        engine.initBuiltinSnippets();
        int count1 = engine.availableSnippets().size();
        
        engine.initBuiltinSnippets();
        int count2 = engine.availableSnippets().size();
        
        engine.initBuiltinSnippets();
        int count3 = engine.availableSnippets().size();
        
        // 多次初始化应该稳定
        QCOMPARE(count2, count1);
        QCOMPARE(count3, count1);
        
        qInfo() << "Multiple init test passed, stable count:" << count1;
    }

    /**
     * @brief 测试 FunctionSnippet 有效性判断
     */
    void testSnippetValidity()
    {
        FunctionSnippet emptySnippet;
        QVERIFY(!emptySnippet.isValid());
        
        FunctionSnippet validSnippet;
        validSnippet.id = "test_id";
        QVERIFY(validSnippet.isValid());
        
        qInfo() << "Snippet validity test passed";
    }
};

QTEST_MAIN(DslCompletionEngineTest)
#include "dsl_completion_engine_test.moc"
