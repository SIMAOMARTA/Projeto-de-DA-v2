/**
 * @file RegisterAllocator.h
 * @brief Declaração do alocador de registos por coloração de grafos.
 *
 * Implementa quatro variantes do algoritmo de Kempe/Chaitin para coloração
 * do grafo de interferência e consequente alocação de registos físicos:
 *
 * | Variante    | Tarefa | Descrição |
 * |-------------|--------|-----------|
 * | @c basic    | T2.1 | Coloração greedy de Kempe sem limite de spills. |
 * | @c spilling | T2.2 | Coloração com até K webs em spill (itera de 0 a K). |
 * | @c splitting| T2.3 | Divisão iterativa de webs de alto grau antes da coloração. |
 * | @c free     | T2.4 | Heurística de Chaitin: razão grau/tamanho da live range. |
 *
 * @details
 * Todos os métodos são estáticos: RegisterAllocator é uma classe utilitária
 * sem estado (não é instanciável). O ponto de entrada principal é allocate(),
 * que seleciona internamente a variante e devolve um triplo com os
 * resultados, o grafo final e metadados textuais.
 *
 * @see InterferenceGraph
 * @see AlgorithmConfig
 */

#ifndef REGISTER_ALLOCATOR_H
#define REGISTER_ALLOCATOR_H

#include "InterferenceGraph.h"
#include "AlgorithmConfig.h"
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include <ostream>

/**
 * @class RegisterAllocator
 * @brief Classe utilitária estática que implementa os algoritmos de alocação de registos.
 *
 * O fluxo típico de utilização é:
 * @code
 *   InterferenceGraph g(ranges);
 *   AlgorithmConfig   cfg = Parser::parseRegisters("registers.txt");
 *   auto [allocs, finalGraph, meta] = RegisterAllocator::allocate(g, cfg);
 *   RegisterAllocator::printResult(finalGraph, allocs, std::cout, meta);
 * @endcode
 */

class RegisterAllocator {
public:

    /**
     * @struct Allocation
     * @brief Resultado da alocação de uma única web.
     *
     * Após a execução de allocate(), existe um Allocation por cada web
     * no grafo final. O campo @c spilled determina se a web foi vertida
     * para memória (@c true) ou se foi atribuída a um registo (@c false).
     */

    struct Allocation {
        int  webId   = -1;    ///< Id da web alocada (igual ao índice no grafo).
        int  regIdx  = -1;    ///< Índice do registo físico (0-based); -1 se spilled.
        bool spilled = false; ///< @c true se a web foi enviada para memória ("M").
    };

 /**
      * @brief Aloca registos para todas as webs em @p graph e devolve o resultado.
      *
      * @details
      * Seleciona internamente o algoritmo indicado em @p config e retorna
      * um triplo @c {alocações, grafofinal, metadados}:
      *
      * 1. **alocações**: vetor de Allocation, um por web do grafo final.
      * Para algoritmos sem modificação do grafo (basic, spilling, free),
      * tem exatamente tantos elementos quantas as webs do grafo original.
      * Para splitting, pode ter mais (as webs divididas originam novas entradas).
      *
      * 2. **grafo final**: para basic, spilling e free é equivalente ao grafo
      * original (cópia sem modificações); para splitting pode conter mais
      * nós correspondentes às webs derivadas das divisões.
      *
      * 3. **metadados**: string de texto em formato legível por máquina.
      * - Vazia para basic e free.
      * - Para spilling:
      * @code
      * # Spilling metadata [K=<param>]
      * # Format: spilled_web: <webId> var=<name> degree=<d>
      * spilled_count: <n>
      * spilled_web: 2 var=i degree=5
      * @endcode
      * - Para splitting:
      * @code
      * # Splitting metadata [K=<param>]
      * # Format: split: original_var=<name> split_at_line=<line> -> web<A> web<B>
      * splits_count: <n>
      * split: original_var=i split_at_line=9 -> web2 web3
      * @endcode
      *
      * @param graph   Grafo de interferência original.
      * @param config  Configuração com número de registos e variante do algoritmo.
      * @return        Triplo @c {alocações, grafo final, metadados}.
      *
      * @par Complexidade
      * O(W²) para basic/spilling/free; O(K × W² × L) para splitting.
      * W = número de webs, L = linhas médias por web, K = parâmetro do algoritmo.
      */

    static std::tuple<std::vector<Allocation>, InterferenceGraph, std::string> allocate(const InterferenceGraph& graph,
                                                                                        const AlgorithmConfig& config);

    /**
     * @brief Escreve o resultado da alocação num stream de saída.
     *
     * @details
     * Produz um ficheiro de saída com três secções:
     *
     * **Secção de webs** — lista todas as webs do grafo final com os seus
     * pontos de programa em ordem ascendente:
     * @code
     * webs: 3
     * web0: 1+,2,3,4,5,6-
     * web1: 9+,10,11,12,13,14-,20+
     * web2: 7+,8,9,10-
     * @endcode
     *
     * **Secção de registos** — indica o número de registos efetivamente
     * usados e, para cada web, o registo atribuído ou @c M:
     * @code
     * registers: 2
     * r0: web0
     * r0: web1
     * r1: web2
     * @endcode
     *
     * **Secção de metadados** (opcional) — escrita apenas se @p metadata
     * não estiver vazio; contém informação complementar sobre as operações
     * de spill/split no formato descrito em allocate().
     *
     * @param graph        Grafo de interferência final (pode ter mais webs
     *                     que o original no caso de splitting).
     * @param allocations  Resultado de allocate() — um Allocation por web.
     * @param out          Stream de saída (ficheiro ou std::cout).
     * @param metadata     String de metadados complementares (valor por defeito @c "").
     *
     * @par Complexidade
     * O(W × P), onde W = número de webs, P = pontos médios por web.
     */

