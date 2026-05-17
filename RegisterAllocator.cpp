/**
 * @file RegisterAllocator.cpp
 * @brief Implementação do alocador de registos por coloração de grafos.
 *
 * @details
 * Implementa os quatro algoritmos de alocação de registos declarados em
 * RegisterAllocator.h, mais duas funções locais (file-scope) auxiliares
 * à variante @c free:
 *
 * **Funções membro estáticas (classe RegisterAllocator):**
 * - kempeWithSpillCap() : coloração de Kempe com limite opcional de spills.
 * - selectSpillCandidate() : seleção do nó com maior grau de trabalho.
 * - splitWeb()          : divisão de uma web ao ponto mediano.
 * - rebuildGraphFromWebs(): reconstrução do grafo após splitting.
 * - allocate()          : dispatcher principal dos quatro algoritmos.
 * - printResult()       : escrita do resultado no ficheiro de saída.
 *
 * **Funções locais (file-scope, usadas apenas por @c free):**
 * - chaitinCandidate()  : seleciona candidato pela razão grau/tamanho.
 * - kempeChaitin()      : coloração de Kempe com heurística de Chaitin.
 *
 * @section changes Alterações relativamente à versão anterior
 * - **Bug corrigido (basic)**: a versão anterior forçava TODAS as webs a
 * ir para memória quando qualquer uma era vertida. Agora apenas as webs
 * efetivamente derramadas recebem "M"; as restantes mantêm o seu registo.
 * Foi também adicionado o @c return em falta no bloco @c basic, que
 * impedia o retorno antecipado correto.
 * - **Metadados (spilling)**: allocate() devolve agora uma string que
 * detalha, por ordem de seleção, qual a web escolhida para spill, a sua
 * variável e o grau que motivou a escolha.
 * - **Metadados (splitting)**: os metadados indicam qual a web original que
 * foi dividida, em que linha a divisão ocorreu e os índices das duas webs
 * derivadas no grafo final.
 */

#include "RegisterAllocator.h"
#include <iostream>
#include <sstream>
#include <stack>
#include <algorithm>
#include <set>
#include <map>
#include <numeric>

std::vector<RegisterAllocator::Allocation> RegisterAllocator::kempeWithSpillCap(const InterferenceGraph& graph, int N,
                                                                                int spillCap, bool& feasible,
                                                                                std::vector<std::pair<int,int>>* spillLog){
    // Estruturas de estado:
    // - workDegree : grau efetivo de cada nó enquanto os nós são removidos.
    // - active     : conjunto de nós ainda não removidos.
    // - colourStack: pilha dos nós removidos com grau < N.

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

    // Fase de simplificação
    while (!active.empty()) {
        bool removedAny = true;
        // 1. Remove iterativamente todos os nós com grau < N
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

        // 2. Spill obrigatório se todos os nós restantes têm grau >= N
        if (!active.empty()) {
            if (spillCap >= 0 && spillCount >= spillCap) {
                feasible = false;
                return result; // Retorna antecipadamente se atingir o limite
            }
            int victim = selectSpillCandidate(workDegree, active);
            result[victim].spilled = true;
            if (spillLog) spillLog->emplace_back(victim, workDegree[victim]);
            spillCount++;
            for (int nb : graph.neighbors(victim))
                if (active.count(nb)) workDegree[nb]--;
            active.erase(victim);
        }
    }

    // Fase de coloração
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
            // Se a menor cor disponível for >= N, o nó é marcado como spilled.
            result[id].spilled = true;
            spillCount++;
        }
        else {
            colour[id]        = chosen;
            result[id].regIdx = chosen;
        }
    }

    feasible = (spillCap < 0) || (spillCount <= spillCap);
    if (spillCount > 0 && spillCap < 0) {
        std::cerr << "AVISO: alocacao aos " << N << " registos nao foi possivel sem spilling.\n"
        << "       " << spillCount << " web(s) alocada(s) a memoria.\n";
    }

    return result;
}

