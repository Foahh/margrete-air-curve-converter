#pragma once
#include <MargretePlugin.h>
#include <vector>

#include "Config.h"
#include "MargreteHandle.h"

class Interpolator {
public:
    explicit Interpolator(Config &cctx);

    void Convert(int idx = -1);
    void CommitChart(const MargreteHandle &mg) const;

    std::vector<std::vector<MP_NOTEINFO>> m_notes;

private:
    Config &m_cctx;
    void FinalizeSingle(std::vector<MP_NOTEINFO> &chain) const;

    static void Clamp(MP_NOTEINFO &note);
    static void CommitChain(const MargreteComPtr<IMargretePluginChart> &p_chart, const std::vector<MP_NOTEINFO> &chain);
    static MargreteComPtr<IMargretePluginNote> CreateNote(const MargreteComPtr<IMargretePluginChart> &p_chart);
    void InterpolateChain(size_t idx);
};
