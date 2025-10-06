#include <fstream>
#include <iostream>
#include <sstream>

#include "Arc.h"
#include "Parser.h"
#include "Primitive.h"

namespace aff {
    void Parser::ParseSingle(const std::string &str) {
        const size_t start = str.find('(');
        const size_t end = str.rfind(')');
        if (start == std::string::npos || end == std::string::npos) {
            throw std::invalid_argument("Invalid arc format - missing parentheses");
        }

        const std::string content = str.substr(start + 1, end - start - 1);
        std::vector<std::string> parts;
        std::stringstream ss(content);
        std::string part;

        while (getline(ss, part, ',')) {
            parts.push_back(part);
        }

        if (parts.size() < 10) {
            throw std::invalid_argument("Invalid arc format - not enough parameters");
        }

        Arc arc;
        arc.t = ParseT(parts[0]);
        arc.toT = ParseT(parts[1]);
        if (arc.Duration() < 0) {
            throw std::invalid_argument("Invalid arc format - time length must be positive");
        }
        if (arc.Duration() == 0) {
            arc.toT = arc.t + m_cctx.snap;
        }
        arc.x = ParseX(parts[2]);
        arc.toX = ParseX(parts[3]);
        arc.y = ParseY(parts[5]);
        arc.toY = ParseY(parts[6]);
        arc.type = stoi(parts[7]);
        arc.trace = parts[9] != "false";

        using enum EasingMode;

        const int len = arc.Duration();
        if (len >= 2 && parts[4] == "b") {
            Arc first = arc;
            first.toT = arc.t + len / 2;
            first.toX = (arc.x + arc.toX) / 2;
            first.toY = (arc.y + arc.toY) / 2;
            first.eX = Out;
            first.eY = Out;

            Arc second = arc;
            second.t = first.toT;
            second.x = first.toX;
            second.y = first.toY;
            second.eX = In;
            second.eY = In;

            m_arcs.push_back(first);
            m_arcs.push_back(second);
        } else {
            if (parts[4] == "si") {
                arc.eX = In;
                arc.eY = Linear;
            } else if (parts[4] == "so") {
                arc.eX = Out;
                arc.eY = Linear;
            } else if (parts[4] == "sisi") {
                arc.eX = In;
                arc.eY = In;
            } else if (parts[4] == "soso") {
                arc.eX = Out;
                arc.eY = Out;
            } else if (parts[4] == "siso") {
                arc.eX = In;
                arc.eY = Out;
            } else if (parts[4] == "sosi") {
                arc.eX = Out;
                arc.eY = In;
            } else {
                arc.eX = Linear;
                arc.eY = Linear;
            }
            m_arcs.push_back(arc);
        }
    }

    int Parser::ParseT(const std::string &str) const {
        const int time = std::stoi(str);
        const double ticks = time / (60000.0 / m_bpm) * mgxc::BEAT_TICKS;
        return static_cast<int>(std::round(ticks));
    }

    int Parser::ParseX(const std::string &str) {
        const double x = std::stod(str);
        constexpr double start = -0.2;
        constexpr double step = 0.1;
        return static_cast<int>(std::floor((x - start) / step));
    }

    int Parser::ParseY(const std::string &str) {
        const double y = std::stod(str);
        return static_cast<int>(y * 100.0);
    }

    void Print(const Config &cctx) {
        std::cout << "Parsed " << cctx.chains.size() << std::endl;
        int i = 0;
        for (const mgxc::Chain &chain: cctx.chains) {
            std::cout << i++ << ":" << std::endl;
            for (const mgxc::Joint &joint: chain) {
                std::cout << "  t=" << joint.t << ", x=" << joint.x << ", y=" << joint.y
                          << ", eX=" << static_cast<int>(joint.eX) << ", eY=" << static_cast<int>(joint.eY)
                          << std::endl;
            }
        }
    }

