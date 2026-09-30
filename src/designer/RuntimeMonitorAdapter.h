#pragma once
#include "../runtime/RuntimeSessionService.h"
namespace Monitor { class MonitorManager; }
std::shared_ptr<RuntimeMonitorPort> makeRuntimeMonitorPort(Monitor::MonitorManager& manager);