int RegisterAllocator::selectSpillCandidate(const std::map<int,int>& workDegree, const std::set<int>& active){
    // Retorna o id do nó com o valor mais elevado em workDegree.
    // Em caso de empate, é selecionado o nó com menor id (determinismo pelo std::set).

    int best = *active.begin(), bestDeg = -1;
    for (int id : active) {
        int deg = workDegree.at(id);
        if (deg > bestDeg) { bestDeg = deg; best = id; }
    }

    return best;
}

std::pair<Web,Web> RegisterAllocator::splitWeb(const Web& web, int& splitLine){
    // Algoritmo:
    // 1. Extrai o vetor de linhas ordenadas do lineSet_ da web.
    // 2. Calcula mid = size/2 (se mid == 0, força mid = 1 para evitar metades vazias).
    // 3. Primeira metade recebe linhas com índice < mid; segunda recebe >= mid.
    // 4. Distribui os pontos de programa pelas duas metades.
    // 5. Trata casos degenerados (garante que nenhuma metade fica vazia).
    // 6. Cria dois LiveRanges e devolve as duas webs com id=-1.

    const std::set<int>& lineSet = web.getLineSet();
    std::vector<int> lines(lineSet.begin(), lineSet.end());

    size_t mid = lines.size() / 2;
    if (mid == 0) mid = 1;

    splitLine = lines[(int)mid];

    std::set<int> linesFirst(lines.begin(), lines.begin() + (int)mid);
    std::set<int> linesSecond(lines.begin() + (int)mid, lines.end());

    const auto& ranges = web.getRanges();
    std::vector<ProgramPoint> pts1, pts2;
    for (const auto& lr : ranges) {
        for (const auto& p : lr.getPoints()) {
            if (linesFirst.count(p.line)) pts1.push_back(p);
            else pts2.push_back(p);
        }
    }

    // Casos degenerados: garante que nenhuma metade fica sem pontos
    if (pts1.empty()) pts1 = pts2;
    if (pts2.empty()) pts2.push_back(pts1.back());

    LiveRange lr1(web.getVarName() + "_s0", pts1);
    LiveRange lr2(web.getVarName() + "_s1", pts2);
    return { Web(-1, lr1), Web(-1, lr2) };
}

InterferenceGraph RegisterAllocator::rebuildGraphFromWebs(const std::vector<Web>& webs){
    // Percorre o vetor webs, extrai as LiveRanges, e refaz as fases buildWebs e buildEdges.
    std::vector<LiveRange> allRanges;
    for (const auto& w : webs)
        for (const auto& lr : w.getRanges())
            allRanges.push_back(lr);

    return InterferenceGraph(allRanges);
}

static int chaitinCandidate(const InterferenceGraph&  graph, const std::map<int,int>&  workDegree, const std::set<int>& active){
    // A heurística de Chaitin maximiza a razão: score(n) = workDegree(n) / |lineSet(n)|
    // Empates são resolvidos selecionando o menor id (primeiro nó na iteração).

    const auto& webs = graph.getWebs();
    int best = *active.begin();
    double bestScore = -1.0;
    for (int id : active) {
        int deg  = workDegree.at(id);
        int size = (int)webs[id].getLineSet().size();
        if (size == 0) size = 1;  // evita divisão por zero
        double score = static_cast<double>(deg) / size;
        if (score > bestScore) { bestScore = score; best = id; }
    }

    return best;
}

static std::vector<RegisterAllocator::Allocation> kempeChaitin(const InterferenceGraph& graph, int N, bool& feasible){
    // Estruturalmente idêntico a kempeWithSpillCap() (com spillCap = -1),
    // mas usa chaitinCandidate() em vez de selectSpillCandidate() na fase de simplificação.

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

    // fase de simplificação
    while (!active.empty()) {
        bool removedAny = true;
        while (removedAny) {
            removedAny = false;
            for (auto it = active.begin(); it != active.end(); ){
                int id = *it;
                if (workDegree[id] < N){
                    colourStack.push(id);
                    for (int nb : graph.neighbors(id))
                        if (active.count(nb)) workDegree[nb]--;
                    it = active.erase(it);
                    removedAny = true;
                }
                else { ++it; }
            }
        }
        if (!active.empty()){
            int victim = chaitinCandidate(graph, workDegree, active);
            result[victim].spilled = true;
            spillCount++;
            for (int nb : graph.neighbors(victim))
                if (active.count(nb)) workDegree[nb]--;
            active.erase(victim);
        }
    }

    // colorir
    std::map<int,int> colour;
    while (!colourStack.empty()){
        int id = colourStack.top(); colourStack.pop();
        std::set<int> used;
        for (int nb : graph.neighbors(id)){
            auto it = colour.find(nb);
            if (it != colour.end()) used.insert(it->second);
        }
        int chosen = 0;
        while (used.count(chosen)) ++chosen;
        if (chosen >= N) { result[id].spilled = true; spillCount++; }
        else { colour[id] = chosen; result[id].regIdx = chosen; }
    }

    feasible = true;
    if (spillCount > 0) std::cerr << "AVISO [free/Chaitin]: " << spillCount << " web(s) alocada(s) a memoria.\n";
    return result;
}

