/**
 * @file RegisterAllocator.cpp
 * @brief Implementação do alocador de registos.
 * Contém os quatro algoritmos de alocação (basic, spilling, splitting, free),
 * os auxiliares de seleção de candidatos, divisão de webs e organização
 * do resultado.
 */

#include "RegisterAllocator.h"
#include <iostream>
#include <stack>
#include <algorithm>
#include <set>
#include <map>
#include <numeric>

/**
 * @brief Simplificação e coloração.
 * @details
 * ### Simplificação
 *  1. Percorre todos os nós ativos: se workDegree[n] < N, remove n para
 *     a pilha e decrementa o workDegree dos seus vizinhos ativos.
 *  2. Se o ciclo não removeu nenhum nó E ainda há nós ativos, seleciona
 *     o candidato a spill (selectSpillCandidate) e marca-o como spilled.
 *     - Se spillCap >= 0 e já atingimos spillCap spills, define-se
 *       feasible=false e retorna-se antecipadamente.
 *
 * ### Coloração
 * Desempilha um nó de cada vez. Para cada nó, recolhe as cores usadas
 * pelos seus vizinhos já coloridos e atribui a menor cor não usada.
 * Se a menor cor disponível for >= N, o nó é adicionalmente marcado
 * como spilled (pode acontecer em grafos muito densos).
 *
 * @param graph      Grafo de interferência.
 * @param N          Número de registos físicos disponíveis.
 * @param spillCap   Limite máximo de spills, se for negativo é ilimitado.
 * @param feasible   Saída: @c true se a alocação respeita @p spillCap.
 * @return           Vetor de Allocation, um por web.
 *
 * @par Complexidade
 * O(W²) onde W = número de webs.
 */
std::vector<RegisterAllocator::Allocation>
RegisterAllocator::kempeWithSpillCap(const InterferenceGraph& graph,
                                     int N, int spillCap,
                                     bool& feasible,
                                     bool silent)
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

    // Simplificação
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

    // Coloração
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
    if (!silent && spillCount > 0 && spillCap < 0) {
        std::cerr << "AVISO: alocacao aos " << N
                  << " registos nao foi possivel sem spilling.\n"
                  << "       " << spillCount << " web(s) alocada(s) a memoria.\n";
    }
    return result;
}

/**
 * @brief Seleciona o candidato a spill com maior grau.
 *
 * @details
 * Percorre todos os nós ativos e regista o nó com o maior grau
 * de trabalho. Em caso de empate, o nó com id menor é escolhido porque
 * o set está ordenado.
 *
 * Ao remover o nó mais conectado, o grafo tende a tornar-se mais esparso
 * e a coloração seguinte é mais provável de ser viável.
 *
 * @param workDegree  Mapa id -> grau de trabalho atual.
 * @param active      Conjunto de nós ainda ativos.
 * @return            Id do nó selecionado.
 *
 * @par Complexidade
 * O(|active|)
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

/**
 * @brief Divide uma web em dois no ponto mediano das linhas.
 * @details
 * A divisão é feita no índice médio do vetor ordenado de linhas.
 *
 * Linhas com índice < média, corresponde à primeira metade.
 * Linhas com índice >= média, corresponde à segunda metade.
 *
 * **Casos importantes**:
 * - Se média == 0 (web com apenas uma linha), força mid=1, colocando a
 *   única linha na primeira metade.
 * - Se uma das metades ficar vazia após a distribuição, copia os pontos
 *   da outra (evita um LiveRange sem pontos, que seria inválido).
 *
 * @param web  Web a dividir.
 * @return     Par {primeira metade, segunda metade} ambas com id=-1.
 *
 * @par Complexidade
 * O(P log P) onde P = total de pontos da web.
 */
std::pair<Web,Web> RegisterAllocator::splitWeb(const Web& web)
{
    const std::set<int>& lineSet = web.getLineSet();
    std::vector<int> lines(lineSet.begin(), lineSet.end());

    size_t mid = lines.size() / 2;
    if (mid == 0) mid = 1;

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

    if (pts1.empty()) pts1 = pts2;
    if (pts2.empty()) pts2.push_back(pts1.back());

    LiveRange lr1(web.getVarName() + "__split0__", pts1);
    LiveRange lr2(web.getVarName() + "__split1__", pts2);
    return { Web(-1, lr1), Web(-1, lr2) };
}

