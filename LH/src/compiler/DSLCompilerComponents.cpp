#include "DSLCompilerInterface.h"
#include "TextEncoding.h"

#include <QDebug>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>

bool DSLCompilerInterface::listComponents(QString* compilerStdout,
                                          QString* compilerStderr)
{
    const QString python = resolvePythonInterpreter();
    if (python.isEmpty()) {
        if (compilerStderr)
            *compilerStderr = QStringLiteral(
                    "No suitable Python interpreter found for DSL compile. "
                    "Recreate third_party/custom_dsp_language/compile/venv "
                    "and install requirements.txt, or install Python 3 plus antlr4-python3-runtime in PATH.");
        return false;
    }

    const QString workDir = compilerWorkingDir();
    const QString entryScript = compilerEntryScript();
    if (!QFileInfo::exists(entryScript)) {
        const QString msg =
                QStringLiteral("Compiler entry script not found: %1").arg(entryScript);
        qWarning() << "[DSLCompilerInterface]" << msg;
        if (compilerStderr)
            *compilerStderr = msg;
        return false;
    }

    QStringList args;
    args << QStringLiteral("-c")
         << QStringLiteral(
               "import json, sys; "
               "from pathlib import Path; "
               "sys.path.insert(0, str(Path('.').resolve() / 'src')); "
               "from lh_compiler.function_blocks.registry import FunctionBlockRegistry; "
               "registry = FunctionBlockRegistry(); registry.load_defaults(); "
               "names = sorted(registry.list_names()); "
               "print(json.dumps({'components': names}, ensure_ascii=False))");

    QProcess proc;
    proc.setProgram(python);
    proc.setArguments(args);
    proc.setWorkingDirectory(workDir);

    qInfo() << "[DSLCompilerInterface] Running:" << python << args
            << "cwd =" << workDir;

    proc.start();
    if (!proc.waitForFinished(30 * 1000)) {
        proc.kill();
        if (compilerStderr)
            *compilerStderr = QStringLiteral("list-components timeout.");
        qWarning() << "[DSLCompilerInterface] listComponents timeout.";
        return false;
    }

    const QString stdOut =
            TextEncoding::decodeUtf8WithLocalFallback(proc.readAllStandardOutput());
    const QString stdErr =
            TextEncoding::decodeUtf8WithLocalFallback(proc.readAllStandardError());

    if (compilerStdout)
        *compilerStdout = stdOut;
    if (compilerStderr)
        *compilerStderr = stdErr;

    const bool ok = proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;

    if (!ok) {
        qWarning() << "[DSLCompilerInterface] listComponents failed. exitCode="
                   << proc.exitCode();
    }

    return ok;
}

QString DSLCompilerInterface::describeComponents(QString* compilerStderr)
{
    const QString python = resolvePythonInterpreter();
    if (python.isEmpty()) {
        if (compilerStderr)
            *compilerStderr = QStringLiteral(
                    "No suitable Python interpreter found for DSL compile. "
                    "Recreate third_party/custom_dsp_language/compile/venv "
                    "and install requirements.txt, or install Python 3 plus antlr4-python3-runtime in PATH.");
        return QString();
    }

    const QString workDir = compilerWorkingDir();
    const QString entryScript = compilerEntryScript();
    if (!QFileInfo::exists(entryScript)) {
        const QString msg =
                QStringLiteral("Compiler entry script not found: %1").arg(entryScript);
        qWarning() << "[DSLCompilerInterface]" << msg;
        if (compilerStderr)
            *compilerStderr = msg;
        return QString();
    }

    QStringList args;
    args << QStringLiteral("-c")
         << QStringLiteral(
               "import json, sys; "
               "from pathlib import Path; "
               "sys.path.insert(0, str(Path('.').resolve() / 'src')); "
               "from lh_compiler.function_blocks.registry import FunctionBlockRegistry; "
               "registry = FunctionBlockRegistry(); registry.load_defaults(); "
               "param_to_dict = lambda param: {'name': param.name, 'dataType': param.data_type, "
               "'direction': param.direction, 'offset': param.offset, "
               "'defaultValue': param.default_value, 'description': param.description}; "
               "block_to_dict = lambda block: {'name': block.name, 'category': block.category, "
               "'description': block.description, 'typeId': block.type_id, "
               "'memorySize': block.memory_size, "
               "'parameters': [param_to_dict(param) for param in block.parameters]}; "
               "blocks = [block_to_dict(block) for block in registry.all_blocks().values()]; "
               "blocks.sort(key=lambda block: block['name']); "
               "print(json.dumps({'components': blocks}, ensure_ascii=False))");

    QProcess proc;
    proc.setProgram(python);
    proc.setArguments(args);
    proc.setWorkingDirectory(workDir);

    qInfo() << "[DSLCompilerInterface] describeComponents:" << python << args
            << "cwd =" << workDir;

    proc.start();
    if (!proc.waitForFinished(30 * 1000)) {
        proc.kill();
        if (compilerStderr)
            *compilerStderr = QStringLiteral("describe-components timeout.");
        qWarning() << "[DSLCompilerInterface] describeComponents timeout.";
        return QString();
    }

    const QString stdOut =
            TextEncoding::decodeUtf8WithLocalFallback(proc.readAllStandardOutput());
    const QString stdErr =
            TextEncoding::decodeUtf8WithLocalFallback(proc.readAllStandardError());

    if (compilerStderr)
        *compilerStderr = stdErr;

    const bool ok = proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;

    if (!ok) {
        qWarning() << "[DSLCompilerInterface] describeComponents failed. exitCode="
                   << proc.exitCode();
        return QString();
    }

    return stdOut.trimmed();
}
