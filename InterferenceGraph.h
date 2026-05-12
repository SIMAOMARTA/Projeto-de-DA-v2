#ifndef INTERFERENCE_GRAPH_H
#define INTERFERENCE_GRAPH_H

#include "Web.h"
#include "LiveRange.h"
#include <vector>
#include <set>
#include <map>
#include <string>

/**
 * @file InterferenceGraph.h
 * @brief Grafo de interferência para alocação de registos.
 *
 * Responsabilidades:
 *  1. recebe os LiveRanges do parser
 *  2. interseção live ranges que se sobreponham
 *  3. determinar dois nós (webs) que têm aresta a interferirem
 *
 * Complexidade: O(W^2 * L) onde W = nº de webs, L = nº médio de linhas por web.
 */


class InterferenceGraph {
public:
    /**
     * @brief Constrói o grafo a partir de uma lista de LiveRanges.
     *
     * Algoritmo usado:
     *  1. Agrupar os live ranges por nome de variável
     *  2. Para cada grupo, funde-se iterativamente os ranges que se sobrepõem (utilizando o greedy)
     *  3. Para cada par de webs, verifica interferência e adiciona aresta
     *
     * @param ranges Lista de LiveRanges lida pelo Parser.
     */
    explicit InterferenceGraph(const std::vector<LiveRange>& ranges);

    /** @brief Devolve webs. */
    const std::vector<Web>& getWebs() const;

    /**
     * @brief Verifica se dois nós interferem.
     * @param idA  Índice da web A no vetor webs_.
     * @param idB  Índice da web B no vetor webs_.
     */
    bool hasEdge(int idA, int idB) const;

    /**
     * @brief Devolve os vizinhos de um nó.
     * @param id  Índice da web.
     */
    const std::set<int>& neighbors(int id) const;

    /** @brief Grau do nó (número de vizinhos). */
    int degree(int id) const;

    /** @brief Número de nós no grafo. */
    int numNodes() const;

    /** @brief Devolve o grafo no stdout. */
    void print() const;

private:
    std::vector<Web>             webs_;
    std::map<int, std::set<int>> adjList_;   // id -> conjunto de ids vizinhos

    /** @brief 1+2: agrupa e funde live ranges em webs. */
    void buildWebs(const std::vector<LiveRange>& ranges);

    /** @brief 3: constrói as arestas do grafo. */
    void buildEdges();

    /** @brief Adiciona aresta bidirecional entre idA e idB. */
    void addEdge(int idA, int idB);
};

#endif // INTERFERENCE_GRAPH_H