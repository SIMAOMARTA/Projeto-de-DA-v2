/**
 * @file InterferenceGraph.h
 * @brief Declaração do grafo de interferência para alocação de registos.
 *
 * O grafo de interferência é a estrutura central do algoritmo de alocação:
 * os nós são webs e as arestas indicam que dois valores não podem residir
 * no mesmo registo ao mesmo tempo.
 */

#ifndef INTERFERENCE_GRAPH_H
#define INTERFERENCE_GRAPH_H

#include "Web.h"
#include "LiveRange.h"
#include <vector>
#include <set>
#include <map>
#include <string>

/**
 * @class InterferenceGraph
 * @brief Grafo de interferência entre webs de alocação de registos.
 *
 * A construção do grafo é feita em dois passos no construtor:
 *  1. **buildWebs()**: agrupa os LiveRanges por variável e funde os que
 *     se sobrepõem, produzindo um conjunto de webs (nós do grafo).
 *  2. **buildEdges()**: para cada par de webs testa Web::interferesWith()
 *     e, se verdadeiro, adiciona uma aresta bidirecional.
 *
 * A representação interna usa uma lista de adjacência
 * (@c std::map<int, std::set<int>>) indexada pelo id da web.
 */
class InterferenceGraph {
public:
    /**
     * @brief Constrói o grafo a partir de uma lista de LiveRanges.
     *
     * Pipeline completo:
     *  1. Agrupa ranges por nome de variável.
     *  2. Para cada grupo, funde iterativamente ranges sobrepostas (greedy).
     *  3. Renumera os ids das webs de 0 a N-1.
     *  4. Para cada par de webs, verifica interferência e adiciona aresta.
     *
     * @param ranges  Lista de LiveRanges obtida do Parser.
     */
    explicit InterferenceGraph(const std::vector<LiveRange>& ranges);

    /**
     * @brief Devolve a lista de todas as webs (nós) do grafo.
     * @return Referência constante para o vetor de webs.
     */
    const std::vector<Web>& getWebs() const;

    /**
     * @brief Verifica se dois nós interferem.
     * @param idA  Índice da web A no vetor webs_.
     * @param idB  Índice da web B no vetor webs_.
     * @return @c true se os dois nós interferem (aresta presente).
     */
    bool hasEdge(int idA, int idB) const;

    /**
     * @brief Devolve os vizinhos de um nó.
     * @param id  Índice da web.
     * @return    Referência constante para o set de ids vizinhos.
     */
    const std::set<int>& neighbors(int id) const;

    /**
     * @brief Devolve o grau (número de vizinhos) de um nó.
     * @param id  Índice da web.
     * @return    Número de webs com que @p id interfere;
     *            0 se o nó não existir.
     */
    int degree(int id) const;

    /**
     * @brief Número de nós no grafo.
     * @return Número de webs.
     */
    int numNodes() const;

    /**
     * @brief Devolve o grafo no stdout.
     * Cada aresta é impressa apenas uma vez (quando idA < idB).
     */
    void print() const;

private:
    /** @brief Lista de webs (nós do grafo), indexada por id. */
    std::vector<Web>             webs_;

    /** @brief Lista de adjacência: id da web -> conjunto de ids vizinhos. */
    std::map<int, std::set<int>> adjList_;


    /**
     * @brief Passo 1 e 2 da construção: agrupa ranges em webs.
     *
     * Algoritmo greedy por variável:
     *  1. Para cada variável, cria uma web por LiveRange.
     *  2. Itera enquanto houver fusões possíveis:
     *     para cada par (i,j), se se sobrepõem, absorve j em i.
     *  3. Adiciona as webs resultantes ao vetor webs_.
     *  4. Renumera ids de 0 a N-1.
     * @param ranges  Lista de LiveRanges do Parser.
     */
    void buildWebs(const std::vector<LiveRange>& ranges);

    /**
     * @brief Passo 3 da construção: cria as arestas do grafo.
     * Inicializa entradas vazias na adjList_ para todos os nós, depois
     * testa cada par (i,j) com i<j via Web::interferesWith().
     */
    void buildEdges();

    /**
     * @brief Adiciona uma aresta bidirecional entre dois nós.
     * Insere idB no set de adjacência de idA e vice-versa.
     * @param idA  Id do primeiro nó.
     * @param idB  Id do segundo nó.
     */
    void addEdge(int idA, int idB);
};

#endif // INTERFERENCE_GRAPH_H