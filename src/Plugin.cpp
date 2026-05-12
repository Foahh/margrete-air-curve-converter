#include "Plugin.h"

#include <cstring>
#include <meta.h>
#include <stdexcept>
#include <windows.h>

#include "Dialog.h"

MpInteger Plugin::addRef() noexcept { return ++m_refCount; }

MpInteger Plugin::release() noexcept {
    const int i = --m_refCount;
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
    if (p_ctx == nullptr) {
        return MP_FALSE;
    }

    if (m_running.exchange(true, std::memory_order_acq_rel)) {
        return MP_FALSE;
    }

    p_ctx->addRef();

    m_worker = std::jthread([this, p_ctx](const std::stop_token &token) {
        try {
            Dialog dialog(m_cctx, p_ctx, token);
            if (FAILED(dialog.ShowDialog())) {
                throw std::runtime_error("Failed to show plugin dialog...");
            }
        } catch (const std::exception &e) {
            const std::wstring wmsg(e.what(), e.what() + std::strlen(e.what()));
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
}
