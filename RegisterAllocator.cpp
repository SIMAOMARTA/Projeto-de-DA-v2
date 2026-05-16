/**
 * @file RegisterAllocator.cpp
 * @brief Implementação do alocador de registos.
 * Contém os quatro algoritmos de alocação (basic, spilling, splitting, free),
 * os auxiliares de selecção de candidatos, divisão de webs e organização
 * do resultado.
 *
 * @section changes Alterações relativamente à versão anterior
 *  - **Bug corrigido (basic)**: a versão anterior forçava TODAS as webs a
 *    ir para memória quando qualquer uma era vertida. Agora apenas as webs
 *    efetivamente derramadas recebem "M"; as restantes mantêm o seu registo.
 *    Foi também adicionado o `return` em falta no bloco `basic`, que impedia
 *    o retorno antecipado correto.
 *  - **Output complementar (spilling)**: a função `allocate()` devolve agora
 *    uma string de metadados que detalha, por ordem de seleção, qual a web
 *    escolhida para spill, a sua variável e o grau que motivou a escolha.
 *  - **Output complementar (splitting)**: os metadados indicam qual a web
 *    original que foi dividida, em que linha a divisão ocorreu e os índices
 *    das duas webs derivadas no grafo final — num formato legível por máquina.
 */

#include "RegisterAllocator.h"
#include <iostream>
#include <sstream>
#include <stack>
#include <algorithm>
#include <set>
#include <map>
#include <numeric>

// ============================================================
//  kempeWithSpillCap
// ============================================================

/**
 * @brief Simplificação e coloração greedy (Kempe) com limite opcional de spills.
 *
 * @details
 * ### Simplificação
 *  1. Percorre todos os nós ativos: se workDegree[n] < N, remove n para
 *     a pilha e decrementa o workDegree dos seus vizinhos ativos.
 *  2. Se o ciclo não removeu nenhum nó E ainda há nós ativos, seleciona
 *     o candidato a spill (selectSpillCandidate) e marca-o como spilled.
 *     - Se @p spillCap >= 0 e já atingimos @p spillCap spills, define
 *       @p feasible = false e retorna antecipadamente (sem coloração).
 *
 * ### Coloração
 * Desempilha um nó de cada vez; recolhe as cores dos vizinhos já coloridos
 * e atribui a menor cor não usada. Se nenhuma cor < N estiver disponível,
 * o nó é marcado como spilled também nesta fase (grafos muito densos).
 *
 * ### Log de spills
 * Cada vez que um nó é escolhido como candidato a spill na fase de
 * simplificação, o seu id e grau são registados em @p spillLog para que
 * o chamador possa incluir metadados no ficheiro de saída.
 *
 * @param graph      Grafo de interferência.
 * @param N          Número de registos físicos disponíveis.
 * @param spillCap   Limite máximo de spills; -1 significa ilimitado.
 * @param feasible   Saída: @c true se a alocação respeita @p spillCap.
 * @param spillLog   Saída opcional: lista de (webId, grau) das webs vertidas
 *                   durante a simplificação (por ordem de seleção).
 * @return           Vetor de Allocation, um por web.
 *
 * @par Complexidade
 * O(W²) onde W = número de webs.
 */
std::vector<RegisterAllocator::Allocation>
RegisterAllocator::kempeWithSpillCap(const InterferenceGraph& graph,
                                     int N, int spillCap,
                                     bool& feasible,
                                     std::vector<std::pair<int,int>>* spillLog)
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

    // --- Fase de simplificação -------------------------------------------
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
            // Verificar limite de spills antes de selecionar mais um
            if (spillCap >= 0 && spillCount >= spillCap) {
                feasible = false;
                return result;
            }
            int victim = selectSpillCandidate(workDegree, active);
            result[victim].spilled = true;
            if (spillLog)
                spillLog->emplace_back(victim, workDegree[victim]);
            spillCount++;
            for (int nb : graph.neighbors(victim))
                if (active.count(nb)) workDegree[nb]--;
            active.erase(victim);
        }
    }

    // --- Fase de coloração -----------------------------------------------
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

// ============================================================
//  selectSpillCandidate
// ============================================================

