#include <QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QPlainTextEdit>
#include <QScopedPointer>
#include <QSettings>
#include <QTemporaryDir>

#if defined(Q_OS_UNIX)
#include <unistd.h>
#endif

#include "designer/DslScriptEditor.h"
#include "designer/MainWindow.h"
#include "designer/ProjectController.h"

class ProjectSaveCloseTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_settingsDir;

private:
    static bool writeFile(const QString& path, const QByteArray& data)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        return file.write(data) == data.size();
    }

    static QString readFile(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString();
        }
        QString text = QString::fromUtf8(file.readAll());
        return text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    }

    static QString createProject(const QString& root,
                                 const QString& name,
                                 const QString& script)
    {
        const QString projectPath = QDir(root).absoluteFilePath(name);
        if (!QDir().mkpath(projectPath)) {
            return QString();
        }

        const QString scriptPath = QDir(projectPath).absoluteFilePath(QStringLiteral("main.lh"));
        if (!writeFile(scriptPath, script.toUtf8())) {
            return QString();
        }

        ProjectRuntimeConfig config;
        config.projectName = name;
        config.dslScriptPath = scriptPath;
        config.mainScriptPath = scriptPath;
        config.scriptFiles = {scriptPath};

        const QString configPath = QDir(projectPath).absoluteFilePath(QStringLiteral("project_config.json"));
        const QJsonDocument document(config.toJson());
        if (!writeFile(configPath, document.toJson(QJsonDocument::Indented))) {
            return QString();
        }

        return projectPath;
    }

    static bool writeProjectConfig(const QString& projectPath,
                                   const ProjectRuntimeConfig& config)
    {
        const QString configPath = QDir(projectPath).absoluteFilePath(QStringLiteral("project_config.json"));
        return writeFile(configPath,
                         QJsonDocument(config.toJson()).toJson(QJsonDocument::Indented));
    }

    static bool createSymbolicLink(const QString& target, const QString& linkPath)
    {
#if defined(Q_OS_UNIX)
        const QByteArray encodedTarget = QFile::encodeName(target);
        const QByteArray encodedLink = QFile::encodeName(linkPath);
        return ::symlink(encodedTarget.constData(), encodedLink.constData()) == 0;
#else
        return QFile::link(target, linkPath);
#endif
    }

    static void bindEditor(ProjectController& controller, DslScriptEditor& editor)
    {
        controller.setDslEditor(&editor);
        QObject::connect(&controller, &ProjectController::scriptLoadRequired,
                         &editor, [&editor](const QString& path, const QString& content) {
            editor.setScript(content);
            editor.setCurrentFilePath(path);
            editor.setModified(false);
        });
        QObject::connect(&controller, &ProjectController::editorClearRequired,
                         &editor, [&editor]() {
            editor.clearScript();
            editor.setCurrentFilePath(QString());
            editor.setModified(false);
        });
        QObject::connect(&editor, &DslScriptEditor::editorModified,
                         &controller, &ProjectController::setModified);
    }

    static void setDirtyScript(ProjectController& controller,
                               DslScriptEditor& editor,
                               const QString& script)
    {
        editor.setScript(script);
        editor.setModified(true);
        controller.setModified(true);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_settingsDir.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
    }

    void saveProjectPersistsDslWhenAuxiliaryWindowIsActive()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString projectPath = createProject(tempDir.path(), QStringLiteral("project_a"),
                                                  QStringLiteral("PROGRAM Initial\nEND_PROGRAM\n"));
        QVERIFY(!projectPath.isEmpty());

        QScopedPointer<MainWindow> window(new MainWindow());
        auto* controller = window->findChild<ProjectController*>();
        auto* editor = window->findChild<DslScriptEditor*>();
        auto* mdiArea = window->findChild<QMdiArea*>();
        QVERIFY(controller != nullptr);
        QVERIFY(editor != nullptr);
        QVERIFY(mdiArea != nullptr);
        QVERIFY(controller->openProjectFromPath(projectPath));
        window->show();
        QCoreApplication::processEvents();

        const QString changedScript = QStringLiteral("PROGRAM Updated\nEND_PROGRAM\n");
        setDirtyScript(*controller, *editor, changedScript);

        const QString auxiliaryPath = QDir(projectPath).absoluteFilePath(QStringLiteral("notes.txt"));
        QVERIFY(writeFile(auxiliaryPath, QByteArrayLiteral("old note")));
        auto* auxiliaryEditor = new QPlainTextEdit;
        auxiliaryEditor->setPlainText(QStringLiteral("updated note"));
        QMdiSubWindow* auxiliaryWindow = mdiArea->addSubWindow(auxiliaryEditor);
        auxiliaryWindow->setProperty("filePath", auxiliaryPath);
        auxiliaryWindow->show();
        mdiArea->setActiveSubWindow(auxiliaryWindow);
        QCoreApplication::processEvents();
        QCOMPARE(mdiArea->activeSubWindow(), auxiliaryWindow);

        QVERIFY(QMetaObject::invokeMethod(window.data(), "onSaveProject", Qt::DirectConnection));

        QCOMPARE(readFile(QDir(projectPath).absoluteFilePath(QStringLiteral("main.lh"))), changedScript);
        QCOMPARE(readFile(auxiliaryPath), QStringLiteral("updated note"));
        QVERIFY(!editor->isModified());
        QVERIFY(!controller->isModified());
    }

    void saveFailureKeepsEditorAndProjectModified()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString initialScript = QStringLiteral("PROGRAM Initial\nEND_PROGRAM\n");
        const QString projectPath = createProject(tempDir.path(), QStringLiteral("project_a"), initialScript);
        QVERIFY(!projectPath.isEmpty());

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(projectPath));

        const QString changedScript = QStringLiteral("PROGRAM Unsaved\nEND_PROGRAM\n");
        setDirtyScript(controller, editor, changedScript);
        controller.setCurrentScriptFile(
            QDir(tempDir.path()).absoluteFilePath(QStringLiteral("missing/main.lh")));

        QVERIFY(!controller.saveProject());
        QCOMPARE(editor.currentScript(), changedScript);
        QVERIFY(editor.isModified());
        QVERIFY(controller.isModified());
        QCOMPARE(readFile(QDir(projectPath).absoluteFilePath(QStringLiteral("main.lh"))), initialScript);
    }

    void closeCancelKeepsProjectAndEditor()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString projectPath = createProject(tempDir.path(), QStringLiteral("project_a"),
                                                  QStringLiteral("PROGRAM Initial\nEND_PROGRAM\n"));
        QVERIFY(!projectPath.isEmpty());

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(projectPath));

        const QString changedScript = QStringLiteral("PROGRAM Unsaved\nEND_PROGRAM\n");
        setDirtyScript(controller, editor, changedScript);
        connect(&controller, &ProjectController::saveConfirmationRequired,
                this, [](bool& shouldSave, bool& cancelled) {
            shouldSave = false;
            cancelled = true;
        });

        QVERIFY(!controller.closeProject());
        QCOMPARE(controller.currentProjectPath(), projectPath);
        QCOMPARE(editor.currentScript(), changedScript);
        QVERIFY(editor.isModified());
        QVERIFY(controller.isModified());
    }

    void closeSavePersistsDslBeforeClearingProject()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString projectPath = createProject(tempDir.path(), QStringLiteral("project_a"),
                                                  QStringLiteral("PROGRAM Initial\nEND_PROGRAM\n"));
        QVERIFY(!projectPath.isEmpty());

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(projectPath));

        const QString changedScript = QStringLiteral("PROGRAM SavedOnClose\nEND_PROGRAM\n");
        setDirtyScript(controller, editor, changedScript);
        connect(&controller, &ProjectController::saveConfirmationRequired,
                this, [](bool& shouldSave, bool& cancelled) {
            shouldSave = true;
            cancelled = false;
        });

        QVERIFY(controller.closeProject());
        QCOMPARE(readFile(QDir(projectPath).absoluteFilePath(QStringLiteral("main.lh"))), changedScript);
        QVERIFY(!controller.hasOpenProject());
        QVERIFY(editor.currentScript().isEmpty());
    }

    void closeSaveFailureKeepsProjectOpen()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString projectPath = createProject(tempDir.path(), QStringLiteral("project_a"),
                                                  QStringLiteral("PROGRAM Initial\nEND_PROGRAM\n"));
        QVERIFY(!projectPath.isEmpty());

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(projectPath));

        const QString changedScript = QStringLiteral("PROGRAM StillOpen\nEND_PROGRAM\n");
        setDirtyScript(controller, editor, changedScript);
        controller.setCurrentScriptFile(
            QDir(tempDir.path()).absoluteFilePath(QStringLiteral("missing/main.lh")));
        connect(&controller, &ProjectController::saveConfirmationRequired,
                this, [](bool& shouldSave, bool& cancelled) {
            shouldSave = true;
            cancelled = false;
        });

        QVERIFY(!controller.closeProject());
        QCOMPARE(controller.currentProjectPath(), projectPath);
        QCOMPARE(editor.currentScript(), changedScript);
        QVERIFY(editor.isModified());
        QVERIFY(controller.isModified());
    }

    void switchProjectCancelKeepsCurrentProject()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString projectA = createProject(tempDir.path(), QStringLiteral("project_a"),
                                               QStringLiteral("PROGRAM A\nEND_PROGRAM\n"));
        const QString projectB = createProject(tempDir.path(), QStringLiteral("project_b"),
                                               QStringLiteral("PROGRAM B\nEND_PROGRAM\n"));
        QVERIFY(!projectA.isEmpty());
        QVERIFY(!projectB.isEmpty());

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(projectA));

        const QString changedScript = QStringLiteral("PROGRAM UnsavedA\nEND_PROGRAM\n");
        setDirtyScript(controller, editor, changedScript);
        connect(&controller, &ProjectController::saveConfirmationRequired,
                this, [](bool& shouldSave, bool& cancelled) {
            shouldSave = false;
            cancelled = true;
        });

        QVERIFY(!controller.openProjectFromPath(projectB));
        QCOMPARE(controller.currentProjectPath(), projectA);
        QCOMPARE(editor.currentScript(), changedScript);
        QVERIFY(controller.isModified());
    }

    void switchProjectSavePersistsOldDslThenLoadsNewProject()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString projectA = createProject(tempDir.path(), QStringLiteral("project_a"),
                                               QStringLiteral("PROGRAM A\nEND_PROGRAM\n"));
        const QString scriptB = QStringLiteral("PROGRAM B\nEND_PROGRAM\n");
        const QString projectB = createProject(tempDir.path(), QStringLiteral("project_b"), scriptB);
        QVERIFY(!projectA.isEmpty());
        QVERIFY(!projectB.isEmpty());

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(projectA));

        const QString changedScript = QStringLiteral("PROGRAM SavedA\nEND_PROGRAM\n");
        setDirtyScript(controller, editor, changedScript);
        connect(&controller, &ProjectController::saveConfirmationRequired,
                this, [](bool& shouldSave, bool& cancelled) {
            shouldSave = true;
            cancelled = false;
        });

        QVERIFY(controller.openProjectFromPath(projectB));
        QCOMPARE(readFile(QDir(projectA).absoluteFilePath(QStringLiteral("main.lh"))), changedScript);
        QCOMPARE(controller.currentProjectPath(), projectB);
        QCOMPARE(editor.currentScript(), scriptB);
        QVERIFY(!controller.isModified());
    }

    void switchProjectSaveFailureKeepsCurrentProject()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString initialScript = QStringLiteral("PROGRAM A\nEND_PROGRAM\n");
        const QString projectA = createProject(tempDir.path(), QStringLiteral("project_a"), initialScript);
        const QString projectB = createProject(tempDir.path(), QStringLiteral("project_b"),
                                               QStringLiteral("PROGRAM B\nEND_PROGRAM\n"));
        QVERIFY(!projectA.isEmpty());
        QVERIFY(!projectB.isEmpty());

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(projectA));

        const QString changedScript = QStringLiteral("PROGRAM UnsavedA\nEND_PROGRAM\n");
        setDirtyScript(controller, editor, changedScript);
        controller.setCurrentScriptFile(
            QDir(tempDir.path()).absoluteFilePath(QStringLiteral("missing/main.lh")));
        connect(&controller, &ProjectController::saveConfirmationRequired,
                this, [](bool& shouldSave, bool& cancelled) {
            shouldSave = true;
            cancelled = false;
        });

        QVERIFY(!controller.openProjectFromPath(projectB));
        QCOMPARE(controller.currentProjectPath(), projectA);
        QCOMPARE(editor.currentScript(), changedScript);
        QVERIFY(editor.isModified());
        QVERIFY(controller.isModified());
        QCOMPARE(readFile(QDir(projectA).absoluteFilePath(QStringLiteral("main.lh"))), initialScript);
    }

    void relativeMainScriptPathIsResolvedBeforeProjectSwitch()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString projectPath = createProject(tempDir.path(), QStringLiteral("relative_project"),
                                                  QStringLiteral("PROGRAM Default\nEND_PROGRAM\n"));
        QVERIFY(!projectPath.isEmpty());
        QVERIFY(QDir().mkpath(QDir(projectPath).absoluteFilePath(QStringLiteral("scripts"))));
        const QString entryPath = QDir(projectPath).absoluteFilePath(QStringLiteral("scripts/entry.lh"));
        const QString expectedScript = QStringLiteral("PROGRAM Relative\nEND_PROGRAM\n");
        QVERIFY(writeFile(entryPath, expectedScript.toUtf8()));

        ProjectRuntimeConfig config;
        config.projectName = QStringLiteral("relative_project");
        config.mainScriptPath = QStringLiteral("scripts/entry.lh");
        config.dslScriptPath = config.mainScriptPath;
        config.scriptFiles = {config.mainScriptPath};
        QVERIFY(writeFile(QDir(projectPath).absoluteFilePath(QStringLiteral("project_config.json")),
                          QJsonDocument(config.toJson()).toJson()));

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(projectPath));
        QCOMPARE(editor.currentScript(), expectedScript);
        QCOMPARE(controller.currentScriptFile(), entryPath);
    }

    void absoluteMainScriptPathIsAllowed()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString projectPath = createProject(tempDir.path(), QStringLiteral("absolute_project"),
                                                  QStringLiteral("PROGRAM Absolute\nEND_PROGRAM\n"));
        QVERIFY(!projectPath.isEmpty());

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(projectPath));
        QCOMPARE(controller.currentScriptFile(),
                 QFileInfo(QDir(projectPath).absoluteFilePath(QStringLiteral("main.lh"))).canonicalFilePath());
    }

    void emptyMainScriptPathFallsBackToProjectMain()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString projectPath = createProject(tempDir.path(), QStringLiteral("fallback_project"),
                                                  QStringLiteral("PROGRAM Fallback\nEND_PROGRAM\n"));
        QVERIFY(!projectPath.isEmpty());

        ProjectRuntimeConfig config;
        config.projectName = QStringLiteral("fallback_project");
        QVERIFY(writeProjectConfig(projectPath, config));

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(projectPath));
        QCOMPARE(controller.currentScriptFile(),
                 QFileInfo(QDir(projectPath).absoluteFilePath(QStringLiteral("main.lh"))).canonicalFilePath());
    }

    void missingProjectAuxiliaryScriptRemainsNormalized()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString projectPath = createProject(tempDir.path(), QStringLiteral("missing_auxiliary_project"),
                                                  QStringLiteral("PROGRAM Main\nEND_PROGRAM\n"));
        QVERIFY(!projectPath.isEmpty());

        ProjectRuntimeConfig config;
        config.projectName = QStringLiteral("missing_auxiliary_project");
        config.mainScriptPath = QStringLiteral("main.lh");
        config.dslScriptPath = config.mainScriptPath;
        config.scriptFiles = {config.mainScriptPath, QStringLiteral("scripts/missing.lh")};
        QVERIFY(writeProjectConfig(projectPath, config));

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(projectPath));
        QCOMPARE(controller.projectScriptFiles().size(), 2);
        QVERIFY(controller.projectScriptFiles().contains(
                QFileInfo(QDir(projectPath).absoluteFilePath(QStringLiteral("scripts/missing.lh"))).absoluteFilePath()));
    }

    void parentMainScriptPathRejectedBeforeConfirmation()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString currentProject = createProject(tempDir.path(), QStringLiteral("current_project"),
                                                     QStringLiteral("PROGRAM Current\nEND_PROGRAM\n"));
        const QString candidateProject = createProject(tempDir.path(), QStringLiteral("candidate_project"),
                                                       QStringLiteral("PROGRAM Candidate\nEND_PROGRAM\n"));
        const QString externalScript = QDir(tempDir.path()).absoluteFilePath(QStringLiteral("outside.lh"));
        QVERIFY(writeFile(externalScript, QByteArrayLiteral("PROGRAM Outside\nEND_PROGRAM\n")));
        QVERIFY(!currentProject.isEmpty());
        QVERIFY(!candidateProject.isEmpty());

        ProjectRuntimeConfig config;
        config.projectName = QStringLiteral("candidate_project");
        config.mainScriptPath = QStringLiteral("../outside.lh");
        config.dslScriptPath = config.mainScriptPath;
        config.scriptFiles = {config.mainScriptPath};
        QVERIFY(writeProjectConfig(candidateProject, config));

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(currentProject));
        const QString dirtyScript = QStringLiteral("PROGRAM Dirty\nEND_PROGRAM\n");
        setDirtyScript(controller, editor, dirtyScript);
        int confirmations = 0;
        connect(&controller, &ProjectController::saveConfirmationRequired,
                this, [&confirmations](bool&, bool&) { ++confirmations; });

        QVERIFY(!controller.openProjectFromPath(candidateProject));
        QCOMPARE(confirmations, 0);
        QCOMPARE(controller.currentProjectPath(), currentProject);
        QCOMPARE(editor.currentScript(), dirtyScript);
        QVERIFY(controller.isModified());
        QVERIFY(QFileInfo::exists(externalScript));
    }

    void externalAbsoluteMainScriptPathRejectedBeforeConfirmation()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString currentProject = createProject(tempDir.path(), QStringLiteral("current_project"),
                                                     QStringLiteral("PROGRAM Current\nEND_PROGRAM\n"));
        const QString candidateProject = createProject(tempDir.path(), QStringLiteral("candidate_project"),
                                                       QStringLiteral("PROGRAM Candidate\nEND_PROGRAM\n"));
        const QString externalScript = QDir(tempDir.path()).absoluteFilePath(QStringLiteral("outside.lh"));
        QVERIFY(writeFile(externalScript, QByteArrayLiteral("PROGRAM Outside\nEND_PROGRAM\n")));
        QVERIFY(!currentProject.isEmpty());
        QVERIFY(!candidateProject.isEmpty());

        ProjectRuntimeConfig config;
        config.projectName = QStringLiteral("candidate_project");
        config.mainScriptPath = externalScript;
        config.dslScriptPath = externalScript;
        config.scriptFiles = {externalScript};
        QVERIFY(writeProjectConfig(candidateProject, config));

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(currentProject));
        const QString dirtyScript = QStringLiteral("PROGRAM Dirty\nEND_PROGRAM\n");
        setDirtyScript(controller, editor, dirtyScript);
        int confirmations = 0;
        connect(&controller, &ProjectController::saveConfirmationRequired,
                this, [&confirmations](bool&, bool&) { ++confirmations; });

        QVERIFY(!controller.openProjectFromPath(candidateProject));
        QCOMPARE(confirmations, 0);
        QCOMPARE(controller.currentProjectPath(), currentProject);
        QCOMPARE(editor.currentScript(), dirtyScript);
        QVERIFY(controller.isModified());
    }

    void nonLhMainScriptPathRejectedBeforeConfirmation()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString currentProject = createProject(tempDir.path(), QStringLiteral("current_project"),
                                                     QStringLiteral("PROGRAM Current\nEND_PROGRAM\n"));
        const QString candidateProject = createProject(tempDir.path(), QStringLiteral("candidate_project"),
                                                       QStringLiteral("PROGRAM Candidate\nEND_PROGRAM\n"));
        QVERIFY(!currentProject.isEmpty());
        QVERIFY(!candidateProject.isEmpty());

        ProjectRuntimeConfig config;
        config.projectName = QStringLiteral("candidate_project");
        config.mainScriptPath = QStringLiteral("main.txt");
        config.dslScriptPath = config.mainScriptPath;
        config.scriptFiles = {config.mainScriptPath};
        QVERIFY(writeProjectConfig(candidateProject, config));

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(currentProject));
        const QString dirtyScript = QStringLiteral("PROGRAM Dirty\nEND_PROGRAM\n");
        setDirtyScript(controller, editor, dirtyScript);
        int confirmations = 0;
        connect(&controller, &ProjectController::saveConfirmationRequired,
                this, [&confirmations](bool&, bool&) { ++confirmations; });

        QVERIFY(!controller.openProjectFromPath(candidateProject));
        QCOMPARE(confirmations, 0);
        QCOMPARE(controller.currentProjectPath(), currentProject);
        QCOMPARE(editor.currentScript(), dirtyScript);
        QVERIFY(controller.isModified());
    }

    void scriptFilesEscapeRejectedBeforeConfirmation()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString currentProject = createProject(tempDir.path(), QStringLiteral("current_project"),
                                                     QStringLiteral("PROGRAM Current\nEND_PROGRAM\n"));
        const QString candidateProject = createProject(tempDir.path(), QStringLiteral("candidate_project"),
                                                       QStringLiteral("PROGRAM Candidate\nEND_PROGRAM\n"));
        const QString externalScript = QDir(tempDir.path()).absoluteFilePath(QStringLiteral("outside.lh"));
        QVERIFY(writeFile(externalScript, QByteArrayLiteral("PROGRAM Outside\nEND_PROGRAM\n")));
        QVERIFY(!currentProject.isEmpty());
        QVERIFY(!candidateProject.isEmpty());

        ProjectRuntimeConfig config;
        config.projectName = QStringLiteral("candidate_project");
        config.mainScriptPath = QStringLiteral("main.lh");
        config.dslScriptPath = config.mainScriptPath;
        config.scriptFiles = {config.mainScriptPath, QStringLiteral("../outside.lh")};
        QVERIFY(writeProjectConfig(candidateProject, config));

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(currentProject));
        const QString dirtyScript = QStringLiteral("PROGRAM Dirty\nEND_PROGRAM\n");
        setDirtyScript(controller, editor, dirtyScript);
        int confirmations = 0;
        connect(&controller, &ProjectController::saveConfirmationRequired,
                this, [&confirmations](bool&, bool&) { ++confirmations; });

        QVERIFY(!controller.openProjectFromPath(candidateProject));
        QCOMPARE(confirmations, 0);
        QCOMPARE(controller.currentProjectPath(), currentProject);
        QCOMPARE(editor.currentScript(), dirtyScript);
        QVERIFY(controller.isModified());
    }

    void prefixSimilarDirectoryIsRejected()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString currentProject = createProject(tempDir.path(), QStringLiteral("current_project"),
                                                     QStringLiteral("PROGRAM Current\nEND_PROGRAM\n"));
        const QString candidateProject = createProject(tempDir.path(), QStringLiteral("project"),
                                                       QStringLiteral("PROGRAM Candidate\nEND_PROGRAM\n"));
        const QString siblingProject = createProject(tempDir.path(), QStringLiteral("project-sibling"),
                                                     QStringLiteral("PROGRAM Sibling\nEND_PROGRAM\n"));
        QVERIFY(!currentProject.isEmpty());
        QVERIFY(!candidateProject.isEmpty());
        QVERIFY(!siblingProject.isEmpty());

        ProjectRuntimeConfig config;
        config.projectName = QStringLiteral("project");
        config.mainScriptPath = QStringLiteral("../project-sibling/main.lh");
        config.dslScriptPath = config.mainScriptPath;
        config.scriptFiles = {config.mainScriptPath};
        QVERIFY(writeProjectConfig(candidateProject, config));

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(currentProject));
        const QString dirtyScript = QStringLiteral("PROGRAM Dirty\nEND_PROGRAM\n");
        setDirtyScript(controller, editor, dirtyScript);
        int confirmations = 0;
        connect(&controller, &ProjectController::saveConfirmationRequired,
                this, [&confirmations](bool&, bool&) { ++confirmations; });

        QVERIFY(!controller.openProjectFromPath(candidateProject));
        QCOMPARE(confirmations, 0);
        QCOMPARE(controller.currentProjectPath(), currentProject);
        QCOMPARE(editor.currentScript(), dirtyScript);
        QVERIFY(controller.isModified());
    }

    void symlinkToExternalMainScriptIsRejected()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString currentProject = createProject(tempDir.path(), QStringLiteral("current_project"),
                                                     QStringLiteral("PROGRAM Current\nEND_PROGRAM\n"));
        const QString candidateProject = createProject(tempDir.path(), QStringLiteral("candidate_project"),
                                                       QStringLiteral("PROGRAM Candidate\nEND_PROGRAM\n"));
        const QString externalScript = QDir(tempDir.path()).absoluteFilePath(QStringLiteral("outside.lh"));
        const QString linkDir = QDir(candidateProject).absoluteFilePath(QStringLiteral("links"));
        const QString linkPath = QDir(linkDir).absoluteFilePath(QStringLiteral("outside.lh"));
        QVERIFY(writeFile(externalScript, QByteArrayLiteral("PROGRAM Outside\nEND_PROGRAM\n")));
        QVERIFY(QDir().mkpath(linkDir));
        if (!createSymbolicLink(externalScript, linkPath) || !QFileInfo(linkPath).isSymLink()) {
            QSKIP("当前平台或 Qt 构建不支持可识别的符号链接测试");
        }
        QVERIFY(!currentProject.isEmpty());
        QVERIFY(!candidateProject.isEmpty());

        ProjectRuntimeConfig config;
        config.projectName = QStringLiteral("candidate_project");
        config.mainScriptPath = linkPath;
        config.dslScriptPath = linkPath;
        config.scriptFiles = {linkPath};
        QVERIFY(writeProjectConfig(candidateProject, config));

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(currentProject));
        const QString dirtyScript = QStringLiteral("PROGRAM Dirty\nEND_PROGRAM\n");
        setDirtyScript(controller, editor, dirtyScript);
        int confirmations = 0;
        connect(&controller, &ProjectController::saveConfirmationRequired,
                this, [&confirmations](bool&, bool&) { ++confirmations; });

        QVERIFY(!controller.openProjectFromPath(candidateProject));
        QCOMPARE(confirmations, 0);
        QCOMPARE(controller.currentProjectPath(), currentProject);
        QCOMPARE(editor.currentScript(), dirtyScript);
        QVERIFY(controller.isModified());
    }

    void symlinkDirectoryMissingAuxiliaryScriptIsRejected()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString currentProject = createProject(tempDir.path(), QStringLiteral("current_project"),
                                                     QStringLiteral("PROGRAM Current\nEND_PROGRAM\n"));
        const QString candidateProject = createProject(tempDir.path(), QStringLiteral("candidate_project"),
                                                       QStringLiteral("PROGRAM Candidate\nEND_PROGRAM\n"));
        const QString externalDir = QDir(tempDir.path()).absoluteFilePath(QStringLiteral("external_scripts"));
        const QString linkDir = QDir(candidateProject).absoluteFilePath(QStringLiteral("links"));
        QVERIFY(QDir().mkpath(externalDir));
        if (!createSymbolicLink(externalDir, linkDir) || !QFileInfo(linkDir).isSymLink()) {
            QSKIP("当前平台或 Qt 构建不支持可识别的目录符号链接测试");
        }
        QVERIFY(!currentProject.isEmpty());
        QVERIFY(!candidateProject.isEmpty());

        ProjectRuntimeConfig config;
        config.projectName = QStringLiteral("candidate_project");
        config.mainScriptPath = QStringLiteral("main.lh");
        config.dslScriptPath = config.mainScriptPath;
        config.scriptFiles = {config.mainScriptPath, QStringLiteral("links/missing.lh")};
        QVERIFY(writeProjectConfig(candidateProject, config));

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(currentProject));
        const QString dirtyScript = QStringLiteral("PROGRAM Dirty\nEND_PROGRAM\n");
        setDirtyScript(controller, editor, dirtyScript);
        int confirmations = 0;
        connect(&controller, &ProjectController::saveConfirmationRequired,
                this, [&confirmations](bool&, bool&) { ++confirmations; });

        QVERIFY(!controller.openProjectFromPath(candidateProject));
        QCOMPARE(confirmations, 0);
        QCOMPARE(controller.currentProjectPath(), currentProject);
        QCOMPARE(editor.currentScript(), dirtyScript);
        QVERIFY(controller.isModified());
    }

    void invalidCandidateDoesNotRequestSaveConfirmation()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString projectPath = createProject(tempDir.path(), QStringLiteral("project_a"),
                                                  QStringLiteral("PROGRAM A\nEND_PROGRAM\n"));
        QVERIFY(!projectPath.isEmpty());

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(projectPath));
        setDirtyScript(controller, editor, QStringLiteral("PROGRAM Dirty\nEND_PROGRAM\n"));
        int confirmations = 0;
        connect(&controller, &ProjectController::saveConfirmationRequired, this,
                [&confirmations](bool&, bool&) { ++confirmations; });

        const QString invalidPath = QDir(tempDir.path()).absoluteFilePath(QStringLiteral("invalid"));
        QVERIFY(QDir().mkpath(invalidPath));
        QVERIFY(!controller.openProjectFromPath(invalidPath));
        QCOMPARE(confirmations, 0);
        QCOMPARE(controller.currentProjectPath(), projectPath);
        QVERIFY(controller.isModified());
    }

    void nonObjectConfigDoesNotRequestSaveConfirmation()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString projectPath = createProject(tempDir.path(), QStringLiteral("project_a"),
                                                  QStringLiteral("PROGRAM A\nEND_PROGRAM\n"));
        const QString invalidProject = createProject(tempDir.path(), QStringLiteral("invalid_config"),
                                                     QStringLiteral("PROGRAM Invalid\nEND_PROGRAM\n"));
        QVERIFY(!projectPath.isEmpty());
        QVERIFY(!invalidProject.isEmpty());
        QVERIFY(writeFile(QDir(invalidProject).absoluteFilePath(QStringLiteral("project_config.json")),
                          QByteArrayLiteral("[]")));

        DslScriptEditor editor;
        ProjectController controller;
        bindEditor(controller, editor);
        QVERIFY(controller.openProjectFromPath(projectPath));
        setDirtyScript(controller, editor, QStringLiteral("PROGRAM Dirty\nEND_PROGRAM\n"));
        int confirmations = 0;
        connect(&controller, &ProjectController::saveConfirmationRequired, this,
                [&confirmations](bool&, bool&) { ++confirmations; });

        QVERIFY(!controller.openProjectFromPath(invalidProject));
        QCOMPARE(confirmations, 0);
        QCOMPARE(controller.currentProjectPath(), projectPath);
        QVERIFY(controller.isModified());
    }

    void duplicateProjectNameDoesNotRequestSaveConfirmation()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString projectPath = createProject(tempDir.path(), QStringLiteral("project_a"),
                                                  QStringLiteral("PROGRAM A\nEND_PROGRAM\n"));
        QVERIFY(!projectPath.isEmpty());

        ProjectController controller;
        QVERIFY(controller.openProjectFromPath(projectPath));
        controller.setModified(true);
        int confirmations = 0;
        connect(&controller, &ProjectController::saveConfirmationRequired, this,
                [&confirmations](bool&, bool&) { ++confirmations; });
        connect(&controller, &ProjectController::projectNameRequired, this,
                [](QString& name, bool& accepted) { name = QStringLiteral("project_a"); accepted = true; });
        connect(&controller, &ProjectController::directorySelectionRequired, this,
                [&tempDir](const QString&, const QString&, QString& path, bool& accepted) {
                    path = tempDir.path(); accepted = true;
                });

        controller.createNewProject();
        QCOMPARE(confirmations, 0);
        QCOMPARE(readFile(QDir(projectPath).absoluteFilePath(QStringLiteral("main.lh"))),
                 QStringLiteral("PROGRAM A\nEND_PROGRAM\n"));
    }

    void projectSchemaCompatibilityAndShapeErrorsKeepState()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString currentProject = createProject(tempDir.path(), QStringLiteral("current"),
                                                      QStringLiteral("PROGRAM Current\nEND_PROGRAM\n"));
        const QString legacyProject = createProject(tempDir.path(), QStringLiteral("legacy"),
                                                    QStringLiteral("PROGRAM Legacy\nEND_PROGRAM\n"));
        const QString malformedProject = createProject(tempDir.path(), QStringLiteral("malformed"),
                                                       QStringLiteral("PROGRAM Malformed\nEND_PROGRAM\n"));
        QVERIFY(!currentProject.isEmpty());
        QVERIFY(!legacyProject.isEmpty());
        QVERIFY(!malformedProject.isEmpty());

        const QString legacyConfigPath = QDir(legacyProject).absoluteFilePath(
            QStringLiteral("project_config.json"));
        QJsonDocument legacyDocument = QJsonDocument::fromJson(readFile(legacyConfigPath).toUtf8());
        QVERIFY(legacyDocument.isObject());
        QJsonObject legacyObject = legacyDocument.object();
        legacyObject.remove(QStringLiteral("schemaVersion"));
        QVERIFY(writeFile(legacyConfigPath,
                          QJsonDocument(legacyObject).toJson(QJsonDocument::Indented)));

        ProjectController controller;
        QVERIFY(controller.openProjectFromPath(currentProject));
        QVERIFY(controller.openProjectFromPath(legacyProject));
        QCOMPARE(controller.runtimeConfig().schemaVersion,
                 ProjectRuntimeConfig::kCurrentSchemaVersion);

        legacyObject.insert(QStringLiteral("schemaVersion"), 1);
        QVERIFY(writeFile(legacyConfigPath,
                          QJsonDocument(legacyObject).toJson(QJsonDocument::Indented)));
        QVERIFY(controller.openProjectFromPath(legacyProject));
        QCOMPARE(controller.runtimeConfig().schemaVersion, 1);

        legacyObject.insert(QStringLiteral("schemaVersion"), 2);
        QVERIFY(writeFile(legacyConfigPath,
                          QJsonDocument(legacyObject).toJson(QJsonDocument::Indented)));
        QVERIFY(controller.openProjectFromPath(legacyProject));
        QCOMPARE(controller.runtimeConfig().schemaVersion, 2);
        controller.setModified(true);

        QJsonDocument malformedDocument = QJsonDocument::fromJson(
            readFile(QDir(malformedProject).absoluteFilePath(QStringLiteral("project_config.json")))
                .toUtf8());
        QVERIFY(malformedDocument.isObject());
        QJsonObject malformedObject = malformedDocument.object();
        malformedObject.insert(QStringLiteral("schemaVersion"), 4);
        malformedObject.insert(QStringLiteral("controller"), QStringLiteral("not-an-object"));
        malformedObject.insert(QStringLiteral("providers"), QJsonArray{QStringLiteral("not-an-object")});
        malformedObject.insert(QStringLiteral("opcServer"),
                               QJsonObject{{QStringLiteral("enabled"), QStringLiteral("not-a-bool")}});
        QVERIFY(writeFile(QDir(malformedProject).absoluteFilePath(QStringLiteral("project_config.json")),
                          QJsonDocument(malformedObject).toJson(QJsonDocument::Indented)));

        QStringList loadErrors;
        connect(&controller, &ProjectController::errorOccurred, this,
                [&loadErrors](const QString&, const QString& message) {
            loadErrors.append(message);
        });
        QVERIFY(!controller.openProjectFromPath(malformedProject));
        QCOMPARE(controller.currentProjectPath(), legacyProject);
        QVERIFY(controller.isModified());
        QVERIFY(!loadErrors.isEmpty());
        QVERIFY(loadErrors.constLast().contains(QStringLiteral("project_config.controller")));
        QVERIFY(loadErrors.constLast().contains(QStringLiteral("not-an-object")));
        QVERIFY(loadErrors.constLast().contains(QStringLiteral("schemaVersion")));
        QVERIFY(loadErrors.constLast().contains(QStringLiteral("providers[0]")));
        QVERIFY(loadErrors.constLast().contains(QStringLiteral("opcServer.enabled")));
    }

    void configurationBoundsRejectInvalidValuesWithoutClamping()
    {
        ProjectController controller;
        ProjectRuntimeConfig& config = controller.runtimeConfig();
        config.clear();
        config.projectName = QStringLiteral("bounds");

        MonitorProviderRuntimeConfig provider;
        provider.id = QStringLiteral("provider-1");
        provider.channelName = QStringLiteral("channel-1");
        provider.periodMs = 10000;
        provider.priority = 255;
        config.providers.append(provider);

        DslMappingEntry mapping;
        mapping.id = QStringLiteral("mapping-1");
        mapping.snippetId = QStringLiteral("snippet-1");
        mapping.periodMs = 1;
        mapping.lineNumber = -1;
        config.dslMappings.append(mapping);

        ParameterDefinition parameter;
        parameter.id = QStringLiteral("parameter-1");
        parameter.name = QStringLiteral("gain");
        parameter.dataType = QStringLiteral("REAL");
        parameter.minValue = QStringLiteral("0");
        parameter.maxValue = QStringLiteral("10");
        parameter.defaultValue = QStringLiteral("5");
        parameter.currentValue = QStringLiteral("5");
        config.parameters.append(parameter);

        config.opcServer.enabled = true;
        config.transport.parameters.insert(QStringLiteral("timeoutMs"), QStringLiteral("2000"));
        config.transport.parameters.insert(QStringLiteral("retryCount"), QStringLiteral("3"));
        config.transport.parameters.insert(QStringLiteral("dataBits"), QStringLiteral("8"));
        config.transport.parameters.insert(QStringLiteral("stopBits"), QStringLiteral("1"));
        config.transport.parameters.insert(QStringLiteral("baudRate"), QStringLiteral("115200"));

        QStringList errors;
        QVERIFY(controller.validateConfiguration(errors));

        config.providers[0].priority = 256;
        QVERIFY(!controller.validateConfiguration(errors));
        QVERIFY(errors.join(QStringLiteral("\n")).contains(QStringLiteral("priority")));
        QVERIFY(errors.join(QStringLiteral("\n")).contains(QStringLiteral("256")));
        QCOMPARE(config.providers[0].priority, 256);

        config.providers[0].priority = 255;
        config.dslMappings[0].periodMs = 10001;
        config.dslMappings[0].lineNumber = -2;
        QVERIFY(!controller.validateConfiguration(errors));
        QVERIFY(errors.join(QStringLiteral("\n")).contains(QStringLiteral("periodMs")));
        QVERIFY(errors.join(QStringLiteral("\n")).contains(QStringLiteral("lineNumber")));

        config.dslMappings[0].periodMs = 1;
        config.dslMappings[0].lineNumber = -1;
        config.parameters[0].minValue = QStringLiteral("10");
        config.parameters[0].maxValue = QStringLiteral("0");
        config.parameters[0].defaultValue = QStringLiteral("11");
        config.opcServer.maxRegistersPerRequest = 126;
        config.transport.parameters[QStringLiteral("timeoutMs")] = QStringLiteral("not-a-number");
        QVERIFY(!controller.validateConfiguration(errors));
        const QString joinedErrors = errors.join(QStringLiteral("\n"));
        QVERIFY(joinedErrors.contains(QStringLiteral("minValue/maxValue")));
        QVERIFY(joinedErrors.contains(QStringLiteral("maxRegistersPerRequest")));
        QVERIFY(joinedErrors.contains(QStringLiteral("timeoutMs")));
        QVERIFY(joinedErrors.contains(QStringLiteral("not-a-number")));
        QCOMPARE(config.opcServer.maxRegistersPerRequest, 126);
    }

    void schemaShapeFailuresKeepCurrentStateIndependently()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString baselineProject = createProject(tempDir.path(), QStringLiteral("baseline"),
                                                       QStringLiteral("PROGRAM Baseline\nEND_PROGRAM\n"));
        const QString controllerTypeProject = createProject(tempDir.path(), QStringLiteral("bad_controller"),
                                                            QStringLiteral("PROGRAM BadController\nEND_PROGRAM\n"));
        const QString fractionalSchemaProject = createProject(tempDir.path(), QStringLiteral("fractional_schema"),
                                                              QStringLiteral("PROGRAM Fractional\nEND_PROGRAM\n"));
        const QString textualSchemaProject = createProject(tempDir.path(), QStringLiteral("textual_schema"),
                                                           QStringLiteral("PROGRAM Textual\nEND_PROGRAM\n"));
        QVERIFY(!baselineProject.isEmpty());
        QVERIFY(!controllerTypeProject.isEmpty());
        QVERIFY(!fractionalSchemaProject.isEmpty());
        QVERIFY(!textualSchemaProject.isEmpty());

        const auto updateConfig = [&](const QString& projectPath,
                                      const QString& key,
                                      const QJsonValue& value) {
            const QString configPath = QDir(projectPath).absoluteFilePath(
                QStringLiteral("project_config.json"));
            const QJsonDocument document = QJsonDocument::fromJson(readFile(configPath).toUtf8());
            if (!document.isObject()) {
                return false;
            }
            QJsonObject object = document.object();
            object.insert(key, value);
            return writeFile(configPath,
                             QJsonDocument(object).toJson(QJsonDocument::Indented));
        };

        QVERIFY(updateConfig(controllerTypeProject, QStringLiteral("schemaVersion"), 2));
        QVERIFY(updateConfig(controllerTypeProject, QStringLiteral("controller"),
                             QJsonArray{QJsonValue(1)}));
        QVERIFY(updateConfig(fractionalSchemaProject, QStringLiteral("schemaVersion"), 2.5));
        QVERIFY(updateConfig(textualSchemaProject, QStringLiteral("schemaVersion"),
                             QStringLiteral("schema-not-number")));

        ProjectController controller;
        QVERIFY(controller.openProjectFromPath(baselineProject));
        controller.setModified(true);
        const QString baselineName = controller.runtimeConfig().projectName;
        const int baselineSchema = controller.runtimeConfig().schemaVersion;
        const QString baselineMainScript = controller.runtimeConfig().mainScriptPath;
        const QStringList baselineScripts = controller.runtimeConfig().scriptFiles;
        QStringList loadErrors;
        connect(&controller, &ProjectController::errorOccurred, this,
                [&loadErrors](const QString&, const QString& message) {
            loadErrors.append(message);
        });

        const auto rejectedWithoutMutation = [&](const QString& candidateProject,
                                                  const QString& fieldPath,
                                                  const QString& valueText) {
            loadErrors.clear();
            const bool opened = controller.openProjectFromPath(candidateProject);
            if (opened || loadErrors.isEmpty()) {
                return false;
            }
            const QString message = loadErrors.constLast();
            return controller.currentProjectPath() == baselineProject
                    && controller.isModified()
                    && controller.runtimeConfig().projectName == baselineName
                    && controller.runtimeConfig().schemaVersion == baselineSchema
                    && controller.runtimeConfig().mainScriptPath == baselineMainScript
                    && controller.runtimeConfig().scriptFiles == baselineScripts
                    && message.contains(fieldPath)
                    && message.contains(valueText);
        };

        QVERIFY(rejectedWithoutMutation(controllerTypeProject,
                                       QStringLiteral("project_config.controller"),
                                       QStringLiteral("<数组>")));
        QVERIFY(rejectedWithoutMutation(fractionalSchemaProject,
                                       QStringLiteral("project_config.schemaVersion"),
                                       QStringLiteral("2.5")));
        QVERIFY(rejectedWithoutMutation(textualSchemaProject,
                                       QStringLiteral("project_config.schemaVersion"),
                                       QStringLiteral("schema-not-number")));
    }

    void configurationBoundaryMatrixUsesIndependentFailures()
    {
        ProjectController controller;
        ProjectRuntimeConfig& config = controller.runtimeConfig();

        const auto resetValidConfiguration = [&]() {
            config.clear();
            config.projectName = QStringLiteral("boundary-matrix");

            MonitorProviderRuntimeConfig provider;
            provider.id = QStringLiteral("provider-1");
            provider.channelName = QStringLiteral("channel-1");
            provider.periodMs = 20;
            provider.priority = 128;
            config.providers.append(provider);

            DslMappingEntry mapping;
            mapping.id = QStringLiteral("mapping-1");
            mapping.snippetId = QStringLiteral("snippet-1");
            mapping.periodMs = 20;
            mapping.lineNumber = 1;
            config.dslMappings.append(mapping);

            ControllerConfig controllerConfig;
            controllerConfig.model = QStringLiteral("enabled-controller");
            controllerConfig.modbusSlaveId = 1;
            config.controller = controllerConfig;

            ParameterDefinition parameter;
            parameter.id = QStringLiteral("parameter-1");
            parameter.name = QStringLiteral("gain");
            parameter.dataType = QStringLiteral("REAL");
            parameter.minValue = QStringLiteral("0");
            parameter.maxValue = QStringLiteral("10");
            parameter.defaultValue = QStringLiteral("5");
            parameter.currentValue = QStringLiteral("5");
            config.parameters.append(parameter);

            config.opcServer.enabled = true;
            config.opcServer.publishIntervalMs = 100;
            config.opcServer.timeoutMs = 1000;
            config.opcServer.reconnectDelayMs = 100;
            config.opcServer.retries = 3;
            config.opcServer.maxRegistersPerRequest = 10;

            config.transport.parameters.insert(QStringLiteral("timeoutMs"), 1000);
            config.transport.parameters.insert(QStringLiteral("retryCount"), 3);
            config.transport.parameters.insert(QStringLiteral("dataBits"), 8);
            config.transport.parameters.insert(QStringLiteral("stopBits"), 1);
            config.transport.parameters.insert(QStringLiteral("baudRate"), 115200);
        };
        const auto isValid = [&]() {
            QStringList errors;
            return controller.validateConfiguration(errors) && errors.isEmpty();
        };
        const auto hasInvalidError = [&](const QString& field, const QString& value) {
            QStringList errors;
            const bool valid = controller.validateConfiguration(errors);
            const QString message = errors.join(QStringLiteral("\n"));
            return !valid && !errors.isEmpty()
                    && message.contains(field)
                    && message.contains(value);
        };

        resetValidConfiguration();
        config.providers[0].periodMs = 1;
        QVERIFY(isValid());
        resetValidConfiguration();
        config.providers[0].periodMs = 10000;
        QVERIFY(isValid());
        resetValidConfiguration();
        config.providers[0].periodMs = 0;
        QVERIFY(hasInvalidError(QStringLiteral("periodMs"), QStringLiteral("0")));
        resetValidConfiguration();
        config.providers[0].periodMs = 10001;
        QVERIFY(hasInvalidError(QStringLiteral("periodMs"), QStringLiteral("10001")));

        resetValidConfiguration();
        config.controller.modbusSlaveId = 1;
        QVERIFY(isValid());
        resetValidConfiguration();
        config.controller.modbusSlaveId = 247;
        QVERIFY(isValid());
        resetValidConfiguration();
        config.controller.modbusSlaveId = 0;
        QVERIFY(hasInvalidError(QStringLiteral("modbusSlaveId"), QStringLiteral("0")));
        resetValidConfiguration();
        config.controller.modbusSlaveId = 248;
        QVERIFY(hasInvalidError(QStringLiteral("modbusSlaveId"), QStringLiteral("248")));

        resetValidConfiguration();
        config.parameters[0].minValue = QStringLiteral("0");
        config.parameters[0].maxValue = QStringLiteral("10");
        config.parameters[0].defaultValue = QStringLiteral("0");
        config.parameters[0].currentValue = QStringLiteral("10");
        QVERIFY(isValid());
        resetValidConfiguration();
        config.parameters[0].minValue = QStringLiteral("10");
        config.parameters[0].maxValue = QStringLiteral("10");
        config.parameters[0].defaultValue = QStringLiteral("10");
        config.parameters[0].currentValue = QStringLiteral("10");
        QVERIFY(isValid());
        resetValidConfiguration();
        config.parameters[0].minValue = QStringLiteral("not-a-number");
        QVERIFY(hasInvalidError(QStringLiteral("minValue"), QStringLiteral("not-a-number")));
        resetValidConfiguration();
        config.parameters[0].maxValue = QStringLiteral("not-a-number");
        QVERIFY(hasInvalidError(QStringLiteral("maxValue"), QStringLiteral("not-a-number")));
        resetValidConfiguration();
        config.parameters[0].minValue = QStringLiteral("11");
        config.parameters[0].maxValue = QStringLiteral("10");
        config.parameters[0].defaultValue.clear();
        config.parameters[0].currentValue.clear();
        QVERIFY(hasInvalidError(QStringLiteral("minValue/maxValue"), QStringLiteral("11/10")));
        resetValidConfiguration();
        config.parameters[0].defaultValue = QStringLiteral("-1");
        QVERIFY(hasInvalidError(QStringLiteral("defaultValue"), QStringLiteral("-1")));
        resetValidConfiguration();
        config.parameters[0].defaultValue = QStringLiteral("11");
        QVERIFY(hasInvalidError(QStringLiteral("defaultValue"), QStringLiteral("11")));
        resetValidConfiguration();
        config.parameters[0].currentValue = QStringLiteral("-1");
        QVERIFY(hasInvalidError(QStringLiteral("currentValue"), QStringLiteral("-1")));
        resetValidConfiguration();
        config.parameters[0].currentValue = QStringLiteral("11");
        QVERIFY(hasInvalidError(QStringLiteral("currentValue"), QStringLiteral("11")));

        resetValidConfiguration();
        config.opcServer.publishIntervalMs = 10;
        config.opcServer.timeoutMs = 1;
        config.opcServer.reconnectDelayMs = 0;
        config.opcServer.retries = 0;
        config.opcServer.maxRegistersPerRequest = 1;
        QVERIFY(isValid());
        resetValidConfiguration();
        config.opcServer.publishIntervalMs = 60000;
        config.opcServer.timeoutMs = 600000;
        config.opcServer.reconnectDelayMs = 600000;
        config.opcServer.retries = 100;
        config.opcServer.maxRegistersPerRequest = 125;
        QVERIFY(isValid());

        resetValidConfiguration();
        config.opcServer.publishIntervalMs = 9;
        QVERIFY(hasInvalidError(QStringLiteral("publishIntervalMs"), QStringLiteral("9")));
        resetValidConfiguration();
        config.opcServer.publishIntervalMs = 60001;
        QVERIFY(hasInvalidError(QStringLiteral("publishIntervalMs"), QStringLiteral("60001")));
        resetValidConfiguration();
        config.opcServer.timeoutMs = 0;
        QVERIFY(hasInvalidError(QStringLiteral("timeoutMs"), QStringLiteral("0")));
        resetValidConfiguration();
        config.opcServer.timeoutMs = 600001;
        QVERIFY(hasInvalidError(QStringLiteral("timeoutMs"), QStringLiteral("600001")));
        resetValidConfiguration();
        config.opcServer.reconnectDelayMs = -1;
        QVERIFY(hasInvalidError(QStringLiteral("reconnectDelayMs"), QStringLiteral("-1")));
        resetValidConfiguration();
        config.opcServer.reconnectDelayMs = 600001;
        QVERIFY(hasInvalidError(QStringLiteral("reconnectDelayMs"), QStringLiteral("600001")));
        resetValidConfiguration();
        config.opcServer.retries = -1;
        QVERIFY(hasInvalidError(QStringLiteral("retries"), QStringLiteral("-1")));
        resetValidConfiguration();
        config.opcServer.retries = 101;
        QVERIFY(hasInvalidError(QStringLiteral("retries"), QStringLiteral("101")));
        resetValidConfiguration();
        config.opcServer.maxRegistersPerRequest = 0;
        QVERIFY(hasInvalidError(QStringLiteral("maxRegistersPerRequest"), QStringLiteral("0")));
        resetValidConfiguration();
        config.opcServer.maxRegistersPerRequest = 126;
        QVERIFY(hasInvalidError(QStringLiteral("maxRegistersPerRequest"), QStringLiteral("126")));

        resetValidConfiguration();
        config.transport.parameters[QStringLiteral("timeoutMs")] = 1;
        config.transport.parameters[QStringLiteral("retryCount")] = 0;
        config.transport.parameters[QStringLiteral("dataBits")] = 5;
        config.transport.parameters[QStringLiteral("stopBits")] = 1;
        config.transport.parameters[QStringLiteral("baudRate")] = 1;
        QVERIFY(isValid());
        resetValidConfiguration();
        config.transport.parameters[QStringLiteral("timeoutMs")] = 600000;
        config.transport.parameters[QStringLiteral("retryCount")] = 100;
        config.transport.parameters[QStringLiteral("dataBits")] = 8;
        config.transport.parameters[QStringLiteral("stopBits")] = 2;
        config.transport.parameters[QStringLiteral("baudRate")] = 4000000;
        QVERIFY(isValid());

        resetValidConfiguration();
        config.transport.parameters[QStringLiteral("timeoutMs")] = 0;
        QVERIFY(hasInvalidError(QStringLiteral("timeoutMs"), QStringLiteral("0")));
        resetValidConfiguration();
        config.transport.parameters[QStringLiteral("timeoutMs")] = 600001;
        QVERIFY(hasInvalidError(QStringLiteral("timeoutMs"), QStringLiteral("600001")));
        resetValidConfiguration();
        config.transport.parameters[QStringLiteral("retryCount")] = -1;
        QVERIFY(hasInvalidError(QStringLiteral("retryCount"), QStringLiteral("-1")));
        resetValidConfiguration();
        config.transport.parameters[QStringLiteral("retryCount")] = 101;
        QVERIFY(hasInvalidError(QStringLiteral("retryCount"), QStringLiteral("101")));
        resetValidConfiguration();
        config.transport.parameters[QStringLiteral("dataBits")] = 4;
        QVERIFY(hasInvalidError(QStringLiteral("dataBits"), QStringLiteral("4")));
        resetValidConfiguration();
        config.transport.parameters[QStringLiteral("dataBits")] = 9;
        QVERIFY(hasInvalidError(QStringLiteral("dataBits"), QStringLiteral("9")));
        resetValidConfiguration();
        config.transport.parameters[QStringLiteral("stopBits")] = 0;
        QVERIFY(hasInvalidError(QStringLiteral("stopBits"), QStringLiteral("0")));
        resetValidConfiguration();
        config.transport.parameters[QStringLiteral("stopBits")] = 3;
        QVERIFY(hasInvalidError(QStringLiteral("stopBits"), QStringLiteral("3")));
        resetValidConfiguration();
        config.transport.parameters[QStringLiteral("baudRate")] = 0;
        QVERIFY(hasInvalidError(QStringLiteral("baudRate"), QStringLiteral("0")));
        resetValidConfiguration();
        config.transport.parameters[QStringLiteral("baudRate")] = 4000001;
        QVERIFY(hasInvalidError(QStringLiteral("baudRate"), QStringLiteral("4000001")));
    }
};

QTEST_MAIN(ProjectSaveCloseTest)
#include "project_save_close_test.moc"
