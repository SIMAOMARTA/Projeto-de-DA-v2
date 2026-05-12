#include <iostream>
#include <string>

#include "Parser.h"
#include "InterferenceGraph.h"

#ifdef _WIN32
#include <windows.h>
#endif

// ---------------------------------------------------------------------------
// Testa o Parser e o InterferenceGraph com um ficheiro de ranges
// ---------------------------------------------------------------------------
static void testWithFile(const std::string& rangesFile) {
    std::cout << "\n========== " << rangesFile << " ==========\n";
    try {
        auto ranges = Parser::parseRanges(rangesFile);

        std::cout << "Live ranges lidos (" << ranges.size() << "):\n";
        for (const auto& lr : ranges)
            std::cout << "  " << lr.getVarName() << ": " << lr.toString() << "\n";

        InterferenceGraph graph(ranges);
        graph.print();

    } catch (const std::exception& e) {
        std::cerr << "[ERRO] " << e.what() << "\n";
    }
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (argc < 2) {
        std::cerr << "Uso: " << argv[0] << " ranges1.txt [ranges2.txt ...]\n";
        return 1;
    }

    for (int i = 1; i < argc; ++i)
        testWithFile(argv[i]);

    return 0;
}