/**
 * @brief Seleciona o candidato a spill com maior grau de trabalho atual.
 *
 * @details
 * Ao remover o nó mais conectado, o grafo torna-se mais esparso e a
 * coloração seguinte é mais provável de ser viável.
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

// ============================================================
//  splitWeb
// ============================================================

/**
 * @brief Divide uma web em dois no ponto mediano das linhas.
 *
 * @details
 * A divisão é feita no índice médio do vetor ordenado de linhas.
 *
 *  - Linhas com índice < média → primeira metade (sufixo @c _s0).
 *  - Linhas com índice >= média → segunda metade (sufixo @c _s1).
 *
 * **Casos importantes**:
 *  - Se média == 0 (web com apenas uma linha), força mid=1.
 *  - Se uma das metades ficar vazia, copia os pontos da outra.
 *
 * @param web        Web a dividir.
 * @param splitLine  Saída: primeira linha da segunda metade (ponto de corte).
 * @return           Par {primeira metade, segunda metade} ambas com id=-1.
 *
 * @par Complexidade
 * O(P log P) onde P = total de pontos da web.
 */
std::pair<Web,Web> RegisterAllocator::splitWeb(const Web& web, int& splitLine)
{
    const std::set<int>& lineSet = web.getLineSet();
    std::vector<int> lines(lineSet.begin(), lineSet.end());

    size_t mid = lines.size() / 2;
    if (mid == 0) mid = 1;

    splitLine = lines[(int)mid];  // primeira linha da segunda metade

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

    LiveRange lr1(web.getVarName() + "_s0", pts1);
    LiveRange lr2(web.getVarName() + "_s1", pts2);
    return { Web(-1, lr1), Web(-1, lr2) };
}

// ============================================================
//  rebuildGraphFromWebs
// ============================================================

/**
 * @brief Reconstrói um InterferenceGraph a partir de uma lista de webs.
 *
 * @details
 * Extrai todos os LiveRanges de cada web e passa-os ao construtor do
 * InterferenceGraph, que refaz buildWebs() + buildEdges().
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

// ============================================================
//  chaitinCandidate  (função local)
// ============================================================

/**
 * @brief Seleciona o candidato a spill pela heurística de Chaitin.
 *
 * @details
 * Maximiza a razão @c grau_trabalho / tamanho_live_range:
 *  - **Grau alto** → web difícil de colorir.
 *  - **Live range grande** → web cara de manter em memória.
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

// ============================================================
//  kempeChaitin  (função local)
// ============================================================

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

// ============================================================
//  allocate  — dispatcher principal
// ============================================================

/**
 * @brief Executa o algoritmo de alocação selecionado e devolve resultado +
 *        grafo final + string de metadados complementares.
 *
 * @details
 * ### basic
 * Executa kempeWithSpillCap com spillCap=-1 (ilimitado). Apenas as webs
 * que não conseguiram ser coloridas recebem "M" — as restantes mantêm o
 * seu registo. Se alguma web foi vertida, emite aviso no stderr.
 *
 * ### spilling
 * Itera cap de 0 até K (inclusive). Na primeira iteração viável, retorna.
 * Os metadados indicam quantas webs foram vertidas, quais os seus ids,
 * nomes de variável e grau de interferência no momento da seleção.
 *
 * Formato dos metadados (spilling):
 * @code
 * # Spilling metadata [K=<param>]
 * # Format: spilled_web: <webId> var=<name> degree=<d>
 * spilled_count: <n>
 * spilled_web: 2 var=i degree=5
 * @endcode
 *
 * ### splitting
 * Tenta primeiro sem modificações. Se falhar, entra no loop de splitting
 * (até K iterações), dividindo sempre a web com maior grau. Os metadados
 * indicam cada operação de divisão: qual a web original (por id e nome de
 * variável), em que linha ocorreu o corte, e quais os ids das webs derivadas
 * no grafo final.
 *
 * Formato dos metadados (splitting):
 * @code
 * # Splitting metadata [K=<param>]
 * # Format: split: original_var=<name> split_at_line=<line> -> web<A> web<B>
 * splits_count: <n>
 * split: original_var=i split_at_line=9 -> web2 web3
 * @endcode
 *
 * ### free (Chaitin)
 * Usa a razão grau/tamanho da live range para selecionar candidatos a spill.
 *
 * @param graph   Grafo de interferência inicial.
 * @param config  Configuração com algoritmo e número de registos.
 * @return        Tuplo {alocações, grafo final, string de metadados}.
 *
 * @par Complexidade
 * O(W²) para basic/spilling/free;
 * O(K × W² × L) para splitting.
 */
