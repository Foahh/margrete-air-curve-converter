#pragma once
#include <MargretePlugin.h>
#include <shared_mutex>
#include <windows.h>

template<typename T>
class MgComPtr : public MargreteComPtr<T> {
    using Base = MargreteComPtr<T>;

public:
    using Base::get;
    explicit operator bool() const { return this->get() != nullptr; }
};

class MargreteHandle {
public:
    MargreteHandle(const MargreteHandle &) = delete;
    MargreteHandle &operator=(const MargreteHandle &) = delete;
    explicit MargreteHandle(IMargretePluginContext *ctx = nullptr);

    MpBoolean CanCommit() const;
    MgComPtr<IMargretePluginChart> GetChart() const;
    MpInteger GetTickOffset() const;

    void BeginRecording() const;
    void CommitRecording() const;
    void DiscardRecording() const;
    HWND GetHWND() const;

private:
    mutable std::shared_mutex m_mutex;
    IMargretePluginContext *m_ctx{nullptr};
    MgComPtr<IMargretePluginDocument> m_doc{nullptr};
    MgComPtr<IMargretePluginUndoBuffer> m_undo{nullptr};
};
