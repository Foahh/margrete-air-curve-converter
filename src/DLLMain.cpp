#include <Windows.h>
#include <memory>
#include <mutex>

#include "Dialog.h"
#include "DllMain.h"
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
    *ppobj = new PluginCommand();
    (*ppobj)->addRef();
    return MP_TRUE;
}

MpBoolean PluginCommand::getCommandName(wchar_t *text, const MpInteger textLength) const {
    wcsncpy_s(text, textLength, W_EN_TITLE, textLength);
    return MP_TRUE;
}

MpBoolean PluginCommand::invoke(IMargretePluginContext *p_ctx) {
    static Config g_cctx;
    static std::atomic_flag g_flag = ATOMIC_FLAG_INIT;

    if (g_flag.test_and_set(std::memory_order_acquire)) {
        return MP_FALSE;
    }

    p_ctx->addRef();
    std::jthread([p_ctx] {
        try {
            Dialog dlg(g_cctx, p_ctx);
            dlg.ShowDialog();
        } catch (const std::exception &e) {
            const std::wstring msg{e.what(), e.what() + std::strlen(e.what())};
            MessageBox(nullptr, msg.c_str(), L"Error", MB_ICONERROR | MB_OK);
        } catch (...) { MessageBox(nullptr, L"An unknown error occurred", L"Error", MB_ICONERROR | MB_OK); }
        p_ctx->release();
        g_flag.clear(std::memory_order_release);
    }).detach();

    return MP_TRUE;
}
