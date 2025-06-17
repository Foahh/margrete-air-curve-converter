#include "Plugin.h"
#include <memory>
#include <meta.h>
#include <mutex>
#include <windows.h>

#include "Dialog.h"

MpInteger Plugin::addRef() noexcept { return ++m_refCount; }

MpInteger Plugin::release() noexcept {
    const auto i = --m_refCount;
    if (i == 0) {
        delete this;
    }
    return i;
}

MpBoolean Plugin::queryInterface(const MpGuid &iid, void **ppobj) {
    if (!ppobj) {
        return MP_FALSE;
    }

    if (iid == IID_IMargretePluginBase || iid == IID_IMargretePluginCommand) {
        *ppobj = this;
        addRef();
        return MP_TRUE;
    }
    return MP_FALSE;
}

MpBoolean Plugin::getCommandName(wchar_t *text, const MpInteger textLength) const {
    wcsncpy_s(text, textLength, W_EN_TITLE, textLength);
    return MP_TRUE;
}

MpBoolean Plugin::invoke(IMargretePluginContext *p_ctx) {
    if (m_running.exchange(true, std::memory_order_acquire)) {
        return MP_FALSE;
    }

    p_ctx->addRef();

    m_worker = std::jthread([this, p_ctx](const std::stop_token &token) {
        try {
            Dialog dlg(m_cctx, p_ctx, token);
            dlg.ShowDialog();
        } catch (const std::exception &e) {
            std::wstring wmsg(e.what(), e.what() + std::strlen(e.what()));
            MessageBoxW(nullptr, wmsg.c_str(), L"Error", MB_ICONERROR | MB_OK);
        } catch (...) { MessageBoxW(nullptr, L"Unknown error", L"Error", MB_ICONERROR | MB_OK); }

        p_ctx->release();
        m_running.store(false, std::memory_order_release);
    });

    return MP_TRUE;
}

Plugin::~Plugin() {
    if (m_worker.joinable()) {
        m_worker.request_stop();
        m_worker.join();
    }
    release();
}