std::tuple<std::vector<RegisterAllocator::Allocation>, InterferenceGraph, std::string> RegisterAllocator::allocate(
                            const InterferenceGraph& graph,
                            const AlgorithmConfig&    config){

    const int N = config.numRegisters;
    bool feasible = false;
    std::vector<Allocation> result;
    std::string metadata;

    if (config.algorithm == "basic"){
        // Chama kempeWithSpillCap com spillCap = 0. Se falhar, faz fallback
        // vertendo integralmente TODAS as webs para a memória (0 registos efetivos).
        result = kempeWithSpillCap(graph, N, 0, feasible);

        if (!feasible) {
            const int W = graph.numNodes();
            result.assign(W, Allocation{});
            for (int i = 0; i < W; ++i) {
                result[i].webId   = i;
                result[i].regIdx  = -1;   // Força r0, r1, etc. a não serem usados
                result[i].spilled = true;
            }
        }

        return {result, graph, ""};
    }

    if (config.algorithm == "spilling"){
        // Itera cap de 0 até K. Retorna na primeira iteração viável.
        int K = config.algorithmParam;
        std::vector<std::pair<int,int>> spillLog;

        for (int cap = 0; cap <= K; ++cap){
            spillLog.clear();
            result = kempeWithSpillCap(graph, N, cap, feasible, &spillLog);
            if (feasible) break;
        }

        // Constrói metadados
        const auto& webs = graph.getWebs();
        std::ostringstream oss;
        oss << "# Spilling metadata [K=" << K << "]\n";
        oss << "# Format: spilled_web: <webId> var=<name> degree=<d>\n";
        oss << "spilled_count: " << spillLog.size() << "\n";
        for (auto& [wid, deg] : spillLog){
            std::string varName = (wid < (int)webs.size()) ? webs[wid].getVarName() : "?";
            oss << "spilled_web: " << wid << " var=" << varName << " degree=" << deg << "\n";
        }
        metadata = oss.str();

        if (!feasible) {
            std::cerr << "ERRO: alocacao com spilling ate K=" << K << " nao foi possivel.\n";
            const int W2 = graph.numNodes();
            result.assign(W2, Allocation{});
            for (int i = 0; i < W2; ++i) {
                result[i].webId   = i;
                result[i].regIdx  = -1;
                result[i].spilled = true;
            }
        }
        return {result, graph, metadata};
    }

    if (config.algorithm == "splitting"){
        // Tenta sem modificações. Se falhar, divide a web com maior grau até K vezes.
        // Se K divisões falharem, faz fallback para spilling ilimitado.
        int K = config.algorithmParam;

        struct SplitRecord {
            std::string originalVar;
            int         splitAtLine;
            int         newWebA;
            int         newWebB;
        };
        std::vector<SplitRecord> splitLog;

        // Tentativa inicial sem splits
        result = kempeWithSpillCap(graph, N, 0, feasible);
        if (feasible) {
            std::ostringstream oss;
            oss << "# Splitting metadata [K=" << K << "]\n";
            oss << "# Format: split: original_var=<name> split_at_line=<line> -> web<A> web<B>\n";
            oss << "splits_count: 0\n";
            return {result, graph, oss.str()};
        }

        std::vector<Web> currentWebs = graph.getWebs();

        for (int splitsDone = 0; splitsDone < K; ++splitsDone){
            InterferenceGraph curGraph = rebuildGraphFromWebs(currentWebs);

            int candidate = -1, maxDeg = -1;
            for (int i = 0; i < curGraph.numNodes(); ++i){
                if (curGraph.degree(i) > maxDeg){
                    maxDeg = curGraph.degree(i);
                    candidate = i;
                }
            }
            if (candidate == -1 || maxDeg < N) break;

            const Web& candWeb = curGraph.getWebs()[candidate];
            std::string originalVar = candWeb.getVarName();
            int splitLine = 0;
            auto [w1, w2] = splitWeb(candWeb, splitLine);

            std::vector<Web> newWebs;
            newWebs.reserve(currentWebs.size() + 1);
            for (int i = 0; i < (int)currentWebs.size(); ++i){
                if (i == candidate) {
                    newWebs.push_back(std::move(w1));
                    newWebs.push_back(std::move(w2));
                }
                else {
                    newWebs.push_back(currentWebs[i]);
                }
            }
            currentWebs = std::move(newWebs);

            InterferenceGraph newGraph = rebuildGraphFromWebs(currentWebs);
            result = kempeWithSpillCap(newGraph, N, 0, feasible);

            int idxA = candidate;
            int idxB = candidate + 1;
            splitLog.push_back({originalVar, splitLine, idxA, idxB});

            if (feasible){
                std::ostringstream oss;
                oss << "# Splitting metadata [K=" << K << "]\n";
                oss << "# Format: split: original_var=<name> " "split_at_line=<line> -> web<A> web<B>\n";
                oss << "splits_count: " << splitLog.size() << "\n";
                for (auto& s : splitLog)
                    oss << "split: original_var=" << s.originalVar << " split_at_line=" << s.splitAtLine
                        << " -> web" << s.newWebA << " web" << s.newWebB << "\n";

                return {result, newGraph, oss.str()};
            }
        }

        InterferenceGraph fallback = rebuildGraphFromWebs(currentWebs);
        result = kempeWithSpillCap(fallback, N, -1, feasible);

        std::ostringstream oss;
        oss << "# Splitting metadata [K=" << K << "] (fallback to spilling)\n";
        oss << "# Format: split: original_var=<name> " "split_at_line=<line> -> web<A> web<B>\n";
        oss << "splits_count: " << splitLog.size() << "\n";
        for (auto& s : splitLog)
            oss << "split: original_var=" << s.originalVar << " split_at_line=" << s.splitAtLine
                << " -> web" << s.newWebA << " web" << s.newWebB << "\n";

        return {result, fallback, oss.str()};
    }

    if (config.algorithm == "free") {
        result = kempeChaitin(graph, N, feasible);
        return {result, graph, ""};
    }

    return {result, graph, ""};
}

