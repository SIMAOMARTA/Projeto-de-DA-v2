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
 *  - kempeWithSpillCap() : coloração de Kempe com limite opcional de spills.
 *  - selectSpillCandidate() : seleção do nó com maior grau de trabalho.
 *  - splitWeb()          : divisão de uma web ao ponto mediano.
 *  - rebuildGraphFromWebs(): reconstrução do grafo após splitting.
 *  - allocate()          : dispatcher principal dos quatro algoritmos.
 *  - printResult()       : escrita do resultado no ficheiro de saída.
 *
 * **Funções locais (file-scope, usadas apenas por @c free):**
 *  - chaitinCandidate()  : seleciona candidato pela razão grau/tamanho.
 *  - kempeChaitin()      : coloração de Kempe com heurística de Chaitin.
 *
 * @section changes Alterações relativamente à versão anterior
 *  - **Bug corrigido (basic)**: a versão anterior forçava TODAS as webs a
 *    ir para memória quando qualquer uma era vertida. Agora apenas as webs
 *    efetivamente derramadas recebem "M"; as restantes mantêm o seu registo.
 *    Foi também adicionado o @c return em falta no bloco @c basic, que
 *    impedia o retorno antecipado correto.
 *  - **Metadados (spilling)**: allocate() devolve agora uma string que
 *    detalha, por ordem de seleção, qual a web escolhida para spill, a sua
 *    variável e o grau que motivou a escolha.
 *  - **Metadados (splitting)**: os metadados indicam qual a web original que
 *    foi dividida, em que linha a divisão ocorreu e os índices das duas webs
 *    derivadas no grafo final.
 */

#include "RegisterAllocator.h"
#include <iostream>
#include <sstream>
#include <stack>
#include <algorithm>
#include <set>
#include <map>
#include <numeric>

/**
 * @brief Coloração greedy de Kempe com limite opcional de spills.
 *
 * @details
 * ### Estruturas de estado
 *  - @c workDegree : grau efetivo de cada nó enquanto os nós são removidos
 *    da fase ativa. Decrementado para os vizinhos de cada nó removido.
 *  - @c active     : conjunto de nós ainda não removidos do grafo de trabalho.
 *  - @c colourStack: pilha dos nós removidos com grau < N (serão coloridos).
 *
 * ### Fase de simplificação
 *  1. Percorre @c active em ciclo interno: se @c workDegree[n] < N, empilha
 *     @p n, decrementa os workDegrees dos seus vizinhos e remove-o de @c active.
 *  2. Se o ciclo interno não remove nenhum nó mas @c active não está vazio,
 *     todos os nós ativos têm grau >= N (situação de "spill obrigatório"):
 *     - Verifica se atingiu @p spillCap; se sim, define @p feasible = @c false
 *       e retorna antecipadamente.
 *     - Caso contrário, seleciona o candidato com maior grau via
 *       selectSpillCandidate(), marca-o como @c spilled e remove-o de @c active.
 *
 * ### Fase de coloração
 * Desempilha um nó de cada vez e atribui-lhe a menor cor inteira não usada
 * por nenhum vizinho já colorido (armazenado em @c colour). Se a menor cor
 * disponível for >= N, o nó é marcado como @c spilled também nesta fase
 * (grafos muito densos em que mesmo após remoções o grau é >= N).
 *
 * @param graph      Grafo de interferência.
 * @param N          Número de registos físicos disponíveis.
 * @param spillCap   Máximo de spills permitidos na simplificação; @c -1 = ilimitado.
 * @param feasible   Saída: @c true se @p spillCap não foi excedido.
 * @param spillLog   Saída opcional: lista de (webId, grau) das webs vertidas
 *                   durante a simplificação, por ordem de seleção.
 * @return           Vetor de Allocation, um por web.
 *
 * @par Complexidade
 * O(W²) onde W = número de webs, dominado pelo ciclo de simplificação que
 * pode precisar de iterar sobre todos os nós para cada remoção.
 */

