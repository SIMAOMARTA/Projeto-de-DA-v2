/**
 * @file RegisterAllocator.h
 * @brief Declaração do alocador de registos por coloração de grafos.
 *
 * Implementa quatro variantes de alocação baseadas na heurística de Kempe/Chaitin:
 *  - **basic**     : coloração greedy sem limite de spills (T2.1).
 *  - **spilling**  : coloração com no máximo K spills (T2.2).
 *  - **splitting** : divisão iterativa de webs antes da coloração (T2.3).
 *  - **free**      : heurística própria de Chaitin grau/tamanho (T2.4).
 *
 * Todos os métodos são estáticos (classe utilitária sem estado).
 */

#ifndef REGISTER_ALLOCATOR_H
#define REGISTER_ALLOCATOR_H

#include "InterferenceGraph.h"
#include "AlgorithmConfig.h"
#include <vector>
#include <set>
#include <ostream>

class RegisterAllocator {
public:
    /**
     * @struct Allocation
     * @brief Resultado da alocação de uma única web.
     */
    struct Allocation {
        int  webId   = -1; // ID da web alocada
        int  regIdx  = -1; // Índice do registo físico (0-based); -1 se spilled
        bool spilled = false; // true se a web foi enviada para memória
    };

    /**
     * @brief Aloca registos para todas as webs em @p graph.
     * @param graph   Grafo de interferência.
     * @param config  Número de registos e variante do algoritmo.
     * @return        Alocação por web (indexada pelo id da web).
     */
    static std::vector<Allocation> allocate(const InterferenceGraph& graph,
                                            const AlgorithmConfig&    config);

    /**
     * @brief Escreve o resultado da alocação em @p out.
     * @param graph        Grafo de interferência.
     * @param allocations  Resultado de allocate().
     * @param out          Stream de saída.
     */
    static void printResult(const InterferenceGraph&      graph,
                            const std::vector<Allocation>& allocations,
                            std::ostream&                  out);

private:
    /**
     * @brief Coloração greedy de Kempe com limite opcional de spills.
     * @param graph      Grafo de interferência.
     * @param N          Número de registos físicos.
     * @param spillCap   Máximo de spills; negativo = ilimitado.
     * @param feasible   Definido como @c true se o resultado respeita spillCap.
     */
    static std::vector<Allocation> kempeWithSpillCap(const InterferenceGraph& graph,
                                                     int numRegisters,
                                                     int spillCap,
                                                     bool& feasible);

    /**
     * @brief Seleciona o candidato a spill pelo maior grau de trabalho.
     * @param workDegree  Mapa id -> grau de trabalho atual de cada nó ativo.
     * @param active      Conjunto de ids dos nós ainda não removidos do grafo.
     */
    static int selectSpillCandidate(const std::map<int,int>& workDegree,
                                    const std::set<int>&     active);

    /**
     * @brief Divide uma web no ponto mediano das linhas de programa.
     * @param web  Web a dividir; deve ter pelo menos um ponto de programa.
     */
    static std::pair<Web,Web> splitWeb(const Web& web);

    /**
     * @brief Reconstrói um grafo de interferência a partir de uma lista de webs.
     * @param webs  Lista de webs atuais (após divisões ou modificações).
     */
    static InterferenceGraph rebuildGraphFromWebs(const std::vector<Web>& webs);
};

#endif // REGISTER_ALLOCATOR_H