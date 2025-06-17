#include <windows.h>

#include "Dialog.h"
#include "DllMain.h"
#include "Plugin.h"
#include "meta.h"

BOOL APIENTRY DllMain(const HMODULE hModule, const DWORD ulReasonForCall, LPVOID lpReserved) {
    if (ulReasonForCall == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

DLLEXPORT void WINAPI MargretePluginGetInfo(MP_PLUGININFO *info) {
    if (!info) {
        return;
    }

    info->sdkVersion = MP_SDK_VERSION;
    if (info->nameBuffer) {
        wcsncpy_s(info->nameBuffer, info->nameBufferLength, W_EN_TITLE, info->nameBufferLength);
    }
    if (info->descBuffer) {
        wcsncpy_s(info->descBuffer, info->descBufferLength, W_EN_DESC, info->descBufferLength);
    }
    if (info->developerBuffer) {
        wcsncpy_s(info->developerBuffer, info->developerBufferLength, W_DEVELOPER, info->developerBufferLength);
    }
}

DLLEXPORT MpBoolean WINAPI MargretePluginCommandCreate(IMargretePluginCommand **ppobj) {
    *ppobj = new Plugin();
    (*ppobj)->addRef();
    return MP_TRUE;
}