/**
 * @brief Reconstrói um InterferenceGraph a partir de uma lista de webs.
 *
 * @details
 * Extrai todos os LiveRanges de cada web e passa-os ao construtor do
 * InterferenceGraph, que refaz buildWebs() + buildEdges().
 * Garante consistência do grafo após múltiplas divisões de webs.
 *
 * @param webs  Lista de webs atuais.
 * @return      Novo InterferenceGraph reconstruído.
 *
 * @par Complexidade
 * O(W² × L), igual ao construtor do InterferenceGraph.
 */
InterferenceGraph RegisterAllocator::rebuildGraphFromWebs(const std::vector<Web>& webs)
{
    std::vector<LiveRange> allRanges;
    for (const auto& w : webs)
        for (const auto& lr : w.getRanges())
            allRanges.push_back(lr);
    return InterferenceGraph(allRanges);
}


/**
 * @brief Seleciona o candidato a spill pela heurística de Chaitin.
 *
 * @details
 * Maximiza a razão @c grau_trabalho / tamanho_live_range:
 *  - **Grau alto**, web muito conectada, difícil de colorir.
 *  - **Live range grande**, web fraca para guardar em memória.
 *
 * Um valor elevado indica uma web "boa" de manter em registo e
 * "fraca" de deitar para memória — bom candidato a spill.
 *
 * @param graph       Grafo de interferência.
 * @param workDegree  Mapa id -> grau de trabalho atual.
 * @param active      Conjunto de nós ainda ativos.
 * @return            Id do melhor candidato a spill.
 *
 * @par Complexidade
 * O(|active|)
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
 * @brief Coloração de Kempe com heurística de Chaitin para seleção de spill.
 *
 * @details
 * Idêntico ao fluxo de kempeWithSpillCap (sem limite de spills), mas
 * usa chaitinCandidate() em vez de selectSpillCandidate() para escolher
 * o nó a deitar para memória na fase de simplificação.
 *
 * @param graph     Grafo de interferência.
 * @param N         Registos físicos disponíveis.
 * @param feasible  Saída: sempre @c true (sem limite de spills).
 * @return          Vetor de Allocation por web.
 *
 * @par Complexidade
 * O(W²) onde W = número de webs.
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

/**
 * @brief Executa o algoritmo de alocação de registos selecionado na configuração.
 *
 * @details
 * ### basic
 * Chama kempeWithSpillCap com spillCap=-1 (ilimitado). Se não for possível
 * colorir sem spills, os nós excedentes são marcados como spilled e é
 * emitido um aviso no stderr.
 *
 * ### spilling
 * Itera cap de 0 até K (inclusive) silenciosamente; emite uma única
 * mensagem no stderr caso nenhuma iteração seja viável.
 *
 * ### splitting
 *  1. Tenta primeiro sem modificações (cap=0, silencioso).
 *  2. Se falhar, entra no loop de splitting (até K iterações):
 *     - Seleciona a web com maior grau; se tiver só uma linha procura
 *       a próxima candidata divisível.
 *     - Divide-a com splitWeb() e reconstrói o grafo.
 *     - Tenta coloração do novo grafo com cap=0.
 *  3. Se após K splits ainda falhar, usa spills ilimitados (fallback)
 *     sobre o grafo mais refinado obtido.
 *
 * ### free (Chaitin)
 * Usa kempeChaitin() que seleciona candidatos a spill pela razão
 * grau_trabalho / tamanho_da_live_range.
 *
 * @param graph   Grafo de interferência original.
 * @param config  Configuração com algoritmo e número de registos.
 * @return        AllocResult com alocações **e** o grafo efetivamente
 *                colorido (pode diferir do original em splitting).
 *
 * @par Complexidade
 * O(W²) para basic/spilling/free;
 * O(K × W² × L) para splitting.
 */
