#pragma once

#include "MargretePlugin.h"

#define DLLEXPORT extern "C" __declspec(dllexport)

DLLEXPORT void WINAPI MargretePluginGetInfo(MP_PLUGININFO *info);
DLLEXPORT MpBoolean WINAPI MargretePluginCommandCreate(IMargretePluginCommand **ppobj);
