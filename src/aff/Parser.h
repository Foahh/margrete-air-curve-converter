#pragma once

#include <string>
#include <vector>

#include "Arc.h"
#include "Config.h"

namespace aff {
    /**
     * @class Parser
     * @brief Parses .aff files and strings into arc chains for the plugin.
     *
     * Manages arc parsing, chain linking, and provides parsing utilities for BPM, T, X, and Y values.
     */
    class Parser {
    public:
        /**
         * @brief Constructs a Parser with a reference to the configuration context.
         * @param m_cctx Reference to the plugin configuration context.
         */
        explicit Parser(Config &m_cctx) : m_cctx(m_cctx) {}

        /**
         * @brief Parses an .aff file from the given file path.
         * @param filePath Path to the .aff file.
         */
        void ParseFile(const std::string &filePath);
        /**
         * @brief Parses .aff data from a string.
         * @param str The string containing .aff data.
         */
        void Parse(const std::string &str);

    private:
        /** Current BPM value for parsing. */
        double m_bpm{100};
        /** Reference to the plugin configuration context. */
        Config &m_cctx;

        /** List of parsed arcs. */
        std::vector<Arc> m_arcs;
        /** List of arc chains. */
        std::vector<std::vector<Arc>> m_archains;
        /** Flags indicating if arcs have been handled. */
        std::vector<bool> m_handled;

        /**
         * @brief Parses a single arc or line from a string.
         * @param str The string to parse.
         */
        void ParseSingle(const std::string &str);

        /**
         * @brief Attempts to link arcs into a chain.
         * @param chain The chain to link.
         * @return True if linking was successful.
         */
        bool LinkArc(std::vector<Arc> &chain);
        /**
         * @brief Checks if an arc has a preceding arc in the chain.
         * @param arc The arc to check.
         * @return True if a preceding arc exists.
         */
        bool HasPrecedingArc(const Arc &arc);

        /**
         * @brief Parses BPM from a token string.
         * @param token The token containing BPM information.
         */
        void ParseBpm(const std::string &token);
        /**
         * @brief Parses a string for arc data.
         * @param str The string to parse.
         */
        void ParseString(const std::string &str);
        /**
         * @brief Parses a T (tick) value from a string.
         * @param str The string to parse.
         * @return The parsed tick value.
         */
        int ParseT(const std::string &str) const;
        /**
         * @brief Parses an X value from a string.
         * @param str The string to parse.
         * @return The parsed X value.
         */
        static int ParseX(const std::string &str);
        /**
         * @brief Parses a Y value from a string.
         * @param str The string to parse.
         * @return The parsed Y value.
         */
        static int ParseY(const std::string &str);
    };

} // namespace aff
