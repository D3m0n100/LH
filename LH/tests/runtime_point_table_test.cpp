/**
 * @file runtime_point_table_test.cpp
 * @brief RuntimePointTable 单元测试
 *
 * 测试内容：
 * - 定义注册与批量加载
 * - 按 id/name/kind 查询
 * - 值更新与批量更新
 * - 质量码和时间戳保留
 * - 快照一致性
 * - 清空与边界情况
 */

#include <QtTest/QtTest>
#include "common/RuntimePointTable.h"

class RuntimePointTableTest : public QObject
{
    Q_OBJECT

private:
    static RuntimePointDefinition makePoint(const QString& id, const QString& name,
                                            RuntimePointKind kind = RuntimePointKind::Variable)
    {
        RuntimePointDefinition def;
        def.id = id;
        def.name = name;
        def.kind = kind;
        def.dataType = QStringLiteral("REAL");
        def.access = RuntimePointAccess::ReadWrite;
        def.defaultValue = 0.0;
        return def;
    }

private slots:
    void init() {}
    void cleanup() {}

    // ── 注册与查询 ──────────────────────────────────────

    void registerSinglePoint()
    {
        RuntimePointTable table;
        auto def = makePoint("p1", "Pressure");
        table.registerPoint(def);

        QCOMPARE(table.count(), 1);
        QVERIFY(table.contains("p1"));

        auto result = table.definition("p1");
        QVERIFY(result.has_value());
        QCOMPARE(result->name, QStringLiteral("Pressure"));
        QCOMPARE(result->kind, RuntimePointKind::Variable);
    }

    void registerDuplicateOverwrites()
    {
        RuntimePointTable table;
        table.registerPoint(makePoint("p1", "Old"));
        table.registerPoint(makePoint("p1", "New"));

        QCOMPARE(table.count(), 1);
        QCOMPARE(table.definition("p1")->name, QStringLiteral("New"));
    }

    void reRegisterRebuildsIndexesAndResetsValue()
    {
        RuntimePointTable table;
        auto original = makePoint("p1", "Old", RuntimePointKind::Variable);
        original.defaultValue = 1.0;
        table.registerPoint(original);
        QVERIFY(table.updateValue("p1", 42.0, RuntimePointQuality::Good, QStringLiteral("test")));

        auto replacement = makePoint("p1", "New", RuntimePointKind::Parameter);
        replacement.defaultValue = 7.5;
        table.registerPoint(replacement);

        QVERIFY(!table.definitionByName("Old").has_value());
        QVERIFY(!table.idsByKind(RuntimePointKind::Variable).contains(QStringLiteral("p1")));
        QVERIFY(table.idsByKind(RuntimePointKind::Parameter).contains(QStringLiteral("p1")));
        const auto snapshot = table.snapshot("p1");
        QVERIFY(snapshot.has_value());
        QCOMPARE(snapshot->current.value.toDouble(), 7.5);
        QCOMPARE(snapshot->current.quality, RuntimePointQuality::Unknown);
        QCOMPARE(snapshot->current.origin, QStringLiteral("init"));
        QCOMPARE(snapshot->current.sequence, quint64(0));
        QVERIFY(!snapshot->current.timestamp.isValid());
    }

    void reRegisterDoesNotRemoveSameNameMappingForAnotherId()
    {
        RuntimePointTable table;
        table.registerPoint(makePoint("p1", "Shared"));
        table.registerPoint(makePoint("p2", "Shared"));

        table.registerPoint(makePoint("p1", "Renamed"));

        const auto shared = table.definitionByName("Shared");
        QVERIFY(shared.has_value());
        QCOMPARE(shared->id, QStringLiteral("p2"));
        const auto renamed = table.definitionByName("Renamed");
        QVERIFY(renamed.has_value());
        QCOMPARE(renamed->id, QStringLiteral("p1"));
    }

    void reRegisterRestoresNameMappingForRemainingDefinition()
    {
        RuntimePointTable table;
        table.registerPoint(makePoint("p1", "Shared"));
        table.registerPoint(makePoint("p2", "Shared"));

        table.registerPoint(makePoint("p2", "Renamed"));

        const auto shared = table.definitionByName("Shared");
        QVERIFY(shared.has_value());
        QCOMPARE(shared->id, QStringLiteral("p1"));
        const auto renamed = table.definitionByName("Renamed");
        QVERIFY(renamed.has_value());
        QCOMPARE(renamed->id, QStringLiteral("p2"));
    }

