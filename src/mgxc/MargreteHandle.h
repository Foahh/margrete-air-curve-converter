#pragma once
#include <MargretePlugin.h>
#include <shared_mutex>
#include <windows.h>

/**
 * @brief Smart pointer wrapper for Margrete plugin COM interfaces.
 * @tparam T COM interface type.
 */
template<typename T>
class MgComPtr : public MargreteComPtr<T> {
    using Base = MargreteComPtr<T>;

public:
    using Base::get;
    explicit operator bool() const { return this->get() != nullptr; }
};

/**
 * @class MargreteHandle
 * @brief Manages plugin context, document, undo buffer, and chart access.
 */
class MargreteHandle {
public:
    MargreteHandle(const MargreteHandle &) = delete;
    MargreteHandle &operator=(const MargreteHandle &) = delete;
    /**
     * @brief Constructs a MargreteHandle with an optional plugin context.
     * @param ctx Pointer to the plugin context.
     */
    explicit MargreteHandle(IMargretePluginContext *ctx = nullptr);

    /**
     * @brief Checks if commit operations are possible.
     * @return True if commit is possible.
     */
    MpBoolean CanCommit() const;
    /**
     * @brief Gets the current chart from the plugin document.
     * @return Smart pointer to the chart.
     */
    MgComPtr<IMargretePluginChart> GetChart() const;
    /**
     * @brief Gets the current tick offset from the plugin context.
     * @return The current tick offset.
     */
    MpInteger GetTickOffset() const;

    /**
     * @brief Begins a recording session for undo/redo.
     */
    void BeginRecording() const;
    /**
     * @brief Commits the current recording session.
     */
    void CommitRecording() const;
    /**
     * @brief Discards the current recording session.
     */
    void DiscardRecording() const;
    /**
     * @brief Gets the window handle (HWND) for the plugin UI.
     * @return The HWND value.
     */
    HWND GetHWND() const;

private:
    mutable std::shared_mutex m_mutex; /**< Mutex for thread-safe access. */
    IMargretePluginContext *m_ctx{nullptr}; /**< Plugin context pointer. */
    MgComPtr<IMargretePluginDocument> m_doc{nullptr}; /**< Document pointer. */
    MgComPtr<IMargretePluginUndoBuffer> m_undo{nullptr}; /**< Undo buffer pointer. */
};
