#pragma once
#include <MargretePlugin.h>
#include <shared_mutex>
#include <windows.h>

template<typename T>
class MyComPtr : public MargreteComPtr<T> {
    using Base = MargreteComPtr<T>;

public:
    using Base::get;

    explicit MyComPtr(T *p = nullptr) : Base(p) {
        if (p) {
            p->addRef();
        }
    }

    explicit operator bool() const { return this->get() != nullptr; }
};

class Margrete {
public:
    Margrete(const Margrete &) = delete;
    Margrete &operator=(const Margrete &) = delete;
    explicit Margrete(IMargretePluginContext *ctx = nullptr);

    MpBoolean CanCommit() const;
    MyComPtr<IMargretePluginChart> GetChart() const;
    MpInteger GetTickOffset() const;

    void BeginRecording() const;
    void CommitRecording() const;
    void DiscardRecording() const;
    HWND GetHWND() const;

private:
    mutable std::shared_mutex m_mutex;
    IMargretePluginContext *m_ctx{nullptr};
    MyComPtr<IMargretePluginDocument> m_doc{nullptr};
    MyComPtr<IMargretePluginUndoBuffer> m_undo{nullptr};
};
