#ifndef LH_PROJECT_CONFIG_VALIDATION_H
#define LH_PROJECT_CONFIG_VALIDATION_H
#include "common/ConfigTypes.h"
#include <QJsonObject>
#include <QStringList>
namespace ProjectConfigValidation {
void appendUniqueError(QStringList& errors, const QString& error);
void appendIntegerRangeError(QStringList& errors, const QString& entity,
                             const QString& field, const QString& value,
                             int minimum, int maximum);
bool validateProjectConfigShape(const QJsonObject& object, QStringList& errors);
void validateParameterRanges(const ProjectRuntimeConfig& cfg, QStringList& errors);
void validateOpcServerConfiguration(const ProjectRuntimeConfig& cfg, QStringList& errors);
void validateTransportConfiguration(const ProjectRuntimeConfig& cfg, QStringList& errors);
}
#endif
