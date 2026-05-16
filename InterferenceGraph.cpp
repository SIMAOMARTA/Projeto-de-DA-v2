/**
 * @file InterferenceGraph.cpp
 * @brief Implementação do grafo de interferência.
 *
 * Implementa a construção do grafo em dois passos:
 *  - buildWebs()  : fusão de live ranges em webs por variável.
 *  - buildEdges() : deteção de interferência entre cada par de webs.
 */

#include "InterferenceGraph.h"
#include <iostream>
#include <map>
#include <algorithm>

//construtor
/**
 * @brief Constrói o grafo de interferência a partir de uma lista de LiveRanges.
 *
 * @details
 * Envia imediatamente para buildWebs() e depois buildEdges().
 * A separação em dois métodos distintos facilita testes unitários
 * independentes de cada fase da construção.
 *
 *
 * @par Complexidade
 * O(W² × L) dominado por buildEdges(),
 * onde W = número de webs e L = número médio de linhas por web.
 */
InterferenceGraph::InterferenceGraph(const std::vector<LiveRange>& ranges) {
    buildWebs(ranges);
    buildEdges();
}

//agrupar e fundir webs

/**
 * @brief Agrupa e funde live ranges em webs por variável.
 *
 * @details
 * Pipeline de construção de webs em três passos:
 *
 * **Agrupar por variável**
 * Usa um @c std::map<string, vector<LiveRange>> para agrupar todas as
 * live ranges pelo nome da variável.
 *
 * **Fusão greedy por grupo**
 * Para cada variável:
 *  1. Cria uma Web individual por LiveRange (estado inicial).
 *  2. Repete enquanto houver fusões possíveis: para cada par (i,j),
 *     se Web::overlaps() == true, absorve j em i e recomeça o ciclo.
 *  3. Termina quando nenhum par de webs se sobrepõe.
 *
 * Esta abordagem greedy garante que todas as ranges que, direta ou
 * transitivamente, partilham linhas são fundidas numa única web.
 *
 * **Renumeração**
 * Após todas as fusões os ids internos podem ter lacunas (ids de webs
 * absorvidas ficam sem uso). Renumera sequencialmente de 0 a N-1.
 *
 *
 * @par Complexidade
 * O(V × R²) onde V = número de variáveis distintas e R = número médio
 * de live ranges por variável.
 */
void InterferenceGraph::buildWebs(const std::vector<LiveRange>& ranges) {
    // Agrupa por nome de variável
    std::map<std::string, std::vector<LiveRange>> byVar;
    for (const auto& lr : ranges)
        byVar[lr.getVarName()].push_back(lr);

    int nextId = 0;

    for (auto& [varName, lrList] : byVar) {
        // Cada live range começa como uma web independente
        std::vector<Web> partialWebs;
        partialWebs.reserve(lrList.size());
        for (const auto& lr : lrList)
            partialWebs.emplace_back(nextId++, lr);

        // Fusão iterativa: se duas webs se sobrepõem, absorve uma na outra
        // Repete até não haver mais fusões
        bool merged = true;
        while (merged) {
            merged = false;
            for (size_t i = 0; i < partialWebs.size() && !merged; ++i) {
                for (size_t j = i + 1; j < partialWebs.size(); ++j) {
                    if (partialWebs[i].overlaps(partialWebs[j])) {
                        // Absorve j em i
                        partialWebs[i].absorb(partialWebs[j]);
                        partialWebs.erase(partialWebs.begin() + (int)j);
                        merged = true;
                        break;
                    }
                }
            }
        }

        // Adiciona as webs finais ao grafo
        for (auto& w : partialWebs)
            webs_.push_back(std::move(w));
    }

    // Renumera os ids de 0..N-1 de forma sequencial (depois das fusões os
    // ids internos podem ter lacunas)
    for (int i = 0; i < (int)webs_.size(); ++i)
        webs_[i].setId(i);
}

