#include <iostream>
#include <fstream>
#include <string>
#include <limits>

#include "Parser.h"
#include "InterferenceGraph.h"
#include "RegisterAllocator.h"
#include "AlgorithmConfig.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

// ----------------------------------------------------------------------
// Função que executa o pipeline completo e grava o resultado em ficheiro.
// Retorna true se a alocação foi bem‑sucedida (pode ter havido spills).
// ----------------------------------------------------------------------
static bool runAllocation(const std::string& rangesFile,
                          const std::string& registersFile,
                          const std::string& outputFile)
{
    try {
        // 1. Parse dos ficheiros de entrada
        auto ranges   = Parser::parseRanges(rangesFile);
        auto config   = Parser::parseRegisters(registersFile);
        std::cout << "Ficheiros lidos com sucesso.\n";

        // 2. Construção do grafo de interferência
        InterferenceGraph graph(ranges);
        std::cout << "Grafo de interferência construído: "
                  << graph.numNodes() << " webs.\n";

        // 3. Executar alocação de registos
        auto allocations = RegisterAllocator::allocate(graph, config);

        // 4. Imprimir resultado no ficheiro de saída
        std::ofstream out(outputFile);
        if (!out.is_open()) {
            std::cerr << "ERRO: Não foi possível criar o ficheiro de saída: "
                      << outputFile << "\n";
            return false;
        }
        RegisterAllocator::printResult(graph, allocations, out);
        out.close();
        std::cout << "Resultado guardado em: " << outputFile << "\n";
        return true;

    } catch (const std::exception& e) {
        std::cerr << "ERRO: " << e.what() << "\n";
        return false;
    }
}

// ----------------------------------------------------------------------
// Menu interativo simples
// ----------------------------------------------------------------------
static void interactiveMenu() {
    std::string rangesFile, registersFile, outputFile;
    while (true) {
        std::cout << "\n========== MENU ==========\n"
                  << "1. Definir ficheiro de live ranges\n"
                  << "2. Definir ficheiro de registos\n"
                  << "3. Definir ficheiro de saída\n"
                  << "4. Executar alocação\n"
                  << "5. Sair\n"
                  << "Opção: ";

        int op;
        std::cin >> op;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        switch (op) {
            case 1:
                std::cout << "Caminho do ficheiro de live ranges: ";
                std::getline(std::cin, rangesFile);
                break;
            case 2:
                std::cout << "Caminho do ficheiro de registos: ";
                std::getline(std::cin, registersFile);
                break;
            case 3:
                std::cout << "Caminho do ficheiro de saída: ";
                std::getline(std::cin, outputFile);
                break;
            case 4:
                if (rangesFile.empty() || registersFile.empty() || outputFile.empty()) {
                    std::cerr << "ERRO: Defina primeiro todos os ficheiros (opções 1-3).\n";
                } else {
                    runAllocation(rangesFile, registersFile, outputFile);
                }
                break;
            case 5:
                return;
            default:
                std::cerr << "Opção inválida.\n";
        }
    }
}

// ----------------------------------------------------------------------
// Ponto de entrada
// ----------------------------------------------------------------------
int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Verifica se é modo batch: -b <ranges> <registers> <output>
    if (argc == 5 && std::string(argv[1]) == "-b") {
        std::string rangesFile    = argv[2];
        std::string registersFile = argv[3];
        std::string outputFile    = argv[4];
        bool ok = runAllocation(rangesFile, registersFile, outputFile);
        return ok ? 0 : 1;
    }

    // Caso contrário, modo interativo
    interactiveMenu();
    return 0;
}