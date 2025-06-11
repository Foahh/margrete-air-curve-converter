#define NOMINMAX

#include <algorithm>
#include <utility>
#include <vector>

#include "Interpolator.h"

#include <format>

#include "Margrete.h"

void Interpolator::InterpolateChain(const size_t idx) {
    if (idx >= static_cast<int>(m_cctx.chains.size())) {
        throw std::out_of_range(std::format("Invalid chain index: {}", idx));
    }

    auto &chain = m_cctx.chains[idx];
    std::ranges::sort(chain);

    if (chain.size() < 2) {
        throw std::invalid_argument(std::format("Chain [{}] must have at least 2 notes", idx));
    }

    std::vector<MP_NOTEINFO> notes;

    const auto &solver = m_cctx.solver;

    MP_NOTEINFO n{};
    n.type = chain.front().type;
    n.longAttr = MP_NOTELONGATTR_CONTROL;
    n.width = chain.front().width;
    n.timelineId = chain.front().til;

    const double snap = m_cctx.snap;
    for (size_t i = 0; i < chain.size() - 1; ++i) {
        auto curr = chain[i];
        auto next = chain[i + 1];

        curr.t = static_cast<int>(std::round(curr.t / snap));
        next.t = static_cast<int>(std::round(next.t / snap));

        if (curr.t > next.t) {
            throw std::invalid_argument(
                    std::format("Invalid Chain [{}]: note [{}] (t={}) must be before note [{}] (t={})", idx, i, curr.t,
                                i + 1, next.t));
        }

        n.x = curr.x;
        n.tick = curr.t;
        n.height = curr.y;

        const double dX = next.x - curr.x;
        const double dT = next.t - curr.t;
        const double dY = next.y - curr.y;

        if (dX == 0 && curr.eY != EasingMode::Linear) {
            const int sY = next.y >= curr.y ? 1 : -1;
            const int fromY = curr.y;
            const int toY = next.y;

            for (int y = fromY; sY > 0 ? y <= toY : y >= toY; y += sY) {
                const double pY = (y - curr.y) / dY;
                const double fPY = solver.InverseSolve(pY, curr.eY);
                const auto fT = static_cast<int>(std::round(curr.t + fPY * dT));

                n.x = curr.x;
                n.height = y;
                n.tick = fT;

                if (!notes.empty() && notes.back().tick == fT) {
                    auto &last = notes.back();

                    const double pT = static_cast<double>(fT - curr.t) / dT;
                    const double fPT = solver.Solve(pT, curr.eY);
                    const double fPTy = curr.y + fPT * dY;

                    const double errLast = std::abs(fPTy - last.height);
                    const double errNew = std::abs(fPTy - n.height);

                    if (errNew < errLast) {
                        last = n;
                    }
                } else {
                    notes.push_back(n);
                }
            }
        } else if (curr.eX != EasingMode::Linear || curr.eY != EasingMode::Linear) {
            const int sX = next.x >= curr.x ? 1 : -1;
            const int fromX = curr.x;
            const int toX = next.x;

            for (int x = fromX; sX > 0 ? x <= toX : x >= toX; x += sX) {
                const double pX = (x - curr.x) / dX;
                const double fPX = solver.InverseSolve(pX, curr.eX);
                const auto fT = static_cast<int>(std::round(curr.t + fPX * dT));

                n.x = x;
                n.tick = fT;

                if (dY != 0) {
                    const double pT = (fT - curr.t) / dT;
                    const double fPT = solver.Solve(pT, curr.eY);
                    const auto fY = static_cast<int>(std::round(curr.y + fPT * dY));

                    n.height = fY;
                }

                if (!notes.empty() && notes.back().tick == fT) {
                    auto &last = notes.back();
                    const double pT = static_cast<double>(fT - curr.t) / dT;

                    const double fPTx = solver.Solve(pT, curr.eX);
                    const double idealX = curr.x + fPTx * dX;

                    double errLast = std::abs(idealX - last.x);
                    double errNew = std::abs(idealX - n.x);

                    if (dY != 0) {
                        const double fPTy = solver.Solve(pT, curr.eY);
                        const double idealY = curr.y + fPTy * dY;

                        errLast = std::hypot(errLast, std::abs(idealY - last.height));
                        errNew = std::hypot(errNew, std::abs(idealY - n.height));
                    }

                    if (errNew < errLast) {
                        last = n;
                    }
                } else {
                    notes.push_back(n);
                }
            }
        }
    }

    notes.front().longAttr = MP_NOTELONGATTR_BEGIN;
    notes.back().longAttr = MP_NOTELONGATTR_END;

    FinalizeSingle(notes);
    m_notes.push_back(std::move(notes));
}

Interpolator::Interpolator(Config &cctx) : m_cctx(cctx) {}

void Interpolator::Convert(const int idx) {
    m_notes.clear();

    if (idx >= 0 && idx < static_cast<int>(m_cctx.chains.size())) {
        return InterpolateChain(static_cast<size_t>(idx));
    }

    for (size_t i = 0; i < m_cctx.chains.size(); ++i) {
        InterpolateChain(i);
    }
}

void Interpolator::FinalizeSingle(std::vector<MP_NOTEINFO> &chain) const {
    for (auto &note: chain) {
        note.tick *= m_cctx.snap;
        note.tick += m_cctx.tOffset;
        note.x += m_cctx.xOffset;
        note.height += m_cctx.yOffset;
        if (m_cctx.clamp) {
            Clamp(note);
        }
    }
    std::ranges::sort(chain, [](const MP_NOTEINFO &a, const MP_NOTEINFO &b) { return a.tick < b.tick; });
}

void Interpolator::Clamp(MP_NOTEINFO &note) {
    note.height = std::clamp(note.height, 0, 360);
    note.x = std::clamp(note.x, 0, 15);
    note.width = std::max(1, std::min(note.width, 16 - note.x));
}

void Interpolator::CommitChart(const Margrete &mg) const {
    if (m_notes.empty()) {
        return;
    }

    try {
        mg.BeginRecording();
        const auto chart = mg.GetChart();
        for (const auto &chain: m_notes) {
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
