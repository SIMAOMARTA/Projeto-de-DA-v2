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
 *
 * @section changes Alterações relativamente à versão anterior
 *  - `allocate()` devolve agora um triplo
 *    `{alocações, grafo final, string de metadados}` em vez de um par.
 *    Os metadados descrevem as operações de spill/split efectuadas e são
 *    escritos no ficheiro de saída por `printResult()`.
 *  - `printResult()` aceita um quarto parâmetro opcional `metadata`
 *    (valor por defeito `""`; compatível com chamadas existentes sem metadados).
 *  - `splitWeb()` aceita um segundo parâmetro de saída `splitLine` que
 *    recebe a primeira linha da segunda metade após a divisão.
 *  - `kempeWithSpillCap()` aceita um quinto parâmetro opcional `spillLog`
 *    (ponteiro nulo por defeito) onde são registados os ids e graus das
 *    webs selecionadas para spill durante a simplificação.
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

class RegisterAllocator {
public:
    /**
     * @struct Allocation
     * @brief Resultado da alocação de uma única web.
     */
    struct Allocation {
        int  webId   = -1;    ///< ID da web alocada.
        int  regIdx  = -1;    ///< Índice do registo físico (0-based); -1 se spilled.
        bool spilled = false; ///< @c true se a web foi enviada para memória.
    };

    /**
     * @brief Aloca registos para todas as webs em @p graph.
     *
     * @details
     * Seleciona internamente o algoritmo indicado em @p config e devolve
     * um triplo com:
     *  1. O vetor de alocações (uma por web do grafo final).
     *  2. O grafo de interferência final utilizado na coloração.
     *     Para @c basic, @c spilling e @c free é uma cópia do grafo original;
     *     para @c splitting pode ter mais nós (webs divididas).
     *  3. Uma string de metadados complementares em formato legível por máquina.
     *     Vazia para @c basic e @c free.
     *     Para @c spilling: lista das webs vertidas (id, variável, grau).
     *     Para @c splitting: lista das divisões efectuadas (variável, linha de
     *     corte, índices das webs derivadas).
     *
     * @param graph   Grafo de interferência original.
     * @param config  Número de registos e variante do algoritmo.
     * @return        Triplo {alocações, grafo final, metadados}.
     *
     * @par Complexidade
     * O(W²) para basic/spilling/free; O(K × W² × L) para splitting.
     */
    static std::tuple<std::vector<Allocation>, InterferenceGraph, std::string>
        allocate(const InterferenceGraph& graph,
                 const AlgorithmConfig&   config);

    /**
     * @brief Escreve o resultado da alocação em @p out.
     *
     * @details
     * Produz duas secções no formato exigido pelo enunciado:
     *  1. Lista de webs com os seus pontos de programa.
     *  2. Número de registos usados e atribuição de cada web a um registo ou `M`.
     *
     * Se @p metadata não estiver vazia, é acrescentada uma terceira secção com
     * os metadados complementares do algoritmo executado (spilling/splitting).
     *
     * @param graph        Grafo de interferência final.
     * @param allocations  Resultado de allocate().
     * @param out          Stream de saída.
     * @param metadata     String de metadados (pode estar vazia; valor por defeito "").
     *
     * @par Complexidade
     * O(W × P) onde W = número de webs, P = pontos médios por web.
     */
    static void printResult(const InterferenceGraph&       graph,
                            const std::vector<Allocation>& allocations,
                            std::ostream&                  out,
                            const std::string&             metadata = "");

private:
    /**
     * @brief Coloração greedy de Kempe com limite opcional de spills.
     *
     * @details
     * Executa as fases de simplificação e coloração do algoritmo de Kempe.
     * Na fase de simplificação, sempre que todos os nós restantes têm grau >= N,
     * seleciona o candidato com maior grau de trabalho como spill.
     * Se @p spillCap >= 0 e o número de spills atingir esse limite, define
     * @p feasible = @c false e retorna antecipadamente (sem coloração).
     * Se @p spillLog não for nulo, regista em cada spill o par (webId, grau).
     *
     * @param graph      Grafo de interferência.
     * @param N          Número de registos físicos disponíveis.
     * @param spillCap   Máximo de spills; negativo = ilimitado.
     * @param feasible   Saída: @c true se a alocação respeita @p spillCap.
     * @param spillLog   Saída opcional: lista de (webId, grau) das webs vertidas
     *                   durante a simplificação, por ordem de seleção.
     *                   Se @c nullptr, o log não é preenchido.
     *
     * @par Complexidade
     * O(W²) onde W = número de webs.
     */
    static std::vector<Allocation>
        kempeWithSpillCap(const InterferenceGraph&          graph,
                          int                               N,
                          int                               spillCap,
                          bool&                             feasible,
                          std::vector<std::pair<int,int>>*  spillLog = nullptr);

    /**
     * @brief Seleciona o candidato a spill pelo maior grau de trabalho atual.
     *
     * @param workDegree  Mapa id -> grau de trabalho atual de cada nó ativo.
     * @param active      Conjunto de ids dos nós ainda não removidos do grafo.
     * @return            Id do nó com maior grau de trabalho.
     *
     * @par Complexidade
     * O(|active|)
     */
    static int selectSpillCandidate(const std::map<int,int>& workDegree,
                                    const std::set<int>&     active);

    /**
     * @brief Divide uma web ao ponto mediano das suas linhas de programa.
     *
     * @details
     * As linhas são ordenadas e divididas ao índice médio. A primeira metade
     * recebe o sufixo @c _s0 no nome da variável, a segunda @c _s1.
     * Em @p splitLine é devolvida a primeira linha da segunda metade, que
     * corresponde ao ponto de programa onde o compilador deve inserir as
     * instruções de save/restore necessárias à fronteira da divisão.
     *
     * @param web        Web a dividir; deve ter pelo menos um ponto de programa.
     * @param splitLine  Saída: primeira linha da segunda metade (ponto de corte).
     * @return           Par {primeira metade (_s0), segunda metade (_s1)}.
     *
     * @par Complexidade
     * O(P log P) onde P = total de pontos da web.
     */
    static std::pair<Web,Web> splitWeb(const Web& web, int& splitLine);

    /**
     * @brief Reconstrói um grafo de interferência a partir de uma lista de webs.
     *
     * @details
     * Extrai todos os LiveRanges de cada web e constrói um novo
     * InterferenceGraph (buildWebs + buildEdges). Garante consistência
     * do grafo após múltiplas divisões de webs em iterações do splitting.
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