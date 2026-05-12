#ifndef REGISTER_ALLOCATOR_H
#define REGISTER_ALLOCATOR_H

#include "InterferenceGraph.h"
#include "AlgorithmConfig.h"
#include <vector>
#include <map>
#include <functional>

class RegisterAllocator {
public:
    struct Allocation {
        int  webId   = -1;
        int  regIdx  = -1;      // 0‑based register index, -1 = spilled (M)
        bool spilled = false;
    };

    static std::vector<Allocation> allocate(const InterferenceGraph& graph,
                                            const AlgorithmConfig&    config);

    static void printResult(const InterferenceGraph&      graph,
                            const std::vector<Allocation>& allocations,
                            std::ostream&                  out);

private:
    // ---- Sub‑algoritmos ----
    // Algoritmo Kempe com limite de spills (cap). Se cap < 0 → ilimitado.
    static std::vector<Allocation> kempeWithSpillCap(const InterferenceGraph& graph,
                                                     int numRegisters,
                                                     int spillCap,
                                                     bool& feasible);

    // Seleciona nó a spillar (heurística de grau máximo no subgrafo ativo)
    static int selectSpillCandidate(const std::map<int,int>& workDegree,
                                    const std::set<int>&     active);

    // Splitting: divide uma web em duas pela mediana das linhas
    static std::pair<Web,Web> splitWeb(const Web& web);

    // Reconstroi grafo de interferência a partir de uma lista de webs
    static InterferenceGraph rebuildGraphFromWebs(const std::vector<Web>& webs);
};

#endif