std::vector<RegisterAllocator::Allocation> RegisterAllocator::kempeWithSpillCap(const InterferenceGraph& graph, int N,
                                                                                int spillCap, bool& feasible,
                                                                                std::vector<std::pair<int,int>>* spillLog){
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
            // Todos os nós restantes têm grau >= N: spill obrigatório
            if (spillCap >= 0 && spillCount >= spillCap) {
                feasible = false;
                return result;
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

/**
 * @brief Seleciona o candidato a spill com maior grau de trabalho atual.
 *
 * @details
 * Itera sobre o conjunto @p active e retorna o id do nó com o valor mais
 * elevado em @p workDegree. Remover o nó mais conectado maximiza a
 * redução de grau nos seus vizinhos, tornando o grafo mais esparso e
 * aumentando a probabilidade de coloração com N cores nas iterações seguintes.
 *
 * Em caso de empate no grau de trabalho, é selecionado o nó com menor id
 * (determinismo pela ordem de iteração do @c std::set).
 *
 * @param workDegree  Mapa id → grau de trabalho atual dos nós ativos.
 * @param active      Conjunto de ids dos nós ainda não removidos.
 * @return            Id do nó com maior grau de trabalho.
 *
 * @pre @p active não deve estar vazio.
 *
 * @par Complexidade
 * O(|active|) — iteração linear sobre o conjunto.
 */

int RegisterAllocator::selectSpillCandidate(const std::map<int,int>& workDegree, const std::set<int>&     active){

    int best = *active.begin(), bestDeg = -1;
    for (int id : active) {
        int deg = workDegree.at(id);
        if (deg > bestDeg) { bestDeg = deg; best = id; }
    }

    return best;
}

/**
 * @brief Divide uma web em duas metades no ponto mediano das suas linhas.
 *
 * @details
 * Algoritmo:
 *  1. Extrai o vetor de linhas ordenadas do @c lineSet_ da web
 *     (já está ordenado por ser um @c std::set<int>).
 *  2. Calcula @c mid = size/2; se @c mid == 0 (web com uma única linha),
 *     força @c mid = 1 para evitar uma segunda metade vazia.
 *  3. A primeira metade recebe as linhas com índice < @c mid;
 *     a segunda recebe as linhas com índice >= @c mid.
 *  4. Distribui os pontos de programa das ranges originais pelas duas
 *     metades de acordo com o conjunto de linhas a que pertencem.
 *  5. Se @c pts1 ficar vazio (caso degenerado), copia @c pts2 para @c pts1.
 *     Se @c pts2 ficar vazio, adiciona o último ponto de @c pts1 a @c pts2.
 *  6. Cria dois LiveRanges com nomes @c varName_s0 e @c varName_s1 e
 *     devolve as duas webs com id=-1 (serão renumeradas pelo reconstrutor).
 *
 * @param web        Web a dividir.
 * @param splitLine  Saída: primeira linha da segunda metade (ponto de corte).
 * @return           Par @c {primeiraMetade, segundaMetade}.
 *
 * @par Complexidade
 * O(P log P) onde P = total de pontos da web, dominado pela construção
 * dos dois @c std::set de linhas.
 */

std::pair<Web,Web> RegisterAllocator::splitWeb(const Web& web, int& splitLine){

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

/**
 * @brief Reconstrói um InterferenceGraph a partir de uma lista de webs modificada.
 *
 * @details
 * Percorre o vetor @p webs e extrai todas as LiveRanges de cada web via
 * Web::getRanges(). O conjunto resultante de ranges é passado ao construtor
 * principal do InterferenceGraph, que refaz as fases buildWebs() (fusão
 * de overlaps) e buildEdges() (determinação de interferências).
 *
 * Este método é chamado pelo algoritmo @c splitting após cada divisão de
 * web, para garantir que o grafo de interferência reflita o novo conjunto
 * de webs (incluindo as webs derivadas) antes de tentar a coloração.
 *
 * @param webs  Lista atual de webs (pode incluir webs derivadas de divisões
 *              anteriores com nomes do tipo @c "i_s0", @c "i_s1").
 * @return      Novo InterferenceGraph reconstruído a partir de @p webs.
 *
 * @par Complexidade
 * O(W² × L), igual ao construtor principal de InterferenceGraph.
 */

InterferenceGraph RegisterAllocator::rebuildGraphFromWebs(const std::vector<Web>& webs){

    std::vector<LiveRange> allRanges;
    for (const auto& w : webs)
        for (const auto& lr : w.getRanges())
            allRanges.push_back(lr);

    return InterferenceGraph(allRanges);
}

/**
 * @brief Seleciona o candidato a spill pela heurística de Chaitin (T2.4).
 *
 * @details
 * A heurística de Chaitin maximiza a razão:
 * @f[
 *   \text{score}(n) = \frac{\text{workDegree}(n)}{|\text{lineSet}(n)|}
 * @f]
 *
 * Intuição:
 *  - **Grau alto** → web com muitos conflitos, difícil de colorir;
 *    eliminá-la liberta mais colorações.
 *  - **Live range grande** → web cara de manter em registo ao longo
 *    de muitas instruções; vertê-la para memória tem menor custo relativo.
 *
 * Em caso de empate no score, é selecionado o primeiro nó encontrado na
 * iteração sobre @p active (menor id pelo determinismo do @c std::set).
 *
 * @param graph       Grafo de interferência (para aceder aos lineSets das webs).
 * @param workDegree  Mapa id → grau de trabalho atual.
 * @param active      Conjunto de nós ainda ativos.
 * @return            Id do nó com maior score grau/tamanho.
 *
 * @par Complexidade
 * O(|active|) — iteração linear sobre o conjunto.
 */

static int chaitinCandidate(const InterferenceGraph&  graph, const std::map<int,int>&  workDegree, const std::set<int>& active){

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

/**
 * @brief Coloração de Kempe com heurística de Chaitin para seleção de spill.
 *
 * @details
 * Estruturalmente idêntico a kempeWithSpillCap() sem limite de spills
 * (@c spillCap = -1), mas usa chaitinCandidate() em vez de
 * selectSpillCandidate() para escolher o nó a deitar para memória na
 * fase de simplificação. Esta substituição implementa a heurística de
 * Chaitin descrita na tarefa T2.4 do enunciado.
 *
 * A fase de coloração é idêntica à de kempeWithSpillCap(): menor cor
 * disponível não usada por nenhum vizinho já colorido.
 *
 * @param graph     Grafo de interferência.
 * @param N         Número de registos físicos disponíveis.
 * @param feasible  Saída: sempre @c true (sem limite de spills).
 * @return          Vetor de Allocation por web.
 *
 * @par Complexidade
 * O(W²) onde W = número de webs.
 */

static std::vector<RegisterAllocator::Allocation> kempeChaitin(const InterferenceGraph& graph, int N, bool& feasible){

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

/**
 * @brief Executa o algoritmo de alocação selecionado e devolve o resultado.
 *
 * @details
 * Seleciona o algoritmo com base em @p config.algorithm e delega na
 * implementação correspondente. Devolve sempre um triplo
 * @c {alocações, grafofinal, metadados}.
 *
 * ### Variante @c basic
 * Chama kempeWithSpillCap() com @c spillCap = -1 (sem limite). As webs
 * não coloríveis recebem @c spilled=true individualmente; as restantes
 * mantêm o registo atribuído. Nenhum metadado é gerado.
 *
 * ### Variante @c spilling
 * Itera @c cap de 0 até K (inclusive). Na primeira iteração em que
 * kempeWithSpillCap() define @p feasible = @c true, retorna. Os metadados
 * listam, por ordem de seleção, o id, nome de variável e grau de cada
 * web vertida. Se nenhum cap de 0 a K for viável, o fallback é devolver
 * todas as webs como spilled.
 *
 * Formato dos metadados (spilling):
 * @code
 * # Spilling metadata [K=<param>]
 * # Format: spilled_web: <webId> var=<name> degree=<d>
 * spilled_count: <n>
 * spilled_web: 2 var=i degree=5
 * @endcode
 *
 * ### Variante @c splitting
 * Tenta primeiro sem modificações. Se falhar, entra no loop de splitting
 * (até K iterações): divide sempre a web com maior grau via splitWeb(),
 * reconstrói o grafo com rebuildGraphFromWebs() e tenta colorir. Os
 * metadados listam cada divisão efectuada. Se K divisões não forem
 * suficientes, faz fallback para spilling ilimitado no grafo com splits.
 *
 * Formato dos metadados (splitting):
 * @code
 * # Splitting metadata [K=<param>]
 * # Format: split: original_var=<name> split_at_line=<line> -> web<A> web<B>
 * splits_count: <n>
 * split: original_var=i split_at_line=9 -> web2 web3
 * @endcode
 *
 * ### Variante @c free (Chaitin)
 * Chama kempeChaitin() que usa a razão grau/tamanho da live range para
 * selecionar candidatos a spill. Nenhum metadado é gerado.
 *
 * @param graph   Grafo de interferência original.
 * @param config  Configuração com número de registos e variante do algoritmo.
 * @return        Triplo @c {alocações, grafo final, metadados}.
 *
 * @par Complexidade
 * O(W²) para basic/spilling/free;
 * O(K × W² × L) para splitting.
 */

std::tuple<std::vector<RegisterAllocator::Allocation>, InterferenceGraph, std::string> RegisterAllocator::allocate(
                            const InterferenceGraph& graph,
                            const AlgorithmConfig&    config){

    const int N = config.numRegisters;
    bool feasible = false;
    std::vector<Allocation> result;
    std::string metadata;

    if (config.algorithm == "basic"){
        result = kempeWithSpillCap(graph, N, -1, feasible);
        return {result, graph, ""};
    }

    if (config.algorithm == "spilling"){
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
        int K = config.algorithmParam;

        /**
         * @brief Registo de uma única operação de divisão de web.
         *
         * Armazena os dados necessários para construir a linha de
         * metadados correspondente no ficheiro de saída.
         */

        struct SplitRecord {
            std::string originalVar;  ///< nome da variável antes do split
            int         splitAtLine;  ///< ponto de corte
            int         newWebA;      ///< indice da primeira metade
            int         newWebB;      ///< Índice da segunda metade
        };
        std::vector<SplitRecord> splitLog;

        // Tentativa inicial sem splits
        result = kempeWithSpillCap(graph, N, 0, feasible);
        if (feasible) return {result, graph, ""};

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
            // caso não haja candidato com grau >= N
            if (candidate == -1 || maxDeg < N) break;

            const Web& candWeb = curGraph.getWebs()[candidate];
            std::string originalVar = candWeb.getVarName();
            int splitLine = 0;
            auto [w1, w2] = splitWeb(candWeb, splitLine);

            // Substitui a web candidata pelas duas metades no vetor atual
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

            // Tenta colorir o grafo com as webs modificadas
            InterferenceGraph newGraph = rebuildGraphFromWebs(currentWebs);
            result = kempeWithSpillCap(newGraph, N, 0, feasible);

            // As duas metades ocupam posições consecutivas a partir de candidate
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

/**
 * @brief Escreve o resultado da alocação num stream de saída.
 *
 * @details
 * ### Secção de webs
 * Lista todas as webs do grafo final (incluindo webs derivadas de splits)
 * no formato exigido pelo enunciado. Os pontos de programa de cada web
 * são escritos em ordem ascendente (garantido por Web::toString()).
 *
 * ### Secção de registos
 * Determina o número de registos efetivamente usados como @c maxReg + 1
 * (onde @c maxReg é o maior índice de registo atribuído; 0 se todas as
 * webs foram vertidas). Escreve uma linha por web: @c "rN: webK" se
 * alocada ou @c "M: webK" se vertida para memória.
 *
 * ### Secção de metadados
 * Se @p metadata não estiver vazia, é acrescentada uma secção adicional
 * com os metadados complementares, separada por uma linha em branco.
 * O formato é o descrito em allocate() para cada variante.
 *
 * @param graph        Grafo de interferência final.
 * @param allocations  Resultado de allocate() — um Allocation por web.
 * @param out          Stream de saída.
 * @param metadata     String de metadados complementares (vazia por defeito).
 *
 * @par Complexidade
 * O(W × P), onde W = número de webs, P = pontos médios por web.
 */

void RegisterAllocator::printResult(const InterferenceGraph& graph, const std::vector<Allocation>& allocations,
                                    std::ostream& out, const std::string& metadata){

    const auto& webs = graph.getWebs();
    const int   W    = (int)webs.size();

    out << "# Total number of webs followed by the listing of the program points of each one\n";
    out << "# program points in each web are sorted in ascending order\n";
    out << "webs: " << W << "\n";
    for (int i = 0; i < W; ++i)
        out << "web" << i << ": " << webs[i].toString() << "\n";

    // Determina o número de registos efetivamente utilizados
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

    if (!metadata.empty()) {
        out << "\n";
        out << metadata;
    }
}
