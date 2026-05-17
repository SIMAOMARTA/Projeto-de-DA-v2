/**
 * @file main.cpp
 * @brief Ponto de partida para a alocação de registos.
 *
 * O programa pode ser executado em dois modos:
 *
 * **Modo batch**, modo não interativo:
 * @code
 * ./prog -b testes/ranges.txt testes/registers.txt testes/output.txt
 * @endcode
 *
 * **Modo interativo**, onde se apresenta um menu simples que
 * permite configurar os ficheiros e executar a alocação de forma iterativa.
 *
 * @note Alteração relativamente à versão anterior: RegisterAllocator::allocate()
 * devolve agora um triplo {alocações, grafo final, metadados}. O desempacotamento
 * foi atualizado para extrair também a string de metadados, que é passada a
 * printResult() para ser escrita no ficheiro de saída.
 */

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <iostream>
#include <fstream>
#include <string>
#include <limits>

#include "Parser.h"
#include "InterferenceGraph.h"
#include "RegisterAllocator.h"
#include "AlgorithmConfig.h"

static bool runAllocation(const std::string& rangesFile,
                          const std::string& registersFile,
                          const std::string& outputFile)
{
    // Algoritmo:
    // 1. Parse dos dois ficheiros de entrada (ranges e configuração).
    // 2. Construção do grafo de interferência.
    // 3. Alocação de registos com o algoritmo configurado.
    // 4. Escrita do resultado (+ metadados) no ficheiro de saída.
    try {
        // Parse dos ficheiros de entrada
        auto ranges   = Parser::parseRanges(rangesFile);
        auto config   = Parser::parseRegisters(registersFile);
        std::cout << "Ficheiros lidos com sucesso.\n";

        // Construção do grafo de interferência
        InterferenceGraph graph(ranges);
        std::cout << "Grafo de interferência construído: "
                  << graph.numNodes() << " webs.\n";

        // Executar alocação de registos.
        // allocate() devolve {alocações, grafo final, metadados}.
        // Para splitting o grafo final pode ter mais webs do que o original
        // (uma por cada divisão efectuada); os outros algoritmos devolvem
        // uma cópia do grafo original inalterado.
        // Os metadados descrevem quais webs foram vertidas (spilling) ou
        // divididas (splitting) e são escritos no ficheiro de saída.
        auto [allocations, finalGraph, metadata] =
            RegisterAllocator::allocate(graph, config);

        // Imprimir resultado no ficheiro de saída
        std::ofstream out(outputFile);
        if (!out.is_open()) {
            std::cerr << "ERRO: Não foi possível criar o ficheiro de saída: "
                      << outputFile << "\n";
            return false;
        }
        RegisterAllocator::printResult(finalGraph, allocations, out, metadata);
        out.close();
        std::cout << "Resultado guardado em: " << outputFile << "\n";
        return true;

    } catch (const std::exception& e) {
        std::cerr << "ERRO: " << e.what() << "\n";
        return false;
    }
}

static void interactiveMenu() {
    // Apresenta as seguintes opções em loop até o utilizador escolher a opção "Sair":
    // 1. Definir o ficheiro de live ranges.
    // 2. Definir o ficheiro de configuração de registos.
    // 3. Definir o ficheiro de saída.
    // 4. Executar a alocação (requer sempre as opções 1-3 preenchidas).
    // 5. Sair.
    // Entradas inválidas são reportadas com mensagem de erro.
    std::string rangesFile, registersFile, outputFile;
    while (true) {
        std::cout << "\n================== MENU ==================\n"
                  << "1. Definir ficheiro de live ranges\n"
                  << "2. Definir ficheiro de registos\n"
                  << "3. Definir ficheiro de saída\n"
                  << "4. Executar alocação\n"
                  << "5. Sair\n"
                  << "Opção: ";

        int op;
        if (!(std::cin >> op)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cerr << "ERRO: Opção inválida. Por favor, introduza um número de 1 a 5.\n";
            continue;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        switch (op) {
            case 1:
                std::cout << "Caminho do ficheiro de range: ";
                std::getline(std::cin, rangesFile);
                break;
            case 2:
                std::cout << "Caminho do ficheiro de registo: ";
                std::getline(std::cin, registersFile);
                break;
            case 3:
                std::cout << "Caminho do ficheiro de saída: ";
                std::getline(std::cin, outputFile);
                break;
            case 4:
                if (rangesFile.empty() || registersFile.empty() || outputFile.empty()) {
                    std::cerr << "ERRO: Defina primeiro todos os ficheiros (opções 1 a 3).\n";
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

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Modo batch: ./prog -b ranges.txt registers.txt output.txt
    if (argc == 5 && std::string(argv[1]) == "-b") {
        std::string rangesFile    = argv[2];
        std::string registersFile = argv[3];
        std::string outputFile    = argv[4];
        bool ok = runAllocation(rangesFile, registersFile, outputFile);
        return ok ? 0 : 1;
    }

    // Modo interativo: Qualquer outra invocação inicia o menu.
    interactiveMenu();
    return 0;
}