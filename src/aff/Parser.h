#pragma once

#include <string>
#include <vector>

#include "Arc.h"
#include "Config.h"

namespace aff {
    class Parser {
    public:
        explicit Parser(Config &m_cctx) : m_cctx(m_cctx) {}

        void ParseFile(const std::string &filePath);
        void Parse(const std::string &str);

    private:
        double m_bpm{100};
        Config &m_cctx;

        std::vector<Arc> m_arcs;
        std::vector<bool> m_handled;
        std::vector<std::vector<Arc>> m_chains;

        void ParseSingle(const std::string &str);

        bool PushLink(std::vector<Arc> &chain);
        bool HasPreceding(const Arc &arc);
        std::vector<Arc> BuildChain(size_t startIndex);
        void ParseBpm(const std::string &token);
        void ParseString(const std::string &str);
        void FinalizeArc(const std::string_view &str, Arc &arc);
        void FinalizeNote() const;

        int ParseT(const std::string &str) const;
        static int ParseX(const std::string &str);
        static int ParseY(const std::string &str);
    };

} // namespace aff
