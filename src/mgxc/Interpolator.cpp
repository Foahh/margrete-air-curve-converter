#define NOMINMAX

#include <algorithm>
#include <utility>
#include <vector>

#include "Interpolator.h"

#include <format>
#include <iostream>

#include "MargreteHandle.h"

Interpolator::Interpolator(Config &cctx) : m_cctx(cctx) {}

static int step(const int v) { return (v > 0) - (v < 0); }
static int iround(const double v) { return static_cast<int>(std::round(v)); }

Joint Interpolator::Snap(const mgxc::Note &src) const {
    Joint k;
    k.x = src.x;
    k.y = src.y;
    k.t = iround(src.t / m_cctx.snap);
    k.eX = src.eX;
    k.eY = src.eY;
    return k;
}

void Interpolator::Push(const MP_NOTEINFO &base, const Joint &curr, const Joint &next) {
    if (m_chain.empty() || m_chain.back().tick != base.tick) {
        m_chain.push_back(base);
        return;
    }

    auto &last = m_chain.back();
    if (last.x == base.x && last.height == base.height && last.tick == base.tick) {
        return;
    }

    const double dT = next.t - curr.t;
    const double dX = next.x - curr.x;
    const double dY = next.y - curr.y;

    const double pT = static_cast<double>(base.tick - curr.t) / dT;
    const double fPTx = m_cctx.solver.Solve(pT, curr.eX);
    const double idealX = curr.x + fPTx * dX;
    double errLast = std::abs(idealX - last.x);
    double errNew = std::abs(idealX - base.x);

    if (dY != 0) {
        const double fPTy = m_cctx.solver.Solve(pT, curr.eY);
        const double idealY = curr.y + fPTy * dY;
        errLast = std::hypot(errLast, std::abs(idealY - last.height));
        errNew = std::hypot(errNew, std::abs(idealY - base.height));
    }

    if (errNew < errLast) {
        last = base;
    }
}

void Interpolator::VerticalSegment(MP_NOTEINFO base, const Joint &curr, const Joint &next) {
    const double dT = next.t - curr.t;
    const double dY = next.y - curr.y;
    const int sY = step(dY);

    for (int y = curr.y; sY > 0 ? y <= next.y : y >= next.y; y += sY) {
        const double pY = (y - curr.y) / dY;
        const double fPY = m_cctx.solver.InverseSolve(pY, curr.eY);

        base.tick = iround(curr.t + fPY * dT);
        base.x = curr.x;
        base.height = y;

        Push(base, curr, next);
    }
}

void Interpolator::HorizontalSegment(MP_NOTEINFO base, const Joint &curr, const Joint &next) {
    const double dT = next.t - curr.t;
    const double dX = next.x - curr.x;
    const double dY = next.y - curr.y;
    const int sX = step(dX);

    for (int x = curr.x; sX > 0 ? x <= next.x : x >= next.x; x += sX) {
        const double pX = (x - curr.x) / dX;
        const double fPX = m_cctx.solver.InverseSolve(pX, curr.eX);

        base.tick = iround(curr.t + fPX * dT);
        base.x = x;

        if (dY != 0) {
            const double pT = (base.tick - curr.t) / dT;
            const double fPT = m_cctx.solver.Solve(pT, curr.eY);
            base.height = iround(curr.y + fPT * dY);
        }

        Push(base, curr, next);
    }
}


