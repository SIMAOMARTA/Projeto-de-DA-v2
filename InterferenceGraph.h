/**
 * @file InterferenceGraph.h
 * @brief Declaração da classe InterferenceGraph.
 *
 * O grafo de interferência é a estrutura central do alocador de registos.
 * Os seus nós representam webs (uniões de live ranges sobrepostas de uma
 * variável) e as suas arestas ligam webs que interferem — i.e., que não
 * podem partilhar o mesmo registo físico.
 *
 * @details
 * A classe utiliza como estrutura de dados subjacente o @c Graph<int>
 * fornecido nas aulas TP, mapeando cada web ao seu id inteiro. Esta
 * escolha cumpre o requisito de T1.2 do enunciado: "This data structure
 * must be based on the data structure described in the TP lectures for
 * graphs".
 *
 * Estrutura interna:
 *  - @c webs_      : vetor de Web, indexado por id (posição == id).
 *  - @c baseGraph_ : @c Graph<int> com os mesmos ids como informação dos vértices.
 *
 * Os dois contêineres são mantidos sincronizados: um vértice com info @p i
 * em @c baseGraph_ corresponde sempre a @c webs_[i].
 *
 * @see Web
 * @see RegisterAllocator
 */

#ifndef INTERFERENCEGRAPH_H
#define INTERFERENCEGRAPH_H

#include <vector>
#include "LiveRange.h"
#include "Web.h"
#include "Graph.h"

/**
 * @class InterferenceGraph
 * @brief Grafo de interferência entre webs de variáveis.
 *
 * Recebe um conjunto de live ranges, funde-as em webs por variável e
 * constrói as arestas de interferência entre pares de webs de variáveis
 * distintas (ou derivadas) que coexistam em pelo menos um ponto de programa.
 *
 * A classe é usada de duas formas:
 *  1. Construída a partir de live ranges (construtor principal).
 *  2. Copiada pelo RegisterAllocator quando precisa de preservar o grafo
 *     original enquanto experimenta modificações (spilling/splitting).
 */

class InterferenceGraph {
public:

    /**
     * @brief Constrói o grafo de interferência a partir de um conjunto de live ranges.
     *
     * @details
     * Executa duas fases:
     *  1. **buildWebs()**: agrupa as live ranges por variável e funde as que
     *     se sobrepõem (partilham pelo menos uma linha), usando um algoritmo
     *     greedy de fusão iterativa. Adiciona cada web final como vértice do
     *     @c baseGraph_.
     *  2. **buildEdges()**: para cada par de webs distintas, invoca
     *     Web::interferesWith() e, se verdadeiro, acrescenta uma aresta
     *     bidirecional ao @c baseGraph_.
     *
     * @param ranges  Lista de live ranges de entrada (pode conter múltiplas
     *                ranges da mesma variável; a fusão é feita internamente).
     *
     * @par Complexidade
     * O(W² × L), onde W = número de webs resultante, L = número médio de
     * linhas por web. Dominado pela fase buildEdges() que testa todos os
     * pares (W² chamadas a interferesWith()).
     */

    explicit InterferenceGraph(const std::vector<LiveRange>& ranges);

    /**
     * @brief Construtor de cópia profunda.
     *
     * @details
     * Copia o vetor de webs e reconstrói @c baseGraph_ do zero, para
     * garantir que a cópia não partilha estado com o original. Usa
     * buildEdges() sobre as webs copiadas para recriar todas as arestas.
     *
     * Utilizado pelo RegisterAllocator para preservar o grafo original
     * enquanto experimenta variantes com spilling ou splitting.
     *
     * @param other  Grafo a copiar.
     *
     * @par Complexidade
     * O(W² × L), igual ao construtor principal.
     */

    InterferenceGraph(const InterferenceGraph& other);

    /**
     * @brief Devolve o número de webs (nós) no grafo.
     * @return Número inteiro de nós em @c baseGraph_.
     * @par Complexidade O(1)
     */

    int numNodes() const;

    /**
     * @brief Devolve o vetor de webs que compõem o grafo.
     *
     * A posição @p i do vetor corresponde à web com id @p i.
     *
     * @return Referência constante para o vetor de Web.
     * @par Complexidade O(1)
     */

    const std::vector<Web>& getWebs() const;

