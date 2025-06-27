#pragma once
#include <MargretePlugin.h>
#include <vector>

#include "Config.h"
#include "MargreteHandle.h"

struct Joint {
    int x{}, y{}, t{};
    EasingMode eX{EasingMode::Linear};
    EasingMode eY{EasingMode::Linear};
};

class Interpolator {
public:
    explicit Interpolator(Config &cctx);

    void Convert();
    void Convert(size_t idx);

    void CommitChart(const MargreteHandle &mg) const;

private:
    std::vector<std::vector<MP_NOTEINFO>> m_chains;
    std::vector<MP_NOTEINFO> m_chain;

    Config &m_cctx;

    static void CommitChain(const MargreteComPtr<IMargretePluginChart> &p_chart, const std::vector<MP_NOTEINFO> &chain);
    static MargreteComPtr<IMargretePluginNote> CreateNote(const MargreteComPtr<IMargretePluginChart> &p_chart);

    void DebugPrint() const;

    void InterpolateChain(size_t idx);
    static void Clamp(MP_NOTEINFO &note);

    Joint Snap(const mgxc::Note &src) const;
    void Push(const MP_NOTEINFO &base, const Joint &curr, const Joint &next);
    void VerticalSegment(MP_NOTEINFO base, const Joint &curr, const Joint &next);
    void HorizontalSegment(MP_NOTEINFO base, const Joint &curr, const Joint &next);
};