    static void printResult(const InterferenceGraph& graph, const std::vector<Allocation>& allocations, std::ostream& out,
                            const std::string& metadata = "");

private:

    /**
     * @brief Coloração greedy de Kempe com limite opcional de spills.
     *
     * @details
     * Executa as fases de simplificação e coloração do algoritmo de Kempe:
     *
     * **Fase de simplificação:**
     *  1. Percorre os nós ativos: se workDegree[n] < N, empilha @p n e
     *     decrementa o workDegree dos seus vizinhos ativos.
     *  2. Se o ciclo não remove nenhum nó E ainda há nós ativos, todos têm
     *     grau >= N: seleciona o candidato a spill (maior grau de trabalho)
     *     via selectSpillCandidate() e marca-o como spilled.
     *  3. Se @p spillCap >= 0 e o limite de spills é atingido antes de
     *     esvaziar @c active, define @p feasible = @c false e retorna
     *     antecipadamente sem executar a fase de coloração.
     *
     * **Fase de coloração:**
     *  Desempilha um nó de cada vez e atribui-lhe a menor cor disponível
     *  (não usada por nenhum vizinho já colorido). Se nenhuma cor < N estiver
     *  disponível (grafo muito denso), o nó é marcado como spilled também
     *  nesta fase.
     *
     * **Log de spills:**
     *  Se @p spillLog não for @c nullptr, cada web selecionada para spill
     *  na fase de simplificação tem o seu par (webId, grau) registado no
     *  vetor, por ordem de seleção. Usado pelo algoritmo @c spilling para
     *  construir os metadados de saída.
     *
     * @param graph      Grafo de interferência.
     * @param N          Número de registos físicos disponíveis (número de cores).
     * @param spillCap   Máximo de spills permitidos; @c -1 = ilimitado.
     * @param feasible   Saída: @c true se a alocação respeita @p spillCap.
     * @param spillLog   Saída opcional: lista de (webId, grau) das webs
     *                   vertidas na simplificação, por ordem de seleção.
     *                   @c nullptr por defeito — log não é preenchido.
     * @return           Vetor de Allocation, um por web.
     *
     * @par Complexidade
     * O(W²) onde W = número de webs (dominado pelo ciclo de simplificação).
     */

    static std::vector<Allocation> kempeWithSpillCap(const InterferenceGraph& graph, int N, int spillCap, bool& feasible,
                                                    std::vector<std::pair<int,int>>* spillLog = nullptr);

    /**
     * @brief Seleciona o candidato a spill com maior grau de trabalho atual.
     *
     * @details
     * Itera sobre @p active e retorna o id do nó com o valor mais alto em
     * @p workDegree. Ao remover o nó mais conectado, o grafo fica mais
     * esparso, aumentando a probabilidade de a coloração subsequente ser
     * viável com N cores.
     *
     * Em caso de empate no grau, é selecionado o nó encontrado primeiro
     * na ordem de iteração do @c std::set (menor id).
     *
     * @param workDegree  Mapa id → grau de trabalho atual de cada nó ativo.
     * @param active      Conjunto de ids dos nós ainda não removidos.
     * @return            Id do nó com maior grau de trabalho.
     *
     * @par Complexidade
     * O(|active|)
     */

    static int selectSpillCandidate(const std::map<int,int>& workDegree, const std::set<int>& active);

    /**
     * @brief Divide uma web em duas metades no ponto mediano das suas linhas.
     *
     * @details
     * As linhas são ordenadas (já estão no @c std::set<int>) e divididas
     * ao índice médio @c mid = size/2 (com @c mid >= 1 para evitar
     * metades vazias em webs com uma única linha):
     *  - Linhas com índice < @c mid → primeira metade, variável com sufixo @c _s0.
     *  - Linhas com índice >= @c mid → segunda metade, variável com sufixo @c _s1.
     *
     * Os pontos de programa de cada range original são distribuídos entre
     * as duas metades de acordo com o conjunto de linhas a que pertencem.
     * Se uma das metades ficar sem pontos, é preenchida com os pontos da outra
     * (caso degenerado de web com uma única linha).
     *
     * O parâmetro de saída @p splitLine recebe a primeira linha da segunda
     * metade, que corresponde ao ponto de programa onde o compilador deve
     * inserir as instruções de save/restore na fronteira da divisão.
     *
     * @param web        Web a dividir (deve ter pelo menos um ponto).
     * @param splitLine  Saída: primeira linha da segunda metade (ponto de corte).
     * @return           Par @c {primeiraMetade (_s0), segundaMetade (_s1)}.
     *
     * @par Complexidade
     * O(P log P) onde P = total de pontos de programa da web.
     */

    static std::pair<Web,Web> splitWeb(const Web& web, int& splitLine);

    /**
     * @brief Reconstrói um InterferenceGraph a partir de uma lista de webs modificada.
     *
     * @details
     * Extrai todas as LiveRanges de cada web (via Web::getRanges()) e
     * constrói um novo InterferenceGraph chamando o construtor principal
     * (buildWebs() + buildEdges()). Garante a consistência do grafo após
     * múltiplas divisões de webs em iterações sucessivas do algoritmo
     * @c splitting.
     *
     * @param webs  Lista de webs atuais (pode incluir webs derivadas de divisões).
     * @return      Novo InterferenceGraph reconstruído a partir de @p webs.
     *
     * @par Complexidade
     * O(W² × L), igual ao construtor de InterferenceGraph.
     */

    static InterferenceGraph rebuildGraphFromWebs(const std::vector<Web>& webs);
};

#endif // REGISTER_ALLOCATOR_H
