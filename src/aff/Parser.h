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
        std::vector<std::vector<Arc>> m_archains;
        std::vector<bool> m_handled;

        void ParseSingle(const std::string &str);

        bool LinkArc(std::vector<Arc> &chain);
        bool HasPrecedingArc(const Arc &arc);

        void ParseBpm(const std::string &token);
        void ParseString(const std::string &str);
        int ParseT(const std::string &str) const;
        static int ParseX(const std::string &str);
        static int ParseY(const std::string &str);
    };

} // namespace aff
