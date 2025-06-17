#include "Margrete.h"

#include <MargretePlugin.h>
#include <stdexcept>

#include "Dialog.h"

Margrete::Margrete(IMargretePluginContext *ctx) {
    if (m_ctx == ctx) {
        return;
    }
    m_ctx = ctx;

    if (!ctx) {
        m_doc.reset();
        m_undo.reset();
        return;
    }

    auto result = m_ctx->getDocument(m_doc.put());
    if (result != MP_TRUE) {
        m_doc.reset();
    }

    result = m_doc->getUndoBuffer(m_undo.put());
    if (result != MP_TRUE) {
        m_undo.reset();
    }
}

MpBoolean Margrete::CanCommit() const { return m_ctx && m_doc && m_undo; }

MgComPtr<IMargretePluginChart> Margrete::GetChart() const {
    if (!m_ctx) {
        throw std::runtime_error("IMargretePluginContext is not initialized");
    }

    if (!m_doc) {
        throw std::runtime_error("IMargretePluginDocument is not initialized");
    }

    MgComPtr<IMargretePluginChart> chart;
    const auto result = m_doc->getChart(chart.put());
    if (result != MP_TRUE) {
        throw std::runtime_error("Failed to get IMargretePluginChart from IMargretePluginDocument");
    }
    return chart;
}

MpInteger Margrete::GetTickOffset() const {
    if (!m_ctx) {
        return 0;
    }
    return m_ctx->getCurrentTick();
}

void Margrete::BeginRecording() const {
    if (m_undo) {
        m_undo->beginRecording();
    }
}

void Margrete::CommitRecording() const {
    if (m_undo) {
        m_undo->commitRecording();
    }
    if (m_ctx) {
        m_ctx->update();
    }
}

void Margrete::DiscardRecording() const {
    if (m_undo) {
        m_undo->discardRecording();
    }
    if (m_ctx) {
        m_ctx->update();
    }
}

HWND Margrete::GetHWND() const {
    if (!m_ctx) {
        return nullptr;
    }
    return static_cast<HWND>(m_ctx->getMainWindowHandle());
}