    /**
     * @brief Verifica se existe uma aresta de interferência entre as webs @p idA e @p idB.
     *
     * @param idA  Id da primeira web.
     * @param idB  Id da segunda web.
     * @return     @c true se as webs @p idA e @p idB interferem.
     *
     * @par Complexidade
     * Dependente de @c Graph<int>::hasEdge() — tipicamente O(grau do nó).
     */

    bool hasEdge(int idA, int idB) const;

    /**
     * @brief Devolve os ids das webs vizinhas (que interferem) com a web @p id.
     *
     * @details
     * Percorre a lista de adjacência do vértice @p id em @c baseGraph_
     * e recolhe os ids de todos os destinos.
     *
     * @param id  Id da web cujos vizinhos se pretendem.
     * @return    Vetor de ids das webs que interferem com a web @p id.
     *
     * @par Complexidade
     * O(grau(@p id))
     */

    std::vector<int> neighbors(int id) const;

    /**
     * @brief Devolve o grau de interferência (número de vizinhos) da web @p id.
     *
     * @details
     * Corresponde ao número de arestas de interferência incidentes na web
     * com identificador @p id. Usado pelo RegisterAllocator na fase de
     * simplificação do algoritmo de Kempe.
     *
     * @param id  Id da web.
     * @return    Grau inteiro da web no grafo de interferência.
     *
     * @par Complexidade
     * O(1) (acesso direto ao tamanho da lista de adjacência).
     */

     int degree(int id) const;

    /**
     * @brief Imprime o grafo para @c std::cout (modo de depuração).
     *
     * @details
     * Escreve a lista de webs com os seus pontos de programa e, de seguida,
     * a lista de arestas de interferência (cada aresta impressa uma única
     * vez, com o menor id primeiro).
     *
     * @par Complexidade
     * O(W + E), onde W = número de webs, E = número de arestas.
     */

    void print() const;

private:

    /**
     * @brief Vetor de webs, indexado por id (posição @p i ↔ web com id @p i).
     *
     * Preenchido por buildWebs() e imutável após a construção.
     */

    std::vector<Web> webs_;

    /**
     * @brief Grafo de inteiros do professor que representa a estrutura de adjacência.
     *
     * Os vértices têm como informação o id da web (@c int), igual ao índice
     * em @c webs_. As arestas são bidirecionais e sem peso (peso 0.0).
     */

    Graph<int> baseGraph_;

    /**
     * @brief Constrói as webs a partir das live ranges e adiciona os vértices ao grafo.
     *
     * @details
     * Algoritmo:
     *  1. Agrupa as live ranges por nome de variável.
     *  2. Para cada variável, cria uma Web por range (webs parciais).
     *  3. Itera até convergência: se dois parciais se sobrepõem (overlaps()),
     *     funde o segundo no primeiro (absorb()) e remove o segundo.
     *  4. Renumera os ids finais (0..W-1) e adiciona cada web como vértice
     *     em @c baseGraph_.
     *
     * @param ranges  Live ranges de entrada.
     *
     * @par Complexidade
     * O(R³ × P) no pior caso, onde R = número de ranges da variável com
     * mais ranges, P = número de pontos por range. O fator cúbico deve-se
     * ao facto de a cada fusão (absorb) o loop externo reiniciar desde o
     * início, podendo ocorrer até R fusões cada uma precedida de O(R²)
     * chamadas a overlaps(). Na prática tende a ser muito mais eficiente
     * porque as fusões são poucas.
     */

    void buildWebs(const std::vector<LiveRange>& ranges);

    /**
     * @brief Constrói as arestas de interferência entre todas as webs.
     *
     * @details
     * Testa todos os pares (i, j) com i < j. Para cada par em que
     * Web::interferesWith() retorna @c true, invoca addEdge(i, j).
     *
     * @par Complexidade
     * O(W² × L), onde W = número de webs, L = linhas médias por web.
     */

    void buildEdges();

    /**
     * @brief Adiciona uma aresta bidirecional de interferência entre as webs @p idA e @p idB.
     *
     * @details
     * Delega em @c baseGraph_.addBidirectionalEdge(idA, idB, 0.0).
     * Chamado exclusivamente por buildEdges().
     *
     * @param idA  Id da primeira web.
     * @param idB  Id da segunda web.
     *
     * @par Complexidade
     * Dependente de @c Graph<int>::addBidirectionalEdge().
     */

    void addEdge(int idA, int idB);
};

#endif // INTERFERENCEGRAPH_H