//construir as arestas

/**
 * @brief Constrói as arestas do grafo testando interferência entre cada par de webs.
 *
 * @details
 * Inicializa entradas vazias na adjList_ para todos os nós, garantindo
 * que neighbors() e degree() funcionam mesmo para nós isolados (sem
 * vizinhos). De seguida itera sobre todos os pares (i,j) com i < j e
 * chama Web::interferesWith(); se verdadeiro, adiciona aresta via addEdge().
 *
 * @par Complexidade
 * O(W² × L) onde W = número de webs e L = número médio de linhas por web.
 */
void InterferenceGraph::buildEdges() {
    int n = (int)webs_.size();
    // Inicializa lista de adjacência vazia para todos os nós
    for (int i = 0; i < n; ++i)
        adjList_[i];   // garante que a entrada existe mesmo que não haja vizinhos

    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            if (webs_[i].interferesWith(webs_[j]))
                addEdge(i, j);
}

/**
 * @brief Adiciona uma aresta bidirecional entre dois nós.
 *
 * @details
 * Insere idB no set de adjacência de idA e idA no set de idB.
 * Como std::set ignora duplicados, chamadas repetidas com os mesmos
 * argumentos são seguras.
 *
 *
 * @par Complexidade
 * O(log W) onde W = número de webs.
 */
void InterferenceGraph::addEdge(int idA, int idB) {
    adjList_[idA].insert(idB);
    adjList_[idB].insert(idA);
}


// getters
/**
 * @brief Devolve a lista de todas as webs (nós) do grafo.
 * @par Complexidade
 * O(1)
 */
const std::vector<Web>& InterferenceGraph::getWebs() const {
    return webs_;
}

/**
 * @brief Verifica se existe uma aresta entre dois nós.
 *
 *
 * @par Complexidade
 * O(log W) onde W = número de webs.
 */
bool InterferenceGraph::hasEdge(int idA, int idB) const {
    auto it = adjList_.find(idA);
    if (it == adjList_.end()) return false;
    return it->second.count(idB) > 0;
}

/**
 * @brief Devolve o conjunto de vizinhos de um nó.
 *
 *
 *
 * @par Complexidade
 * O(log W) onde W = número de webs.
 */
const std::set<int>& InterferenceGraph::neighbors(int id) const {
    return adjList_.at(id);
}

/**
 * @brief Devolve o grau (número de vizinhos) de um nó.
 *
 *
 * @par Complexidade
 * O(log W) onde W = número de webs.
 */
int InterferenceGraph::degree(int id) const {
    auto it = adjList_.find(id);
    if (it == adjList_.end()) return 0;
    return (int)it->second.size();
}

/**
 * @brief Devolve o número de nós (webs) no grafo.
 * @par Complexidade
 * O(1)
 */
int InterferenceGraph::numNodes() const {
    return (int)webs_.size();
}

//debug
/**
 * @brief Imprime o grafo no stdout para fins de depuração.
 *
 * @details
 * Imprime primeiro a lista de webs com o respectivo toString(),
 * e depois todas as arestas no formato "webA -- webB". Cada aresta
 * é impressa apenas uma vez, garantido pela condição id < nb.
 *
 * @par Complexidade
 * O(W + E) onde W = número de webs e E = número de arestas.
 */
void InterferenceGraph::print() const {
    std::cout << "=== Interference Graph ===\n";
    std::cout << "Webs (" << webs_.size() << "):\n";
    for (const auto& w : webs_) {
        std::cout << "  web" << w.getId()
                  << " [" << w.getVarName() << "]: "
                  << w.toString() << "\n";
    }
    std::cout << "Edges:\n";
    for (const auto& [id, nbrs] : adjList_) {
        for (int nb : nbrs) {
            if (id < nb)   // imprime cada aresta só uma vez
                std::cout << "  web" << id << " -- web" << nb << "\n";
        }
    }
    std::cout << "==========================\n";
}