    void Parser::Parse(const std::string &str) {
        m_archains.clear();
        m_arcs.clear();
        m_handled.clear();

        ParseString(str);
        if (m_arcs.empty()) {
            throw std::runtime_error("No arcs found in the chart");
        }

        for (size_t i = 0; i < m_arcs.size(); ++i) {
            if (m_handled[i] || HasPrecedingArc(m_arcs[i])) {
                continue;
            }

            std::vector<Arc> chain;

            chain.push_back(m_arcs[i]);
            m_handled[i] = true;

            bool found;
            do {
                found = LinkArc(chain);
            } while (found);

            if (!chain.empty()) {
                m_archains.push_back(std::move(chain));
            }
        }

        if (!m_cctx.append) {
            m_cctx.chains.clear();
        }

        for (const std::vector<Arc> &archain: m_archains) {
            if (archain.empty()) {
                continue;
            }

            mgxc::Chain chain;
            chain.width = m_cctx.width;
            chain.til = m_cctx.til;
            chain.type = archain.front().trace ? MP_NOTETYPE_AIRCRUSH : MP_NOTETYPE_AIRSLIDE;

            for (size_t i = 0; i < archain.size(); ++i) {
                const Arc &arc = archain[i];
                chain.emplace_back(arc.t, arc.x, arc.y, arc.eX, arc.eY);
                if (i == archain.size() - 1) {
                    chain.emplace_back(arc.toT, arc.toX, arc.toY, arc.eX, arc.eY);
                }
            }

            m_cctx.chains.push_back(std::move(chain));
        }

        Print(m_cctx);
    }

    void Parser::ParseFile(const std::string &filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            throw std::invalid_argument("Could not open file: " + filePath);
        }

        const std::string content((std::istreambuf_iterator(file)), (std::istreambuf_iterator<char>()));
        Parse(content);
    }

    bool Parser::LinkArc(std::vector<Arc> &chain) {
        const Arc &lastArc = chain.back();
        size_t linkCount = 0;
        size_t linkIndex = m_arcs.size();

        for (size_t j = 0; j < m_arcs.size(); ++j) {
            if (m_handled[j]) {
                continue;
            }

            if (const Arc &candidate = m_arcs[j]; lastArc.CanLinkWith(candidate)) {
                linkCount++;
                if (linkIndex == m_arcs.size()) {
                    linkIndex = j;
                }
            }
        }

        if (linkCount > 1) {
            return false;
        }

        if (linkCount == 1) {
            chain.push_back(m_arcs[linkIndex]);
            m_handled[linkIndex] = true;
            return true;
        }

        return false;
    }

    bool Parser::HasPrecedingArc(const Arc &arc) {
        for (size_t j = 0; j < m_arcs.size(); ++j) {
            if (m_handled[j]) {
                continue;
            }
            if (m_arcs[j].CanLinkWith(arc)) {
                return true;
            }
        }
        return false;
    }

    void Parser::ParseBpm(const std::string &token) {
        if (token.rfind("timing(", 0) == 0) {
            std::string content = token.substr(7);
            content.erase(content.find(')'));

            std::stringstream ss(content);
            std::string param;
            std::vector<double> params;

            while (getline(ss, param, ',')) {
                params.push_back(std::stod(param));
            }

            if (params.size() >= 3 && params[0] == 0) {
                m_bpm = params[1];
            }
        }
    }

    void Parser::ParseString(const std::string &str) {
        std::stringstream ss(str);
        std::string token;

        while (getline(ss, token, ';')) {
            std::erase_if(token, [](const auto c) { return ::isspace(c); });
            ParseBpm(token);
        }

        ss.clear();
        ss.str(str);

        while (getline(ss, token, ';')) {
            std::erase_if(token, [](const auto c) { return ::isspace(c); });

            if (token.rfind("arc(", 0) == 0) {
                try {
                    ParseSingle(token);
                } catch (...) { continue; }
            }
        }
        m_handled.resize(m_arcs.size(), false);
    }
} // namespace aff
