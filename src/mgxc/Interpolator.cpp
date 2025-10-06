#define NOMINMAX

#include <algorithm>
#include <format>
#include <iostream>
#include <utility>
#include <vector>

#include "Interpolator.h"
#include "MargreteHandle.h"
#include "Primitive.h"
#include "Utils.h"

Interpolator::Interpolator(Config &cctx) : m_cctx(cctx) {}

void Interpolator::PushSegment(const mgxc::Chain &chain, const mgxc::Joint &curr, const mgxc::Joint &next,
                               const mgxc::Joint &base) {

    MP_NOTEINFO note{};
    note.type = chain.type;
    note.longAttr = MP_NOTELONGATTR_CONTROL;
    note.direction = MP_NOTEDIR_UP;
    note.exAttr = MP_NOTEEXATTR_NONE;
    note.variationId = 0;
    note.x = base.x;
    note.width = chain.width;
    note.height = base.y;
    note.tick = base.t;
    note.timelineId = chain.til;
    note.optionValue = MP_OPTIONVALUE_AIRCRUSH_TRACELIKE;

    if (m_noteChain.empty() || m_noteChain.back().tick != base.t) {
        m_noteChain.push_back(note);
        return;
    }

    MP_NOTEINFO &last = m_noteChain.back();
    if (last.x == base.x && last.height == base.y && last.tick == base.t) {
        return;
    }

    const double dT = next.t - curr.t;
    const double dX = next.x - curr.x;
    const double dY = next.y - curr.y;

    const double pT = (base.t - curr.t) / dT;
    const double fPTx = chain.es.Solve(pT, curr.eX);
    const double idealX = curr.x + fPTx * dX;
    double errLast = std::abs(idealX - last.x);
    double errNew = std::abs(idealX - base.x);

    if (dY != 0) {
        const double fPTy = chain.es.Solve(pT, curr.eY);
        const double idealY = curr.y + fPTy * dY;
        errLast = std::hypot(errLast, std::abs(idealY - last.height));
        errNew = std::hypot(errNew, std::abs(idealY - base.y));
    }

    if (errNew < errLast) {
        last = note;
    }
}

void Interpolator::VerticalSegment(const mgxc::Chain &chain, const mgxc::Joint &curr, const mgxc::Joint &next) {
    const double dT = next.t - curr.t;
    const double dY = next.y - curr.y;
    const int sY = utils::step(dY);

    mgxc::Joint base = curr;
    for (int y = curr.y; sY > 0 ? y <= next.y : y >= next.y; y += sY) {
        const double pY = (y - curr.y) / dY;
        const double fPY = chain.es.InverseSolve(pY, curr.eY);

        base.t = utils::iround(curr.t + fPY * dT);
        base.x = curr.x;
        base.y = y;

        PushSegment(chain, curr, next, base);
    }
}

void Interpolator::HorizontalSegment(const mgxc::Chain &chain, const mgxc::Joint &curr, const mgxc::Joint &next) {
    const double dT = next.t - curr.t;
    const double dX = next.x - curr.x;
    const double dY = next.y - curr.y;
    const int sX = utils::step(dX);

    mgxc::Joint base = curr;
    for (int x = curr.x; sX > 0 ? x <= next.x : x >= next.x; x += sX) {
        const double pX = (x - curr.x) / dX;
        const double fPX = chain.es.InverseSolve(pX, curr.eX);

        base.t = utils::iround(curr.t + fPX * dT);
        base.x = x;

        if (dY != 0) {
            const double pT = (base.t - curr.t) / dT;
            const double fPT = chain.es.Solve(pT, curr.eY);
            base.y = utils::iround(curr.y + fPT * dY);
        }

        PushSegment(chain, curr, next, base);
    }
}

void Interpolator::InterpolateChain(std::size_t idx) {
    m_noteChain = {};

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

        const bool trivX = curr.eX == EasingMode::Linear;
        const bool trivY = curr.eY == EasingMode::Linear;
        const bool sameX = curr.x == next.x;
        const bool sameY = curr.y == next.y;

        if ((sameX || trivX) && (sameY || trivY)) {
            PushSegment(chain, curr, next, curr);
        } else if (sameX) {
            VerticalSegment(chain, curr, next);
        } else {
            HorizontalSegment(chain, curr, next);
        }

        PushSegment(chain, curr, next, next);
    }

    for (MP_NOTEINFO &note: m_noteChain) {
        note.tick *= m_cctx.snap;
        note.tick += m_cctx.tOffset;
        note.x += m_cctx.xOffset;
        note.height += m_cctx.yOffset;
        if (m_cctx.clamp) {
            Clamp(note);
        }
    }

    m_noteChain.front().longAttr = MP_NOTELONGATTR_BEGIN;
    m_noteChain.back().longAttr = MP_NOTELONGATTR_END;

    m_noteChains.push_back(std::move(m_noteChain));
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
    m_noteChains.clear();

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
