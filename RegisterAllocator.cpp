#include "RegisterAllocator.h"
#include <iostream>
#include <stack>
#include <algorithm>
#include <set>
#include <map>
#include <numeric>

// ==================== Kempe com spill cap ====================

/**
 * @brief Kempe/Chaitin greedy graph-colouring with an optional spill cap.
 *
 * Phase 1 – simplification: repeatedly remove nodes with working degree < N
 * onto a stack.  When stuck, select a spill candidate (highest working degree).
 *
 * Phase 2 – colouring: pop stack and assign the lowest free colour.
 *
 * @param graph      Interference graph.
 * @param N          Physical registers available.
 * @param spillCap   Maximum spills; negative = unlimited.
 * @param feasible   Output: true iff allocation respects spillCap.
 * @return           Per-web allocation vector.
 * @complexity O(W^2) where W = number of webs.
 */
std::vector<RegisterAllocator::Allocation>
RegisterAllocator::kempeWithSpillCap(const InterferenceGraph& graph,
                                     int N, int spillCap,
                                     bool& feasible)
{
    const int W = graph.numNodes();
    std::vector<Allocation> result(W);
    for (int i = 0; i < W; ++i) {
        result[i].webId   = i;
        result[i].regIdx  = -1;
        result[i].spilled = false;
    }
    if (W == 0) { feasible = true; return result; }

    std::map<int,int> workDegree;
    for (int i = 0; i < W; ++i) workDegree[i] = graph.degree(i);

    std::set<int> active;
    for (int i = 0; i < W; ++i) active.insert(i);

    std::stack<int> colourStack;
    int spillCount = 0;

    // Phase 1
    while (!active.empty()) {
        bool removedAny = true;
        while (removedAny) {
            removedAny = false;
            for (auto it = active.begin(); it != active.end(); ) {
                int id = *it;
                if (workDegree[id] < N) {
                    colourStack.push(id);
                    for (int nb : graph.neighbors(id))
                        if (active.count(nb)) workDegree[nb]--;
                    it = active.erase(it);
                    removedAny = true;
                } else { ++it; }
            }
        }
        if (!active.empty()) {
            if (spillCap >= 0 && spillCount >= spillCap) {
                feasible = false;
                return result;
            }
            int victim = selectSpillCandidate(workDegree, active);
            result[victim].spilled = true;
            spillCount++;
            for (int nb : graph.neighbors(victim))
                if (active.count(nb)) workDegree[nb]--;
            active.erase(victim);
        }
    }

    // Phase 2
    std::map<int,int> colour;
    while (!colourStack.empty()) {
        int id = colourStack.top(); colourStack.pop();
        std::set<int> used;
        for (int nb : graph.neighbors(id)) {
            auto it = colour.find(nb);
            if (it != colour.end()) used.insert(it->second);
        }
        int chosen = 0;
        while (used.count(chosen)) ++chosen;
        if (chosen >= N) {
            result[id].spilled = true;
            spillCount++;
        } else {
            colour[id]        = chosen;
            result[id].regIdx = chosen;
        }
    }

    feasible = (spillCap < 0) || (spillCount <= spillCap);
    if (spillCount > 0 && spillCap < 0) {
        std::cerr << "AVISO: alocacao aos " << N
                  << " registos nao foi possivel sem spilling.\n"
                  << "       " << spillCount << " web(s) alocada(s) a memoria.\n";
    }
    return result;
}

// ==================== Spill candidate selection ====================

/**
 * @brief Select the active node with the highest working degree (ties by id).
 * @complexity O(|active|)
 */
int RegisterAllocator::selectSpillCandidate(const std::map<int,int>& workDegree,
                                            const std::set<int>&     active)
{
    int best = *active.begin(), bestDeg = -1;
    for (int id : active) {
        int deg = workDegree.at(id);
        if (deg > bestDeg) { bestDeg = deg; best = id; }
    }
    return best;
}

// ==================== Web splitting ====================

/**
 * @brief Split a web into two halves at the median program line.
 *
 * Lines at or before the median go into the first half; the rest into the
 * second.  Program-point annotations (isStart / isEnd) are preserved.
 *
 * @param web  Web to split.
 * @return     Pair {firstHalf, secondHalf} with placeholder id -1.
 * @complexity O(P log P) where P = total program points in the web.
 */
std::pair<Web,Web> RegisterAllocator::splitWeb(const Web& web)
{
    const std::set<int>& lineSet = web.getLineSet();
    std::vector<int> lines(lineSet.begin(), lineSet.end());
    // lineSet is already sorted

    size_t mid = lines.size() / 2;
    if (mid == 0) mid = 1; // at least one line per half

    std::set<int> linesFirst(lines.begin(), lines.begin() + (int)mid);
    std::set<int> linesSecond(lines.begin() + (int)mid, lines.end());

    const auto& ranges = web.getRanges();
    std::vector<ProgramPoint> pts1, pts2;
    for (const auto& lr : ranges) {
        for (const auto& p : lr.getPoints()) {
            if (linesFirst.count(p.line))  pts1.push_back(p);
            else                            pts2.push_back(p);
        }
    }

    // Degenerate fallback: put everything in first half (avoids empty LiveRange)
    if (pts1.empty()) pts1 = pts2;
    if (pts2.empty()) pts2.push_back(pts1.back());

    LiveRange lr1(web.getVarName() + "_s0", pts1);
    LiveRange lr2(web.getVarName() + "_s1", pts2);
    return { Web(-1, lr1), Web(-1, lr2) };
}

