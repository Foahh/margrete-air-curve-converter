#pragma once

#include <atomic>
#include <thread>

#include "Config.h"
#include "MargretePlugin.h"

class Plugin final : public IMargretePluginCommand {
public:
    MpInteger addRef() noexcept override;
    MpInteger release() noexcept override;

    MpBoolean queryInterface(const MpGuid &iid, void **ppobj) override;
    MpBoolean getCommandName(wchar_t *text, MpInteger textLength) const override;
    MpBoolean invoke(IMargretePluginContext *p_ctx) override;

private:
    ~Plugin();
    std::atomic<MpInteger> m_refCount{0};
    std::atomic_bool m_running{false};
    std::jthread m_worker;

    Config m_cctx;
};
