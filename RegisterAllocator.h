#ifndef REGISTER_ALLOCATOR_H
#define REGISTER_ALLOCATOR_H

#include "InterferenceGraph.h"
#include "AlgorithmConfig.h"
#include <vector>
#include <map>
#include <set>
#include <ostream>

/**
 * @file RegisterAllocator.h
 * @brief Register allocation via graph colouring (Kempe/Chaitin heuristics).
 *
 * Supported algorithm variants (set via AlgorithmConfig):
 *  - "basic"    : Kempe greedy with unlimited spills.
 *  - "spilling" : Kempe with at most K spills (K = algorithmParam).
 *  - "splitting": Iterative web splitting (up to K splits) before colouring.
 *  - "free"     : Kempe with Chaitin degree/size spill-cost heuristic.
 */
class RegisterAllocator {
public:
    /** @brief Result of allocating a single web. */
    struct Allocation {
        int  webId   = -1;
        int  regIdx  = -1;      ///< 0-based register index; -1 = spilled
        bool spilled = false;
    };

    /**
     * @brief Allocate registers for all webs in @p graph.
     * @param graph   Interference graph.
     * @param config  Number of registers and algorithm variant.
     * @return        Per-web allocation (indexed by web id).
     */
    static std::vector<Allocation> allocate(const InterferenceGraph& graph,
                                            const AlgorithmConfig&    config);

    /**
     * @brief Write the allocation result to @p out.
     * @param graph        Interference graph (used for web metadata).
     * @param allocations  Result of allocate().
     * @param out          Output stream.
     */
    static void printResult(const InterferenceGraph&      graph,
                            const std::vector<Allocation>& allocations,
                            std::ostream&                  out);

private:
    /**
     * @brief Kempe greedy colouring with an optional spill cap.
     * @param graph      Interference graph.
     * @param N          Number of physical registers.
     * @param spillCap   Max spills; negative = unlimited.
     * @param feasible   Set to true iff the result respects spillCap.
     * @complexity O(W^2)
     */
    static std::vector<Allocation> kempeWithSpillCap(const InterferenceGraph& graph,
                                                     int numRegisters,
                                                     int spillCap,
                                                     bool& feasible);

    /**
     * @brief Select the spill candidate by highest working degree.
     * @complexity O(|active|)
     */
    static int selectSpillCandidate(const std::map<int,int>& workDegree,
                                    const std::set<int>&     active);

    /**
     * @brief Split a web at its median program line.
     * @complexity O(P log P)
     */
    static std::pair<Web,Web> splitWeb(const Web& web);

    /**
     * @brief Rebuild an interference graph from a list of webs.
     * @complexity O(W^2 * L)
     */
    static InterferenceGraph rebuildGraphFromWebs(const std::vector<Web>& webs);
};

#endif // REGISTER_ALLOCATOR_H