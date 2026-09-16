#pragma once

#include "ProjectController.h"

#include <QVariantMap>

// Resolves the same project/manifest Profile source used by formal downloads.
// The returned map contains downloadProfilePath when a source is configured and
// profileOverrideConflict/profileOverrideError when an override is unsafe.
QVariantMap resolveDownloadProfileOptions(const QVariantMap& options,
                                          const ProjectRuntimeConfig& config,
                                          const QString& projectPath);