    void reRegisterKeepsNameMappingOwnedByAnotherId()
    {
        RuntimePointTable table;
        table.registerPoint(makePoint("p1", "Shared"));
        table.registerPoint(makePoint("p2", "Shared"));

        table.registerPoint(makePoint("p1", "Shared"));

        const auto shared = table.definitionByName("Shared");
        QVERIFY(shared.has_value());
        QCOMPARE(shared->id, QStringLiteral("p2"));
    }

    void loadDefinitionsBulk()
    {
        RuntimePointTable table;
        QList<RuntimePointDefinition> defs;
        defs << makePoint("p1", "A") << makePoint("p2", "B") << makePoint("p3", "C");
        table.loadDefinitions(defs);

        QCOMPARE(table.count(), 3);
        QCOMPARE(table.allIds().size(), 3);
    }

    void loadDefinitionsRemainsIncremental()
    {
        RuntimePointTable table;
        table.registerPoint(makePoint("existing", "Existing"));
        table.loadDefinitions({makePoint("loaded", "Loaded")});

        QCOMPARE(table.count(), 2);
        QVERIFY(table.contains("existing"));
        QVERIFY(table.contains("loaded"));
    }

    void loadFromProjectConfigReplacesPreviousPointSet()
    {
        RuntimePointTable table;
        ProjectRuntimeConfig first;
        VariableDefinition oldVariable;
        oldVariable.id = QStringLiteral("old-variable");
        oldVariable.name = QStringLiteral("Old");
        oldVariable.dataType = QStringLiteral("REAL");
        oldVariable.defaultValue = QStringLiteral("1.0");
        first.variables.append(oldVariable);
        table.loadFromProjectConfig(first);
        QVERIFY(table.contains(oldVariable.id));
        QVERIFY(table.updateValue(oldVariable.id, 12.0));

        ProjectRuntimeConfig second;
        VariableDefinition newVariable;
        newVariable.id = QStringLiteral("new-variable");
        newVariable.name = QStringLiteral("New");
        newVariable.dataType = QStringLiteral("REAL");
        newVariable.defaultValue = QStringLiteral("2.0");
        second.variables.append(newVariable);
        table.loadFromProjectConfig(second);

        QCOMPARE(table.count(), 1);
        QVERIFY(!table.contains(oldVariable.id));
        QVERIFY(!table.snapshot(oldVariable.id).has_value());
        QVERIFY(table.contains(newVariable.id));
        QCOMPARE(table.snapshot(newVariable.id)->current.quality, RuntimePointQuality::Unknown);
    }

    // ── 按 name 查询 ────────────────────────────────────

    void queryByName()
    {
        RuntimePointTable table;
        table.registerPoint(makePoint("p1", "Pressure"));
        table.registerPoint(makePoint("p2", "Temperature"));

        auto result = table.definitionByName("Temperature");
        QVERIFY(result.has_value());
        QCOMPARE(result->id, QStringLiteral("p2"));
    }

    void queryByNameNotFound()
    {
        RuntimePointTable table;
        QVERIFY(!table.definitionByName("NonExistent").has_value());
    }

    // ── 按 kind 查询 ────────────────────────────────────

    void queryByKind()
    {
        RuntimePointTable table;
        table.registerPoint(makePoint("p1", "A", RuntimePointKind::Variable));
        table.registerPoint(makePoint("p2", "B", RuntimePointKind::Parameter));
        table.registerPoint(makePoint("p3", "C", RuntimePointKind::Variable));

        auto vars = table.idsByKind(RuntimePointKind::Variable);
        QCOMPARE(vars.size(), 2);

        auto params = table.idsByKind(RuntimePointKind::Parameter);
        QCOMPARE(params.size(), 1);
        QCOMPARE(params.first(), QStringLiteral("p2"));
    }

    // ── 值更新 ──────────────────────────────────────────

    void updateSingleValue()
    {
        RuntimePointTable table;
        table.registerPoint(makePoint("p1", "Pressure"));

        QVERIFY(table.updateValue("p1", 42.5, RuntimePointQuality::Good, "test"));

        auto snap = table.snapshot("p1");
        QVERIFY(snap.has_value());
        QCOMPARE(snap->current.value.toDouble(), 42.5);
        QCOMPARE(snap->current.quality, RuntimePointQuality::Good);
        QCOMPARE(snap->current.origin, QStringLiteral("test"));
        QCOMPARE(snap->current.sequence, quint64(1));
    }

