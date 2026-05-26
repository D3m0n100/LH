#include <QtTest>

#include "DslScriptEditor.h"

class DslScriptEditorSaveTest : public QObject
{
    Q_OBJECT

private slots:
    void scriptForSaveRemovesMappingMarkersOnly()
    {
        DslScriptEditor editor;
        editor.setScript(QStringLiteral(
            "ValveOpen := TRUE;\n"
            "// @dsl_mapping_id: 123e4567-e89b-12d3-a456-426614174000\n"
            "ValveClose := FALSE;\n"
            "  // @dsl_mapping_id: abcdefab-1234-5678-9abc-def012345678\n"
            "// keep this normal comment\n"));

        const QString saved = editor.scriptForSave();

        QVERIFY(!saved.contains(QStringLiteral("@dsl_mapping_id")));
        QVERIFY(saved.contains(QStringLiteral("ValveOpen := TRUE;")));
        QVERIFY(saved.contains(QStringLiteral("ValveClose := FALSE;")));
        QVERIFY(saved.contains(QStringLiteral("// keep this normal comment")));
    }
};

QTEST_MAIN(DslScriptEditorSaveTest)
#include "dsl_script_editor_save_test.moc"