RegisterAllocator::AllocResult
RegisterAllocator::allocate(const InterferenceGraph& graph,
                            const AlgorithmConfig&    config)
{
    const int N = config.numRegisters;
    bool feasible = false;

    // --- basic ---
    if (config.algorithm == "basic") {
        auto allocs = kempeWithSpillCap(graph, N, -1, feasible);
        // warning emitted inside kempeWithSpillCap when spillCap<0 and spills>0
        return { allocs, graph };
    }

    // --- spilling ---
    if (config.algorithm == "spilling") {
        int K = config.algorithmParam;
        for (int cap = 0; cap <= K; ++cap) {
            auto allocs = kempeWithSpillCap(graph, N, cap, feasible, /*silent=*/true);
            if (feasible) return { allocs, graph };
        }
        // Completely infeasible within K spills — spill everything
        std::cerr << "ERRO: alocacao com spilling ate K=" << K
                  << " nao foi possivel. Todas as webs alocadas a memoria.\n";
        const int W2 = graph.numNodes();
        std::vector<Allocation> all(W2);
        for (int i = 0; i < W2; ++i) { all[i].webId = i; all[i].spilled = true; }
        return { all, graph };
    }

    // --- splitting ---
    if (config.algorithm == "splitting") {
        int K = config.algorithmParam;

        // First attempt: no splits needed
        {
            auto allocs = kempeWithSpillCap(graph, N, 0, feasible, /*silent=*/true);
            if (feasible) return { allocs, graph };
        }

        std::vector<Web> currentWebs = graph.getWebs();
        int splitsDone = 0;

        while (splitsDone < K) {
            InterferenceGraph curGraph = rebuildGraphFromWebs(currentWebs);

            // Find best candidate: highest degree AND more than one line (splittable)
            int candidate = -1, maxDeg = -1;
            for (int i = 0; i < curGraph.numNodes(); ++i) {
                int d = curGraph.degree(i);
                bool splittable = (int)curGraph.getWebs()[i].getLineSet().size() > 1;
                if (splittable && d > maxDeg) { maxDeg = d; candidate = i; }
            }
            // No splittable web with edges remains — further splitting cannot help
            if (candidate == -1 || maxDeg == 0) break;

            auto [w1, w2] = splitWeb(curGraph.getWebs()[candidate]);

            std::vector<Web> newWebs;
            newWebs.reserve((size_t)curGraph.numNodes() + 1);
            for (int i = 0; i < curGraph.numNodes(); ++i) {
                if (i == candidate) { newWebs.push_back(w1); newWebs.push_back(w2); }
                else                { newWebs.push_back(curGraph.getWebs()[i]); }
            }
            currentWebs = std::move(newWebs);
            ++splitsDone;

            InterferenceGraph newGraph = rebuildGraphFromWebs(currentWebs);
            auto allocs = kempeWithSpillCap(newGraph, N, 0, feasible, /*silent=*/true);
            if (feasible) return { allocs, newGraph };  // ← correct graph returned
        }

        // Fallback: unlimited spilling on the best graph produced so far
        std::cerr << "AVISO: splitting ate K=" << K
                  << " nao foi suficiente. Recorrendo a spilling ilimitado.\n";
        InterferenceGraph fallback = rebuildGraphFromWebs(currentWebs);
        auto allocs = kempeWithSpillCap(fallback, N, -1, feasible);
        return { allocs, fallback };  // ← correct (split) graph returned
    }

    // --- free (Chaitin) ---
    if (config.algorithm == "free") {
        auto allocs = kempeChaitin(graph, N, feasible);
        return { allocs, graph };
    }

    return { {}, graph };
}


/**
 * @brief Imprime o resultado da alocação para um stream de saída.
 *
 * @details
 * Usa @c result.finalGraph para obter as webs efetivamente coloridas
 * (que pode incluir webs novas criadas por splitting) e
 * @c result.allocations para as atribuições de registo correspondentes.
 *
 * Percorre as allocations duas vezes:
 *  1. Para calcular o número de registos efetivamente usados
 *     (maxReg+1, onde maxReg é o maior regIdx não-spilled).
 *  2. Para emitir as linhas de atribuição ("rN: webM" ou "M: webM").
 *
 * @param result  Resultado de allocate() (contém grafo final e alocações).
 * @param out     Stream de saída.
 *
 * @par Complexidade
 * O(W × P) onde W = número de webs, P = pontos médios por web.
 */
void RegisterAllocator::printResult(const AllocResult& result,
                                    std::ostream&      out)
{
    const auto& webs        = result.finalGraph.getWebs();
    const auto& allocations = result.allocations;
    const int   W           = (int)webs.size();

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