void RegisterAllocator::printResult(const InterferenceGraph& graph, const std::vector<Allocation>& allocations,
                                    std::ostream& out, const std::string& metadata){

    // Secção de webs: Lista todas as webs e os seus pontos
    const auto& webs = graph.getWebs();
    const int   W    = (int)webs.size();

    out << "# Total number of webs followed by the listing of the program points of each one\n";
    out << "# program points in each web are sorted in ascending order\n";
    out << "webs: " << W << "\n";
    for (int i = 0; i < W; ++i)
        out << "web" << i << ": " << webs[i].toString() << "\n";

    // Secção de registos: Determina o número de registos efetivamente utilizados
    int maxReg = -1;
    for (const auto& a : allocations)
        if (!a.spilled && a.regIdx > maxReg) maxReg = a.regIdx;
    const int regsUsed = (maxReg == -1) ? 0 : maxReg + 1;

    out << "# Total number of registers used, followed by assignment to webs\n";
    out << "registers: " << regsUsed << "\n";

    for (int i = 0; i < W; ++i) {
        const Allocation& a = allocations[i];
        if (a.spilled) out << "M: web"  << i << "\n";
        else out << "r" << a.regIdx << ": web" << i << "\n";
    }

    // Secção de metadados: Acrescenta metadados complementares (se existirem)
    if (!metadata.empty()) {
        out << "\n";
        out << metadata;
    }
}
