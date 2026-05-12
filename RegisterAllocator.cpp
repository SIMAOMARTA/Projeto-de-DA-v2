#include "RegisterAllocator.h"
#include <iostream>
#include <stack>
#include <algorithm>
#include <set>
#include <limits>


/**
 * Executa o algoritmo greedy de coloração descrito na figura 9 do enunciado.
 *
 * Usa um "subgrafo de trabalho" representado pelos graus atuais de cada nó
 * (workDegree) e pelo conjunto de nós ainda activos (active). Não modifica
 * o InterferenceGraph original.
 *
 * Complexidade: O(W^2) onde W = número de webs.
 */
std::vector<RegisterAllocator::Allocation>
RegisterAllocator::allocate(const InterferenceGraph& graph,
                            const AlgorithmConfig&    config)
{
    const int N = config.numRegisters;
    const int W = graph.numNodes();

    // Resultado final: indexed by webId
    std::vector<Allocation> result(W);
    for (int i = 0; i < W; ++i) {
        result[i].webId   = i;
        result[i].regIdx  = -1;
        result[i].spilled = false;
    }

    if (W == 0) return result;

    // Graus no subgrafo de trabalho (diminuem à medida que removemos nós)
    std::map<int,int> workDegree;
    for (int i = 0; i < W; ++i)
        workDegree[i] = graph.degree(i);

    // Nós ainda activos (não removidos)
    std::set<int> active;
    for (int i = 0; i < W; ++i)
        active.insert(i);

    // Pilha de nós a colorir (fase 2)
    std::stack<int> colourStack;

    // Ids dos nós spilled (ordem de spill)
    std::vector<int> spilled;

    //SIMPLIFICACAO
    while (!active.empty()) {
        bool removedAny = true;

        // Tenta remover todos os nós com grau < N
        while (removedAny) {
            removedAny = false;
            for (auto it = active.begin(); it != active.end(); ) {
                int id = *it;
                if (workDegree[id] < N) {
                    colourStack.push(id);
                    // Atualiza graus dos vizinhos
                    for (int nb : graph.neighbors(id)) {
                        if (active.count(nb))
                            workDegree[nb]--;
                    }
                    it = active.erase(it);
                    removedAny = true;
                } else {
                    ++it;
                }
            }
        }

        // Se ainda há nós ativos e todos têm grau >= N → spill
        if (!active.empty()) {
            int victim = selectSpillCandidate(workDegree, active);
            spilled.push_back(victim);
            result[victim].spilled = true;
            // Actualiza vizinhos
            for (int nb : graph.neighbors(victim)) {
                if (active.count(nb))
                    workDegree[nb]--;
            }
            active.erase(victim);
        }
    }

    //COLORIR
    // Mapa id → cor atribuída (só para nós não spilled)
    std::map<int,int> colour;

    while (!colourStack.empty()) {
        int id = colourStack.top();
        colourStack.pop();

        // Cores usadas pelos vizinhos já coloridos
        std::set<int> usedColours;
        for (int nb : graph.neighbors(id)) {
            auto it = colour.find(nb);
            if (it != colour.end())
                usedColours.insert(it->second);
        }

        // Atribui a cor mais baixa disponível (0-based = r0, r1, ...)
        int chosen = 0;
        while (usedColours.count(chosen))
            ++chosen;

        colour[id] = chosen;
        result[id].regIdx = chosen;
    }

    // Verifica se a coloração usou mais cores do que registos disponíveis
    // (não deveria acontecer com o algoritmo correcto, mas verificamos)
    for (int i = 0; i < W; ++i) {
        if (!result[i].spilled && result[i].regIdx >= N) {
            // Força spill — situação anómala
            result[i].spilled = true;
            result[i].regIdx  = -1;
        }
    }

    // Aviso se houve spills
    if (!spilled.empty()) {
        std::cerr << "AVISO: alocação aos " << N
                  << " registos disponíveis não foi possível sem spilling.\n"
                  << "       " << spilled.size() << " web(s) alocada(s) a memória.\n";
    }

    return result;
}


/**
 * Heurística: spilla o nó com maior grau no subgrafo de trabalho.
 * Nós com mais interferências são os que mais dificultam a coloração.
 */
int RegisterAllocator::selectSpillCandidate(const std::map<int,int>& workDegree,
                                            const std::set<int>&     active)
{
    int best    = *active.begin();
    int bestDeg = -1;
    for (int id : active) {
        int deg = workDegree.at(id);
        if (deg > bestDeg) {
            bestDeg = deg;
            best    = id;
        }
    }
    return best;
}


// printResult
/**
 * Imprime o resultado no formato pedido pelo enunciado (secção 3.5).
 *
 * Se todos os nós foram spilled, o número de registos usados é 0 e
 * todos aparecem com "M:".
 */
void RegisterAllocator::printResult(const InterferenceGraph&      graph,
                                    const std::vector<Allocation>& allocations,
                                    std::ostream&                  out)
{
    const auto& webs = graph.getWebs();
    const int   W    = (int)webs.size();

    //webs
    out << "# Total number of webs followed by the listing of the program points of each one\n";
    out << "# program points in each web are sorted in ascending order\n";
    out << "webs: " << W << "\n";
    for (int i = 0; i < W; ++i)
        out << "web" << i << ": " << webs[i].toString() << "\n";

    // Conta registos realmente usados
    int maxReg = -1;
    for (const auto& a : allocations)
        if (!a.spilled && a.regIdx > maxReg)
            maxReg = a.regIdx;

    bool allSpilled = (maxReg == -1);
    int  regsUsed   = allSpilled ? 0 : maxReg + 1;

    //registos
    out << "# Total number of registers used, followed by assignment to webs\n";
    out << "registers: " << regsUsed << "\n";

    if (allSpilled) {
        // Caso infeasible: todos vão para memória
        for (int i = 0; i < W; ++i)
            out << "M: web" << i << "\n";
    } else {
        // Agrupa por registo para imprimir "rK: webX"
        // (um registo pode conter várias webs que não interferem entre si)
        std::map<int, std::vector<int>> regToWebs;
        for (const auto& a : allocations) {
            if (a.spilled)
                out << "M: web" << a.webId << "\n";
            else
                regToWebs[a.regIdx].push_back(a.webId);
        }
        for (auto& [reg, wids] : regToWebs) {
            std::sort(wids.begin(), wids.end());
            for (int wid : wids)
                out << "r" << reg << ": web" << wid << "\n";
        }
    }
}