std::tuple<std::vector<RegisterAllocator::Allocation>,
           InterferenceGraph,
           std::string>
RegisterAllocator::allocate(const InterferenceGraph& graph,
                            const AlgorithmConfig&    config)
{
    const int N = config.numRegisters;
    bool feasible = false;
    std::vector<Allocation> result;
    std::string metadata;

    // ------------------------------------------------------------------
    // basic — grafo não se altera
    // ------------------------------------------------------------------
    if (config.algorithm == "basic") {
        result = kempeWithSpillCap(graph, N, -1, feasible);
        // CORREÇÃO: apenas as webs efetivamente vertidas recebem M;
        // as outras mantêm o registo atribuído. (A versão anterior forçava
        // TODAS a ir para memória se qualquer uma fosse vertida — errado.)
        return {result, graph, ""};
    }

    // ------------------------------------------------------------------
    // spilling — grafo não se altera
    // ------------------------------------------------------------------
    if (config.algorithm == "spilling") {
        int K = config.algorithmParam;
        std::vector<std::pair<int,int>> spillLog;  // (webId, grau)

        for (int cap = 0; cap <= K; ++cap) {
            spillLog.clear();
            result = kempeWithSpillCap(graph, N, cap, feasible, &spillLog);
            if (feasible) break;
        }

        // Constrói metadados de spilling (formato legível por máquina)
        const auto& webs = graph.getWebs();
        std::ostringstream oss;
        oss << "# Spilling metadata [K=" << K << "]\n";
        oss << "# Format: spilled_web: <webId> var=<name> degree=<d>\n";
        oss << "spilled_count: " << spillLog.size() << "\n";
        for (auto& [wid, deg] : spillLog) {
            std::string varName = (wid < (int)webs.size())
                                  ? webs[wid].getVarName() : "?";
            oss << "spilled_web: " << wid
                << " var=" << varName
                << " degree=" << deg << "\n";
        }
        metadata = oss.str();

        if (!feasible) {
            std::cerr << "ERRO: alocacao com spilling ate K=" << K
                      << " nao foi possivel.\n";
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

    // ------------------------------------------------------------------
    // splitting — grafo pode crescer com cada divisão de web
    // ------------------------------------------------------------------
    if (config.algorithm == "splitting") {
        int K = config.algorithmParam;

        // Estrutura para rastrear cada divisão efetuada
        struct SplitRecord {
            std::string originalVar;  // nome da variável original
            int         splitAtLine;  // primeira linha da segunda metade
            int         newWebA;      // índice no grafo final
            int         newWebB;      // índice no grafo final
        };
        std::vector<SplitRecord> splitLog;

        // Tentativa inicial sem splits
        result = kempeWithSpillCap(graph, N, 0, feasible);
        if (feasible) return {result, graph, ""};

        std::vector<Web> currentWebs = graph.getWebs();

        for (int splitsDone = 0; splitsDone < K; ++splitsDone) {
            InterferenceGraph curGraph = rebuildGraphFromWebs(currentWebs);

            // Candidato: web com maior grau no grafo atual
            int candidate = -1, maxDeg = -1;
            for (int i = 0; i < curGraph.numNodes(); ++i) {
                if (curGraph.degree(i) > maxDeg) {
                    maxDeg = curGraph.degree(i);
                    candidate = i;
                }
            }
            if (candidate == -1 || maxDeg < N) break;

            const Web& candWeb = curGraph.getWebs()[candidate];
            std::string originalVar = candWeb.getVarName();
            int splitLine = 0;
            auto [w1, w2] = splitWeb(candWeb, splitLine);

            // Substitui a web candidata pelas duas metades
            std::vector<Web> newWebs;
            newWebs.reserve(currentWebs.size() + 1);
            for (int i = 0; i < (int)currentWebs.size(); ++i) {
                if (i == candidate) {
                    newWebs.push_back(std::move(w1));
                    newWebs.push_back(std::move(w2));
                } else {
                    newWebs.push_back(currentWebs[i]);
                }
            }
            currentWebs = std::move(newWebs);

            // Tenta colorir o grafo com as webs modificadas
            InterferenceGraph newGraph = rebuildGraphFromWebs(currentWebs);
            result = kempeWithSpillCap(newGraph, N, 0, feasible);

            // Regista a divisão (índices no novo grafo, após reconstrução)
            // Após rebuildGraphFromWebs o grafo pode renumerar as webs;
            // os dois fragmentos ficam sempre em posições consecutivas
            // correspondentes à posição do candidato no vetor currentWebs.
            int idxA = candidate;
            int idxB = candidate + 1;
            splitLog.push_back({originalVar, splitLine, idxA, idxB});

            if (feasible) {
                // Constrói metadados e retorna o grafo com splits
                std::ostringstream oss;
                oss << "# Splitting metadata [K=" << K << "]\n";
                oss << "# Format: split: original_var=<name> "
                       "split_at_line=<line> -> web<A> web<B>\n";
                oss << "splits_count: " << splitLog.size() << "\n";
                for (auto& s : splitLog)
                    oss << "split: original_var=" << s.originalVar
                        << " split_at_line=" << s.splitAtLine
                        << " -> web" << s.newWebA
                        << " web" << s.newWebB << "\n";
                return {result, newGraph, oss.str()};
            }
        }

        // Fallback: spilling ilimitado sobre o grafo com splits feitos
        InterferenceGraph fallback = rebuildGraphFromWebs(currentWebs);
        result = kempeWithSpillCap(fallback, N, -1, feasible);

        std::ostringstream oss;
        oss << "# Splitting metadata [K=" << K << "] (fallback to spilling)\n";
        oss << "# Format: split: original_var=<name> "
               "split_at_line=<line> -> web<A> web<B>\n";
        oss << "splits_count: " << splitLog.size() << "\n";
        for (auto& s : splitLog)
            oss << "split: original_var=" << s.originalVar
                << " split_at_line=" << s.splitAtLine
                << " -> web" << s.newWebA
                << " web" << s.newWebB << "\n";
        return {result, fallback, oss.str()};
    }

    // ------------------------------------------------------------------
    // free (Chaitin) — grafo não se altera
    // ------------------------------------------------------------------
    if (config.algorithm == "free") {
        result = kempeChaitin(graph, N, feasible);
        return {result, graph, ""};
    }

    return {result, graph, ""};
}

// ============================================================
//  printResult
// ============================================================

/**
 * @brief Imprime o resultado da alocação para um stream de saída.
 *
 * @details
 * ### Secção de webs
 * Lista todas as webs do grafo final (incluindo webs derivadas de splits)
 * com os seus pontos de programa em ordem ascendente.
 *
 * ### Secção de registos
 * Indica o número de registos efetivamente usados (pode ser inferior a
 * config.numRegisters) e, para cada web, o registo atribuído ou "M" se
 * foi vertida para memória.
 *
 * ### Secção de metadados (opcional)
 * Se @p metadata não estiver vazia (algoritmos spilling e splitting),
 * é escrita numa secção final do ficheiro no formato descrito em
 * RegisterAllocator::allocate(). O formato é legível por máquina:
 * linhas com palavras-chave específicas e separadores '=', '->', ' '.
 *
 * @param graph        Grafo de interferência final.
 * @param allocations  Resultado de allocate().
 * @param out          Stream de saída.
 * @param metadata     String de metadados complementares (pode estar vazia).
 *
 * @par Complexidade
 * O(W × P) onde W = número de webs, P = pontos médios por web.
 */
void RegisterAllocator::printResult(const InterferenceGraph&       graph,
                                    const std::vector<Allocation>&  allocations,
                                    std::ostream&                   out,
                                    const std::string&              metadata)
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

    // Secção de metadados complementares (spilling / splitting)
    if (!metadata.empty()) {
        out << "\n";
        out << metadata;
    }
}