#ifndef PARSER_H
#define PARSER_H

#include "LiveRange.h"
#include "AlgorithmConfig.h"
#include <string>
#include <vector>
#include <stdexcept>

class Parser {
public:

    static std::vector<LiveRange> parseRanges(const std::string& filename);
    static AlgorithmConfig parseRegisters(const std::string& filename);

private:

    static ProgramPoint parsePoint(const std::string& token);
    static std::string trim(const std::string& s);
    static bool isCommentOrEmpty(const std::string& line);
};

#endif // PARSER_H