/**
 * @brief Rebuild an InterferenceGraph from a list of Webs.
 *
 * Extracts all LiveRanges and passes them through the normal constructor.
 * @complexity Same as InterferenceGraph constructor: O(W^2 * L).
 */
InterferenceGraph RegisterAllocator::rebuildGraphFromWebs(const std::vector<Web>& webs)
{
    std::vector<LiveRange> allRanges;
    for (const auto& w : webs)
        for (const auto& lr : w.getRanges())
            allRanges.push_back(lr);
    return InterferenceGraph(allRanges);
}

// ==================== Free algorithm: Chaitin spill heuristic ====================

/**
 * @brief Chaitin spill heuristic: pick node that maximises degree / liveRangeSize.
 *
 * A web that spans many lines is relatively cheap to spill; a web with many
 * neighbours but a short live range is expensive to keep in a register.
 * We spill the node with the highest degree-to-size ratio.
 *
 * @complexity O(|active|)
 */
static int chaitinCandidate(const InterferenceGraph&  graph,
                            const std::map<int,int>&  workDegree,
                            const std::set<int>&      active)
{
    const auto& webs  = graph.getWebs();
    int    best       = *active.begin();
    double bestScore  = -1.0;
    for (int id : active) {
        int deg  = workDegree.at(id);
        int size = (int)webs[id].getLineSet().size();
        if (size == 0) size = 1;
        double score = static_cast<double>(deg) / size;
        if (score > bestScore) { bestScore = score; best = id; }
    }
    return best;
}

/**
 * @brief Kempe colouring with the Chaitin spill-cost heuristic.
 *
 * Identical flow to kempeWithSpillCap (unlimited spills) but uses
 * chaitinCandidate instead of highest-degree selection.
 *
 * @complexity O(W^2)
 */
static std::vector<RegisterAllocator::Allocation>
kempeChaitin(const InterferenceGraph& graph, int N, bool& feasible)
{
    const int W = graph.numNodes();
    std::vector<RegisterAllocator::Allocation> result(W);
    for (int i = 0; i < W; ++i) {
        result[i].webId = i; result[i].regIdx = -1; result[i].spilled = false;
    }
    if (W == 0) { feasible = true; return result; }

    std::map<int,int> workDegree;
    for (int i = 0; i < W; ++i) workDegree[i] = graph.degree(i);
    std::set<int> active;
    for (int i = 0; i < W; ++i) active.insert(i);
    std::stack<int> colourStack;
    int spillCount = 0;

    while (!active.empty()) {
        bool removedAny = true;
        while (removedAny) {
            removedAny = false;
            for (auto it = active.begin(); it != active.end(); ) {
                int id = *it;
                if (workDegree[id] < N) {
                    colourStack.push(id);
                    for (int nb : graph.neighbors(id))
                        if (active.count(nb)) workDegree[nb]--;
                    it = active.erase(it);
                    removedAny = true;
                } else { ++it; }
            }
        }
        if (!active.empty()) {
            int victim = chaitinCandidate(graph, workDegree, active);
            result[victim].spilled = true;
            spillCount++;
            for (int nb : graph.neighbors(victim))
                if (active.count(nb)) workDegree[nb]--;
            active.erase(victim);
        }
    }

    std::map<int,int> colour;
    while (!colourStack.empty()) {
        int id = colourStack.top(); colourStack.pop();
        std::set<int> used;
        for (int nb : graph.neighbors(id)) {
            auto it = colour.find(nb);
            if (it != colour.end()) used.insert(it->second);
        }
        int chosen = 0;
        while (used.count(chosen)) ++chosen;
        if (chosen >= N) { result[id].spilled = true; spillCount++; }
        else { colour[id] = chosen; result[id].regIdx = chosen; }
    }

    feasible = true;
    if (spillCount > 0)
        std::cerr << "AVISO [free/Chaitin]: " << spillCount
                  << " web(s) alocada(s) a memoria.\n";
    return result;
}

// ==================== Main dispatch ====================

/**
 * @brief Dispatch to the appropriate allocation sub-algorithm.
 *
 * - "basic"    : Kempe greedy, unlimited spills.
 * - "spilling" : Kempe, at most K spills (tries 0..K until feasible).
 * - "splitting": Iteratively splits the highest-degree web (up to K times)
 *                and retries colouring without spills; falls back to unlimited
 *                spilling if splitting alone is insufficient.
 * - "free"     : Kempe with Chaitin degree/size spill heuristic.
 *
 * @complexity Dominated by sub-algorithm; O(W^2) for basic/spilling/free,
 *             O(K * W^2 * L) for splitting.
 */
