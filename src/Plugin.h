#pragma once

#include <atomic>
#include <thread>

#include "Config.h"
#include "MargretePlugin.h"

/**
 * @class Plugin
 * @brief Main plugin class for the Margrete Air Curve Converter.
 *
 * Implements the IMargretePluginCommand interface, manages reference counting, threading, and configuration context.
 */
class Plugin final : public IMargretePluginCommand {
public:
    /**
     * @brief Increments the reference count.
     * @return The new reference count.
     */
    MpInteger addRef() noexcept override;
    /**
     * @brief Decrements the reference count and deletes the object if it reaches zero.
     * @return The new reference count.
     */
    MpInteger release() noexcept override;

    /**
     * @brief Queries for a supported interface.
     * @param iid Interface ID.
     * @param ppobj Pointer to the interface pointer to receive the result.
     * @return True if the interface is supported, false otherwise.
     */
    MpBoolean queryInterface(const MpGuid &iid, void **ppobj) override;
    /**
     * @brief Gets the command name for the plugin.
     * @param text Buffer to receive the command name.
     * @param textLength Length of the buffer.
     * @return True on success, false otherwise.
     */
    MpBoolean getCommandName(wchar_t *text, MpInteger textLength) const override;
    /**
     * @brief Invokes the plugin command.
     * @param p_ctx Plugin context.
     * @return True on success, false otherwise.
     */
    MpBoolean invoke(IMargretePluginContext *p_ctx) override;

private:
    /**
     * @brief Destructor (private, called by release()).
     */
    ~Plugin();
    /** Reference count for COM-style lifetime management. */
    std::atomic<MpInteger> m_refCount{0};
    /** Indicates if the plugin is running. */
    std::atomic_bool m_running{false};
    /** Worker thread for background processing. */
    std::jthread m_worker;
    /** Configuration context for the plugin. */
    Config m_cctx;
};
