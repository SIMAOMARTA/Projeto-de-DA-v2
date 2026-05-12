#ifndef REGISTER_ALLOCATOR_H
#define REGISTER_ALLOCATOR_H

#include "InterferenceGraph.h"
#include "AlgorithmConfig.h"
#include <vector>
#include <string>
#include <map>

/**
 * @file RegisterAllocator.h
 * @brief Algoritmo greedy para colorir os grafos para alocação dos registos.
 *
 * Implementa o algoritmo seguinte (3.2):
 * 1. Kempe's algorithm:
 *    Enquanto o grafo não estiver vazio:
 *     Para cada nó com grau < N: remove do grafo de trabalho, empilha.
 *     Se todos os nós restantes têm grau >= N:
 *       Seleciona um nó para "spill" (sem registo atribuído) e remove-o.
 *
 * 2. Colorir:
 *   Desempilha cada nó e atribui-lhe a cor mais baixa que não esteja
 *   usada pelos vizinhos já coloridos. Como o grau era < N quando foi
 *   empilhado, há sempre uma cor disponível.
 *
 * Os nós "spilled" ficam com registo = -1.
 *
 * Complexidade: O(W^2) no pior caso, onde W = número de webs.
 */

class RegisterAllocator {
public:
    /**
     * @brief Alocação para uma web.
     */
    struct Allocation {
        int  webId   = -1;     // id da web
        int  regIdx  = -1;     // índice do registo (0-based); -1 = spilled (memória)
        bool spilled = false;  // true se foi para memória
    };

    /**
     * @brief Executa a alocação greedy.
     *
     * @param graph   Grafo de interferência já construído.
     * @param config  Configuração (numRegisters, algorithm, algorithmParam).
     * @return        Vetor com uma Allocation por web, na ordem dos ids.
     */
    static std::vector<Allocation> allocate(const InterferenceGraph& graph,
                                            const AlgorithmConfig&    config);

    /**
     * @brief Imprime o resultado no formato do enunciado.
     *
     * @param graph       Grafo com as webs.
     * @param allocations Resultado de allocate().
     * @param out         Stream de saída (cout ou ficheiro).
     */
    static void printResult(const InterferenceGraph&      graph,
                            const std::vector<Allocation>& allocations,
                            std::ostream&                  out);

private:
    /**
     * @brief Seleciona o nó a spilhar quando todos têm grau >= N.
     *
     * Heurística simples: escolhe o nó com maior grau no subgrafo
     * de trabalho (mais interferências → mais difícil de colorir).
     *
     * @param workDegree  Graus actuais no subgrafo de trabalho.
     * @param active      Conjunto de nós ainda não removidos.
     * @return            Id do nó a spilhar.
     */
    static int selectSpillCandidate(const std::map<int,int>& workDegree,
                                    const std::set<int>&     active);
};

#endif // REGISTER_ALLOCATOR_H