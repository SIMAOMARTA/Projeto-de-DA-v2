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
     * @struct AllocResult
     * @brief Agrupa as alocações e o grafo final (que pode diferir do grafo
     *        original após operações de splitting).
     *
     * O grafo armazenado em @c finalGraph é o que foi efetivamente colorido:
     *  - Para @c basic, @c spilling e @c free, é igual ao grafo de entrada.
     *  - Para @c splitting, pode ter mais nós/webs do que o original se uma
     *    ou mais webs foram divididas.
     *
     * @c allocations está indexado pelo id das webs de @c finalGraph.
     */
    struct AllocResult {
        std::vector<Allocation> allocations; ///< Alocação por web (id da web de finalGraph).
        InterferenceGraph       finalGraph;  ///< Grafo efetivamente colorido.
    };

    /**
     * @brief Aloca registos para todas as webs em @p graph.
     * @param graph   Grafo de interferência original.
     * @param config  Número de registos e variante do algoritmo.
     * @return        AllocResult com alocações e o grafo final usado.
     */
    static AllocResult allocate(const InterferenceGraph& graph,
                                const AlgorithmConfig&   config);

    /**
     * @brief Escreve o resultado da alocação em @p out.
     * @param result  Resultado de allocate() (contém grafo e alocações).
     * @param out     Stream de saída.
     */
    static void printResult(const AllocResult& result,
                            std::ostream&      out);

private:
    /**
     * @brief Coloração greedy de Kempe com limite opcional de spills.
     * @param graph      Grafo de interferência.
     * @param N          Número de registos físicos.
     * @param spillCap   Máximo de spills; negativo = ilimitado.
     * @param feasible   Definido como @c true se o resultado respeita spillCap.
     * @param silent     Se @c true, suprime mensagens de aviso no stderr
     *                   (usado nas iterações de sondagem do algoritmo spilling).
     */
    static std::vector<Allocation> kempeWithSpillCap(const InterferenceGraph& graph,
                                                     int numRegisters,
                                                     int spillCap,
                                                     bool& feasible,
                                                     bool silent = false);

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