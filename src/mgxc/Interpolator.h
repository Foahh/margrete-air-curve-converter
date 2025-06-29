#pragma once
#include <MargretePlugin.h>
#include <vector>

#include "Config.h"
#include "MargreteHandle.h"
#include "Primitive.h"

class Interpolator {
public:
    explicit Interpolator(Config &cctx);

    void Convert(int idx = -1);
    void Commit(const MargreteHandle &mg) const;

private:
    Config &m_cctx;

    std::vector<std::vector<MP_NOTEINFO>> m_noteChains;
    std::vector<MP_NOTEINFO> m_noteChain;

    static void CommitChain(const MargreteComPtr<IMargretePluginChart> &p_chart, const std::vector<MP_NOTEINFO> &chain);
    static MargreteComPtr<IMargretePluginNote> CreateNote(const MargreteComPtr<IMargretePluginChart> &p_chart);

    void InterpolateChain(size_t idx);
    static void Clamp(MP_NOTEINFO &note);

    void PushSegment(const mgxc::Chain &chain, const mgxc::Joint &curr, const mgxc::Joint &next,
                     const mgxc::Joint &base);
    void VerticalSegment(const mgxc::Chain &chain, const mgxc::Joint &curr, const mgxc::Joint &next);
    void HorizontalSegment(const mgxc::Chain &chain, const mgxc::Joint &curr, const mgxc::Joint &next);
};