    void updateNonexistentReturnsFalse()
    {
        RuntimePointTable table;
        QVERIFY(!table.updateValue("no_such", 1.0));
    }

    void updateIncrementsSequence()
    {
        RuntimePointTable table;
        table.registerPoint(makePoint("p1", "A"));

        table.updateValue("p1", 1.0);
        table.updateValue("p1", 2.0);
        table.updateValue("p1", 3.0);

        QCOMPARE(table.snapshot("p1")->current.sequence, quint64(3));
    }

    void batchUpdate()
    {
        RuntimePointTable table;
        table.registerPoint(makePoint("p1", "A"));
        table.registerPoint(makePoint("p2", "B"));
        table.registerPoint(makePoint("p3", "C"));

        QHash<QString, QVariant> updates;
        updates["p1"] = 10.0;
        updates["p2"] = 20.0;
        updates["unknown"] = 99.0; // 应被跳过

        int ok = table.updateValues(updates, RuntimePointQuality::Good, "batch");
        QCOMPARE(ok, 2);
        QCOMPARE(table.snapshot("p1")->current.value.toDouble(), 10.0);
        QCOMPARE(table.snapshot("p2")->current.value.toDouble(), 20.0);
    }

    // ── 质量码 ──────────────────────────────────────────

    void qualityCodePreserved()
    {
        RuntimePointTable table;
        table.registerPoint(makePoint("p1", "A"));

        table.updateValue("p1", 1.0, RuntimePointQuality::Simulated);
        QCOMPARE(table.snapshot("p1")->current.quality, RuntimePointQuality::Simulated);

        table.setQuality("p1", RuntimePointQuality::Stale);
        QCOMPARE(table.snapshot("p1")->current.quality, RuntimePointQuality::Stale);
        // 值不变
        QCOMPARE(table.snapshot("p1")->current.value.toDouble(), 1.0);
    }

    void initialQualityIsUnknown()
    {
        RuntimePointTable table;
        table.registerPoint(makePoint("p1", "A"));
        QCOMPARE(table.snapshot("p1")->current.quality, RuntimePointQuality::Unknown);
    }

    // ── 时间戳 ──────────────────────────────────────────

    void timestampSetOnUpdate()
    {
        RuntimePointTable table;
        table.registerPoint(makePoint("p1", "A"));

        const QDateTime before = QDateTime::currentDateTime();
        table.updateValue("p1", 1.0);
        const QDateTime after = QDateTime::currentDateTime();

        const QDateTime ts = table.snapshot("p1")->current.timestamp;
        QVERIFY(ts.isValid());
        QVERIFY(ts >= before.addSecs(-1));
        QVERIFY(ts <= after.addSecs(1));
    }

    // ── 快照 ────────────────────────────────────────────

    void snapshotContainsDefinitionAndValue()
    {
        RuntimePointTable table;
        auto def = makePoint("p1", "Pressure");
        def.unit = QStringLiteral("bar");
        table.registerPoint(def);
        table.updateValue("p1", 5.0, RuntimePointQuality::Good);

        auto snap = table.snapshot("p1");
        QVERIFY(snap.has_value());
        QCOMPARE(snap->definition.name, QStringLiteral("Pressure"));
        QCOMPARE(snap->definition.unit, QStringLiteral("bar"));
        QCOMPARE(snap->current.value.toDouble(), 5.0);
    }

    void allSnapshotsCount()
    {
        RuntimePointTable table;
        table.registerPoint(makePoint("p1", "A"));
        table.registerPoint(makePoint("p2", "B"));

        QCOMPARE(table.allSnapshots().size(), 2);
    }

    void snapshotsByKind()
    {
        RuntimePointTable table;
        table.registerPoint(makePoint("p1", "A", RuntimePointKind::Variable));
        table.registerPoint(makePoint("p2", "B", RuntimePointKind::Parameter));
        table.registerPoint(makePoint("p3", "C", RuntimePointKind::Variable));

        auto varSnaps = table.snapshotsByKind(RuntimePointKind::Variable);
        QCOMPARE(varSnaps.size(), 2);
    }