void Interpolator::InterpolateChain(std::size_t idx) {
    m_chain = {};

    if (idx >= m_cctx.chains.size()) {
        throw std::out_of_range(std::format("Invalid chain index: {}", idx));
    }

    const auto &chain = m_cctx.chains[idx];
    if (chain.size() < 2) {
        throw std::invalid_argument(std::format("Chain [{}] must have at least 2 notes", idx));
    }

    MP_NOTEINFO n{};
    n.type = chain.front().type;
    n.longAttr = MP_NOTELONGATTR_CONTROL;
    n.width = chain.front().width;
    n.timelineId = chain.front().til;

    for (std::size_t i = 0; i < chain.size() - 1; ++i) {
        Joint curr = Snap(chain[i]);
        Joint next = Snap(chain[i + 1]);

        if (curr.t > next.t) {
            throw std::invalid_argument(
                    std::format("Invalid Chain [{}]: note [{}] (t={}) must be before note [{}] (t={})", idx, i, curr.t,
                                i + 1, next.t));
        }

        n.x = curr.x;
        n.tick = curr.t;
        n.height = curr.y;

        const bool trivX = curr.eX == EasingMode::Linear;
        const bool trivY = curr.eY == EasingMode::Linear;
        const bool sameX = curr.x == next.x;
        const bool sameY = curr.y == next.y;

        if ((sameX || trivX) && (sameY || trivY)) {
            Push(n, curr, next);
        } else if (sameX) {
            VerticalSegment(n, curr, next);
        } else {
            HorizontalSegment(n, curr, next);
        }

        n.x = next.x;
        n.tick = next.t;
        n.height = next.y;
        Push(n, curr, next);
    }

    for (auto &note: m_chain) {
        note.tick *= m_cctx.snap;
        note.tick += m_cctx.tOffset;
        note.x += m_cctx.xOffset;
        note.height += m_cctx.yOffset;
        if (m_cctx.clamp) {
            Clamp(note);
        }
    }

    m_chain.front().longAttr = MP_NOTELONGATTR_BEGIN;
    m_chain.back().longAttr = MP_NOTELONGATTR_END;

    m_chains.push_back(m_chain);
}

void Interpolator::DebugPrint() const {
    std::cout << std::endl << "Interpolated " << m_chains.size() << std::endl;
    int i = 0;
    for (const auto &chain: m_chains) {
        std::cout << i++ << ":" << std::endl;
        for (const auto &note: chain) {
            std::cout << "  a=" << note.longAttr << ", t=" << note.tick << ", x=" << note.x << ", h=" << note.height
                      << std::endl;
        }
    }
}

void Interpolator::Convert() {
    m_chains.clear();
    for (size_t i = 0; i < m_cctx.chains.size(); ++i) {
        InterpolateChain(i);
    }
    DebugPrint();
}

void Interpolator::Convert(const size_t idx) {
    m_chains.clear();
    if (idx < static_cast<int>(m_cctx.chains.size())) {
        InterpolateChain(idx);
    }
    DebugPrint();
}

void Interpolator::Clamp(MP_NOTEINFO &note) {
    note.height = std::clamp(note.height, 0, 360);
    note.x = std::clamp(note.x, 0, 15);
    note.width = std::max(1, std::min(note.width, 16 - note.x));
}

void Interpolator::CommitChart(const MargreteHandle &mg) const {
    if (m_chains.empty()) {
        return;
    }

    try {
        mg.BeginRecording();
        const auto chart = mg.GetChart();
        for (const auto &chain: m_chains) {
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
    const auto &airHead = chain.front();

    const auto longBegin = CreateNote(p_chart);
    longBegin->setInfo(&airHead);
    for (size_t i = 1; i < chain.size(); ++i) {
        auto airControl = CreateNote(p_chart);
        airControl->setInfo(&chain[i]);
        longBegin->appendChild(airControl.get());
    }

    if (airHead.type == MP_NOTETYPE_AIRSLIDE) {
        MP_NOTEINFO info = airHead;
        info.type = MP_NOTETYPE_TAP;
        info.longAttr = MP_NOTELONGATTR_NONE;
        info.direction = MP_NOTEDIR_NONE;

        const auto tap = CreateNote(p_chart);
        tap->setInfo(&info);

        info.type = MP_NOTETYPE_AIR;
        info.direction = MP_NOTEDIR_UP;

        const auto air = CreateNote(p_chart);
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
