#define NOMINMAX

#include <algorithm>
#include <format>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

#include "Interpolator.h"
#include "MargreteHandle.h"
#include "Primitive.h"
#include "Utils.h"

Interpolator::Interpolator(Config &cctx) : m_cctx(cctx) {}

void Interpolator::ResetOutput() {
    m_noteChains.clear();
    m_noteChain.clear();
}

void Interpolator::PushPoint(const mgxc::Chain &chain, const mgxc::CurvePoint &point) {
    MP_NOTEINFO note{};
    note.type = chain.type;
    note.longAttr = MP_NOTELONGATTR_CONTROL;
    note.direction = MP_NOTEDIR_UP;
    note.exAttr = MP_NOTEEXATTR_NONE;
    note.x = point.x;
    note.width = chain.width;
    note.height = point.y;
    if (point.t < std::numeric_limits<MpInteger>::min() || point.t > std::numeric_limits<MpInteger>::max()) {
        throw std::out_of_range("Snapped curve tick is outside the supported integer range");
    }
    note.tick = static_cast<MpInteger>(point.t);
    note.timelineId = chain.til;
    note.optionValue = MP_OPTIONVALUE_AIRCRUSH_TRACELIKE;
    if (!m_noteChain.empty() && m_noteChain.back().tick == note.tick) {
        m_noteChain.back() = note;
    } else {
        m_noteChain.push_back(note);
    }
}

void Interpolator::InterpolateChain(std::size_t idx) {
    m_noteChain.clear();

    if (idx >= m_cctx.chains.size()) {
        throw std::out_of_range(std::format("Invalid chain index: {}", idx));
    }

    const mgxc::Chain &chain = m_cctx.chains[idx];
    if (chain.size() < 2) {
        throw std::invalid_argument(std::format("Chain [{}] must have at least 2 notes", idx));
    }

    for (std::size_t i = 0; i < chain.size() - 1; ++i) {
        mgxc::Joint curr = chain[i].Snap(m_cctx.snap);
        mgxc::Joint next = chain[i + 1].Snap(m_cctx.snap);

        if (curr.t >= next.t) {
            throw std::invalid_argument(
                    std::format("Invalid Chain [{}]: note [{}] (t={}) must be before note [{}] (t={})", idx, i, curr.t,
                                i + 1, next.t));
        }

        const mgxc::CurvePoint first{static_cast<std::int64_t>(curr.t) * m_cctx.snap, curr.x, curr.y};
        const mgxc::CurvePoint last{static_cast<std::int64_t>(next.t) * m_cctx.snap, next.x, next.y};
        const auto points = mgxc::FitCurve(
                first, last, m_cctx.snap, [&](double t) { return chain.es.Solve(t, curr.eX); },
                [&](double t) { return chain.es.Solve(t, curr.eY); });
        for (const auto &point: points)
            PushPoint(chain, point);
    }

    FinalizeChain();
    m_noteChains.push_back(std::move(m_noteChain));
}

void Interpolator::FinalizeChain() {
    for (MP_NOTEINFO &note: m_noteChain) {
        note.tick += m_cctx.tOffset;
        note.x += m_cctx.xOffset;
        note.height += m_cctx.yOffset;
        if (m_cctx.clamp) {
            Clamp(note);
        }
    }

    m_noteChain.front().longAttr = MP_NOTELONGATTR_BEGIN;
    m_noteChain.back().longAttr = MP_NOTELONGATTR_END;
}

void Print(const std::vector<std::vector<MP_NOTEINFO>> &chains) {
    std::cout << std::endl << "Interpolated " << chains.size() << std::endl;
    int i = 0;
    for (const std::vector<MP_NOTEINFO> &chain: chains) {
        std::cout << i++ << ":" << std::endl;
        for (const MP_NOTEINFO &note: chain) {
            std::cout << "  a=" << note.longAttr << ", t=" << note.tick << ", x=" << note.x << ", h=" << note.height
                      << std::endl;
        }
    }
}


void Interpolator::Convert(const int idx) {
    ResetOutput();
    if (m_cctx.snap <= 0)
        throw std::invalid_argument("Snap must be positive");

    if (idx < 0) {
        for (size_t i = 0; i < m_cctx.chains.size(); ++i) {
            InterpolateChain(i);
        }
    } else if (idx < m_cctx.chains.size()) {
        InterpolateChain(idx);
    }

    Print(m_noteChains);
}

void Interpolator::Clamp(MP_NOTEINFO &note) {
    note.height = std::clamp(note.height, 0, 360);
    note.x = std::clamp(note.x, 0, 15);
    note.width = std::max(1, std::min(note.width, 16 - note.x));
}

void Interpolator::Commit(const MargreteHandle &mg) const {
    if (m_noteChains.empty()) {
        return;
    }

    try {
        mg.BeginRecording();
        const MgComPtr<IMargretePluginChart> chart = mg.GetChart();
        for (const std::vector<MP_NOTEINFO> &chain: m_noteChains) {
            CommitChain(chart, chain);
        }
        mg.CommitRecording();
    } catch (...) {
        mg.DiscardRecording();
        throw;
    }
}

void Interpolator::CommitChain(const MargreteComPtr<IMargretePluginChart> &p_chart,
                               const std::vector<MP_NOTEINFO> &chain) {
    const MP_NOTEINFO &airHead = chain.front();

    const MargreteComPtr<IMargretePluginNote> longBegin = CreateNote(p_chart);
    longBegin->setInfo(&airHead);
    for (size_t i = 1; i < chain.size(); ++i) {
        MargreteComPtr<IMargretePluginNote> airControl = CreateNote(p_chart);
        airControl->setInfo(&chain[i]);
        longBegin->appendChild(airControl.get());
    }

    if (airHead.type == MP_NOTETYPE_AIRSLIDE) {
        MP_NOTEINFO info = airHead;
        info.type = MP_NOTETYPE_TAP;
        info.longAttr = MP_NOTELONGATTR_NONE;
        info.direction = MP_NOTEDIR_NONE;

        const MargreteComPtr<IMargretePluginNote> tap = CreateNote(p_chart);
        tap->setInfo(&info);

        info.type = MP_NOTETYPE_AIR;
        info.direction = MP_NOTEDIR_UP;

        const MargreteComPtr<IMargretePluginNote> air = CreateNote(p_chart);
        air->setInfo(&info);

        air->appendChild(longBegin.get());
        tap->appendChild(air.get());

        p_chart->appendNote(tap.get());
    } else {
        p_chart->appendNote(longBegin.get());
    }
}

inline MargreteComPtr<IMargretePluginNote> Interpolator::CreateNote(
        const MargreteComPtr<IMargretePluginChart> &p_chart) {
    MargreteComPtr<IMargretePluginNote> note;
    if (!p_chart->createNote(note.put())) {
        throw std::runtime_error("Failed to create IMargretePluginNote from IMargretePluginChart");
    }
    return note;
}
