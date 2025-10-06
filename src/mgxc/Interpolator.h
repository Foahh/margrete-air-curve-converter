#pragma once
#include <MargretePlugin.h>
#include <vector>

#include "Config.h"
#include "MargreteHandle.h"
#include "Primitive.h"

/**
 * @class Interpolator
 * @brief Converts chains to note data and commits them to the plugin chart.
 */
class Interpolator {
public:
    /**
     * @brief Constructs an Interpolator with a reference to the configuration context.
     * @param cctx Reference to the plugin configuration context.
     */
    explicit Interpolator(Config &cctx);

    /**
     * @brief Converts chains to note data for the specified index or all chains.
     * @param idx Index of the chain to convert, or -1 for all.
     */
    void Convert(int idx = -1);
    /**
     * @brief Commits the converted note data to the plugin chart.
     * @param mg MargreteHandle for plugin chart access.
     */
    void Commit(const MargreteHandle &mg) const;

private:
    Config &m_cctx; /**< Reference to the plugin configuration context. */

    std::vector<std::vector<MP_NOTEINFO>> m_noteChains; /**< Converted note chains. */
    std::vector<MP_NOTEINFO> m_noteChain; /**< Temporary note chain for conversion. */

    /**
     * @brief Commits a single note chain to the plugin chart.
     * @param p_chart Plugin chart pointer.
     * @param chain Note chain to commit.
     */
    static void CommitChain(const MargreteComPtr<IMargretePluginChart> &p_chart, const std::vector<MP_NOTEINFO> &chain);
    /**
     * @brief Creates a new note in the plugin chart.
     * @param p_chart Plugin chart pointer.
     * @return Smart pointer to the created note.
     */
    static MargreteComPtr<IMargretePluginNote> CreateNote(const MargreteComPtr<IMargretePluginChart> &p_chart);

    /**
     * @brief Interpolates a single chain by index.
     * @param idx Index of the chain to interpolate.
     */
    void InterpolateChain(size_t idx);
    /**
     * @brief Clamps note values to valid ranges.
     * @param note Note to clamp.
     */
    static void Clamp(MP_NOTEINFO &note);

    void PushSegment(const mgxc::Chain &chain, const mgxc::Joint &curr, const mgxc::Joint &next,
                     const mgxc::Joint &base);
    void VerticalSegment(const mgxc::Chain &chain, const mgxc::Joint &curr, const mgxc::Joint &next);
    void HorizontalSegment(const mgxc::Chain &chain, const mgxc::Joint &curr, const mgxc::Joint &next);
};