std::vector<RegisterAllocator::Allocation>
RegisterAllocator::allocate(const InterferenceGraph& graph,
                            const AlgorithmConfig&    config)
{
    const int N = config.numRegisters;
    bool feasible = false;
    std::vector<Allocation> result;

    // ---- basic ----
    if (config.algorithm == "basic") {
        result = kempeWithSpillCap(graph, N, -1, feasible);
        return result;
    }

    // ---- spilling ----
    if (config.algorithm == "spilling") {
        int K = config.algorithmParam;
        for (int cap = 0; cap <= K; ++cap) {
            result = kempeWithSpillCap(graph, N, cap, feasible);
            if (feasible) return result;
        }
        // All K attempts failed: mark everything as spilled (registers: 0, all M)
        std::cerr << "ERRO: alocacao com spilling ate K=" << K
                  << " nao foi possivel.\n";
        const int W2 = graph.numNodes();
        result.assign(W2, Allocation{});
        for (int i = 0; i < W2; ++i) {
            result[i].webId   = i;
            result[i].regIdx  = -1;
            result[i].spilled = true;
        }
        return result;
    }

    // ---- splitting ----
    if (config.algorithm == "splitting") {
        int K = config.algorithmParam;

        // Try without any modification first
        result = kempeWithSpillCap(graph, N, 0, feasible);
        if (feasible) return result;

        // Work on an evolving list of webs
        std::vector<Web> currentWebs = graph.getWebs();

        for (int splitsDone = 0; splitsDone < K; ++splitsDone) {
            // Rebuild to get up-to-date degrees
            InterferenceGraph curGraph = rebuildGraphFromWebs(currentWebs);

            // Pick the web with the highest degree as split candidate
            int candidate = -1, maxDeg = -1;
            for (int i = 0; i < curGraph.numNodes(); ++i) {
                if (curGraph.degree(i) > maxDeg) {
                    maxDeg = curGraph.degree(i);
                    candidate = i;
                }
            }
            // Nothing worth splitting
            if (candidate == -1 || maxDeg < N) break;

            // Split the candidate and replace it in currentWebs
            const Web& candWeb = curGraph.getWebs()[candidate];
            auto [w1, w2] = splitWeb(candWeb);

            std::vector<Web> newWebs;
            newWebs.reserve(currentWebs.size() + 1);
            for (int i = 0; i < curGraph.numNodes(); ++i) {
                if (i == candidate) {
                    newWebs.push_back(std::move(w1));
                    newWebs.push_back(std::move(w2));
                } else {
                    newWebs.push_back(curGraph.getWebs()[i]);
                }
            }
            currentWebs = std::move(newWebs);

            // Try colouring the new graph with 0 spills
            InterferenceGraph newGraph = rebuildGraphFromWebs(currentWebs);
            result = kempeWithSpillCap(newGraph, N, 0, feasible);
            if (feasible) return result;
        }

        // Splitting alone was not enough: fall back to unlimited spilling
        std::cerr << "ERRO: alocacao com splitting ate K=" << K
                  << " nao foi possivel sem spilling.\n"
                  << "       Recorrendo a spilling ilimitado.\n";
        InterferenceGraph fallback = rebuildGraphFromWebs(currentWebs);
        result = kempeWithSpillCap(fallback, N, -1, feasible);
        return result;
    }

    // ---- free (Chaitin heuristic) ----
    if (config.algorithm == "free") {
        result = kempeChaitin(graph, N, feasible);
        return result;
    }

    // Should never reach here
    return result;
}

// ==================== Output ====================

/**
 * @brief Print the allocation result to an output stream.
 *
 * Outputs the web listing followed by register assignments.
 * Spilled webs are marked with "M:".
 *
 * @complexity O(W * P) where P = average program points per web.
 */
void RegisterAllocator::printResult(const InterferenceGraph&       graph,
                                    const std::vector<Allocation>&  allocations,
                                    std::ostream&                   out)
{
    const auto& webs = graph.getWebs();
    const int   W    = (int)webs.size();

    out << "# Total number of webs followed by the listing of the program points of each one\n";
    out << "# program points in each web are sorted in ascending order\n";
    out << "webs: " << W << "\n";
    for (int i = 0; i < W; ++i)
        out << "web" << i << ": " << webs[i].toString() << "\n";

    int maxReg = -1;
    for (const auto& a : allocations)
        if (!a.spilled && a.regIdx > maxReg) maxReg = a.regIdx;

    const int regsUsed = (maxReg == -1) ? 0 : maxReg + 1;

    out << "# Total number of registers used, followed by assignment to webs\n";
    out << "registers: " << regsUsed << "\n";

    for (int i = 0; i < W; ++i) {
        const Allocation& a = allocations[i];
        if (a.spilled) out << "M: web"  << i << "\n";
        else           out << "r" << a.regIdx << ": web" << i << "\n";
    }
}