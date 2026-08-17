#ifndef DSLCOMPILERINTERNAL_H
#define DSLCOMPILERINTERNAL_H

#include <QString>
#include <QStringList>

#include "common/ConfigTypes.h"

namespace DSLCompilerInternal {

QString defaultOutputFileForSource(const QString& sourceFile, const QString& outputDir);
QStringList buildCompilerProcessArgs(const QString& entryScript,
                                     const QString& compilerInputFile,
                                     const QString& outputFile);
bool prependCodeMetadataHeader(const QString& outputFile,
                               const QString& sourceFile,
                               const QString& projectName,
                               const QString& compilerInputFile,
                               QString* errorMessage);
QString sha256ForFile(const QString& filePath);
QString readTextFile(const QString& filePath, QString* errorMessage);
bool writeTextFile(const QString& filePath, const QString& text, QString* errorMessage);
QString compilerStagingDir(const QString& outputDir);
QString runDslCompilerProcess(const QString& python,
                              const QString& entryScript,
                              const QString& compilerInputFile,
                              const QString& outputFile,
                              const QString& workDir,
                              QString* compilerStdout,
                              QString* compilerStderr);
QString resolveScriptPath(const QString& projectPath, const QString& path);
QString resolveProjectMainScriptPath(const QString& projectPath,
                                     const ProjectRuntimeConfig& config);
QStringList normalizeProjectScriptFiles(const QString& projectPath,
                                        const ProjectRuntimeConfig& config,
                                        const QString& mainScriptFile);
QString assembleProjectCompilerInput(const QString& projectPath,
                                     const QString& outputDir,
                                     const QString& mainScriptFile,
                                     const QStringList& scriptFiles,
                                     QString* errorMessage);

} // namespace DSLCompilerInternal

#endif // DSLCOMPILERINTERNAL_H