    void converterProducesStableAddressingSchema()
    {
        VariableDefinition var;
        var.id = "v1";
        var.name = "Speed";
        var.dataType = "REAL";
        var.scope = "global";
        var.binding = "motor.speed";

        const auto varPt = RuntimePointConverter::fromVariable(var);
        QVERIFY(varPt.addressing.contains("schemaVersion"));
        QCOMPARE(varPt.addressing.value("role").toString(), QStringLiteral("variable"));
        QCOMPARE(varPt.addressing.value("channel").toString(), QStringLiteral("motor.speed"));
        QCOMPARE(varPt.addressing.value("address").toLongLong(), qint64(-1));

        ParameterDefinition param;
        param.id = "p1";
        param.name = "Gain";
        param.dataType = "REAL";
        param.defaultValue = "1.0";
        param.onlineEditable = true;

        const auto paramPt = RuntimePointConverter::fromParameter(param);
        QCOMPARE(paramPt.addressing.value("role").toString(), QStringLiteral("parameter"));
        QCOMPARE(paramPt.addressing.value("mode").toString(), QStringLiteral("editable"));
        QCOMPARE(paramPt.addressing.value("area").toString(), QStringLiteral("parameters"));
        QVERIFY(paramPt.addressing.value("schemaVersion").toString().startsWith(QStringLiteral("lh.runtimePointAddressing")));

        HardwareResourceBinding res;
        res.id = "r1";
        res.resourceType = "serial";
        res.resourceName = "COM1";
        res.channel = "comm.serial";
        res.owner = "runtime";
        res.exclusive = true;

        const auto resPt = RuntimePointConverter::fromResource(res);
        QCOMPARE(resPt.addressing.value("role").toString(), QStringLiteral("resource"));
        QCOMPARE(resPt.addressing.value("area").toString(), QStringLiteral("serial"));
        QCOMPARE(resPt.addressing.value("channel").toString(), QStringLiteral("comm.serial"));
        QCOMPARE(resPt.addressing.value("resourceType").toString(), QStringLiteral("serial"));

        MonitorProviderRuntimeConfig prov;
        prov.id = "m1";
        prov.channelName = "pressure";
        prov.unit = "bar";
        prov.periodMs = 25;
        prov.priority = 10;

        const auto provPt = RuntimePointConverter::fromProvider(prov);
        QCOMPARE(provPt.addressing.value("role").toString(), QStringLiteral("provider"));
        QCOMPARE(provPt.addressing.value("mode").toString(), QStringLiteral("poll"));
        QCOMPARE(provPt.addressing.value("pollGroup").toString(), QStringLiteral("default"));
        QCOMPARE(provPt.addressing.value("periodMs").toInt(), 25);

        DslMappingEntry mapping;
        mapping.id = "d1";
        mapping.snippetId = "analog_input";
        mapping.snippetName = "PressureInput";
        mapping.channelName = "pressure.input";
        mapping.signalPath = "pressure.input";
        mapping.unit = "bar";
        mapping.periodMs = 20;
        mapping.lineNumber = 12;

        const auto mapPt = RuntimePointConverter::fromDslMapping(mapping);
        QCOMPARE(mapPt.addressing.value("role").toString(), QStringLiteral("dsl_mapping"));
        QCOMPARE(mapPt.addressing.value("mode").toString(), QStringLiteral("mapping"));
        QCOMPARE(mapPt.addressing.value("channel").toString(), QStringLiteral("pressure.input"));
        QCOMPARE(mapPt.addressing.value("lineNumber").toInt(), 12);
    }

    // ── 清空 ────────────────────────────────────────────

    void clearRemovesAll()
    {
        RuntimePointTable table;
        table.registerPoint(makePoint("p1", "A"));
        table.registerPoint(makePoint("p2", "B"));
        table.clear();

        QCOMPARE(table.count(), 0);
        QVERIFY(!table.contains("p1"));
        QVERIFY(!table.definitionByName("A").has_value());
        QVERIFY(table.idsByKind(RuntimePointKind::Variable).isEmpty());
        QVERIFY(!table.snapshot("p1").has_value());
    }

    // ── 边界 ────────────────────────────────────────────

    void snapshotNonexistentReturnsNullopt()
    {
        RuntimePointTable table;
        QVERIFY(!table.snapshot("no_such").has_value());
    }

    void definitionNonexistentReturnsNullopt()
    {
        RuntimePointTable table;
        QVERIFY(!table.definition("no_such").has_value());
    }
};

QTEST_MAIN(RuntimePointTableTest)
#include "runtime_point_table_test.moc"
