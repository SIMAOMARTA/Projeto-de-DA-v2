/**
 * @file InterferenceGraph.cpp
 * @brief Implementação da classe InterferenceGraph.
 *
 * @details
 * Implementa a construção do grafo de interferência a partir de um
 * conjunto de live ranges. A implementação divide-se em duas fases:
 *  - buildWebs()  : fusão greedy de live ranges sobrepostas em webs.
 *  - buildEdges() : determinação de interferência entre todos os pares de webs.
 *
 * O grafo subjacente (@c Graph<int>) é o fornecido nas aulas TP, ao qual
 * se acedem via @c addVertex(), @c addBidirectionalEdge(), @c findVertex(),
 * @c getAdj(), @c getNumVertex() e @c getVertexSet().
 *
 * @see InterferenceGraph.h para a documentação completa da interface pública.
 */

#include "InterferenceGraph.h"
#include <iostream>
#include <map>
#include <algorithm>

/**
 * @brief Constrói o grafo de interferência a partir de um conjunto de live ranges.
 *
 * @details
 * Delega a construção nas duas fases privadas buildWebs() e buildEdges(),
 * executadas por esta ordem.
 *
 * @param ranges  Lista de live ranges de entrada.
 *
 * @par Complexidade
 * O(W² × L), dominado pela fase buildEdges().
 */

InterferenceGraph::InterferenceGraph(const std::vector<LiveRange>& ranges){
    buildWebs(ranges);
    buildEdges();
}

/**
 * @brief Agrupa as live ranges por variável e funde as que se sobrepõem.
 *
 * @details
 * Algoritmo (por variável):
 *  1. Cria uma web parcial por cada live range.
 *  2. Entra num ciclo externo que percorre todos os pares (i, j) com i < j.
 *     Se a web @p i e a web @p j se sobrepõem (overlaps()), a web @p j é
 *     absorvida pela @p i e removida do vetor. O ciclo reinicia (flag @c merged).
 *  3. Repete até nenhum par de webs se sobrepor (convergência garantida,
 *     pois a cada iteração o número de webs decresce estritamente em 1).
 *  4. As webs resultantes são adicionadas ao vetor global @c webs_.
 *
 * Após processar todas as variáveis, renumera os ids (0..W-1) e adiciona
 * cada web como vértice em @c baseGraph_.
 *
 * @param ranges  Lista de live ranges de entrada.
 *
 * @par Complexidade
 * O(R² × P) por variável, onde R = número de ranges da variável com mais
 * ranges, P = pontos por range. Na prática tende a ser O(R × P) porque
 * as fusões são poucas.
 */

void InterferenceGraph::buildWebs(const std::vector<LiveRange>& ranges){
    std::map<std::string, std::vector<LiveRange>> byVar;
    for (const auto& lr : ranges)
        byVar[lr.getVarName()].push_back(lr);

    int nextId = 0;

    for (auto& [varName, lrList] : byVar){
        // Uma web parcial por live range
        std::vector<Web> partialWebs;
        partialWebs.reserve(lrList.size());
        for (const auto& lr : lrList)
            partialWebs.emplace_back(nextId++, lr);

        bool merged = true;
        while (merged){
            merged = false;
            for (size_t i = 0; i < partialWebs.size() && !merged; ++i){
                for (size_t j = i + 1; j < partialWebs.size(); ++j){
                    if (partialWebs[i].overlaps(partialWebs[j])){
                        partialWebs[i].absorb(partialWebs[j]);
                        partialWebs.erase(partialWebs.begin() + (int)j);
                        merged = true;
                        break;
                    }
                }
            }
        }

        for (auto& w : partialWebs)
            webs_.push_back(std::move(w));
    }

    for (int i = 0; i < (int)webs_.size(); ++i){
        webs_[i].setId(i);
        baseGraph_.addVertex(i);
    }
}

/**
 * @brief Constrói as arestas de interferência entre todos os pares de webs.
 *
 * @details
 * Testa cada par não ordenado (i, j) com i < j. Para cada par em que
 * Web::interferesWith() retorna @c true, adiciona uma aresta bidirecional
 * ao @c baseGraph_ via addEdge(). Esta fase domina a complexidade total
 * da construção do grafo.
 *
 * @par Complexidade
 * O(W² × L), onde W = número de webs e L = número médio de linhas
 * partilhadas por par de webs. O fator L provém de interferesWith().
 */

void InterferenceGraph::buildEdges() {
    int n = (int)webs_.size();
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (webs_[i].interferesWith(webs_[j])) {
                addEdge(i, j);
            }
        }
    }
}

/**
 * @brief Adiciona uma aresta bidirecional de interferência entre as webs @p idA e @p idB.
 *
 * @details
 * Delega em @c Graph<int>::addBidirectionalEdge() com peso 0.0 (o peso
 * não tem significado semântico no grafo de interferência).
 *
 * @param idA  Id da primeira web.
 * @param idB  Id da segunda web.
 *
 * @par Complexidade
 * Dependente de @c Graph<int>::addBidirectionalEdge() — tipicamente O(1)
 * amortizado com listas de adjacência.
 */

void InterferenceGraph::addEdge(int idA, int idB){
    baseGraph_.addBidirectionalEdge(idA, idB, 0.0);
}

/**
 * @brief Construtor de cópia profunda do grafo de interferência.
 *
 * @details
 * Copia o vetor de webs e reconstrói @c baseGraph_ do zero (adição dos
 * vértices e reconstrução de todas as arestas via buildEdges()), garantindo
 * que a cópia é completamente independente do original.
 *
 * Este construtor é utilizado pelo RegisterAllocator para preservar o
 * grafo original enquanto experimenta spilling ou splitting num grafo
 * auxiliar.
 *
 * @param other  Grafo a copiar.
 *
 * @par Complexidade
 * O(W² × L), igual ao construtor principal.
 */

InterferenceGraph::InterferenceGraph(const InterferenceGraph& other): webs_(other.webs_){
    for (int i = 0; i < (int)webs_.size(); ++i){
        webs_[i].setId(i);
        baseGraph_.addVertex(i);
    }
    buildEdges();
}

/**
 * @brief Devolve o vetor de webs que compõem o grafo.
 * @return Referência constante para o vetor de Web.
 * @par Complexidade O(1)
 */

const std::vector<Web>& InterferenceGraph::getWebs() const{
    return webs_;
}

/**
 * @brief Verifica se existe uma aresta de interferência entre as webs @p idA e @p idB.
 *
 * @param idA  Id da primeira web.
 * @param idB  Id da segunda web.
 * @return     @c true se as webs interferem no @c baseGraph_.
 *
 * @par Complexidade
 * Dependente de @c Graph<int>::hasEdge().
 */

bool InterferenceGraph::hasEdge(int idA, int idB) const{
    return baseGraph_.hasEdge(idA, idB);
}

/**
 * @brief Devolve os ids das webs que interferem com a web @p id.
 *
 * @details
 * Obtém o vértice de @c baseGraph_ com informação @p id e percorre a sua
 * lista de adjacência, recolhendo os ids (informação) de cada destino.
 *
 * @param id  Id da web cujos vizinhos se pretendem.
 * @return    Vetor de ids das webs vizinhas.
 *
 * @par Complexidade
 * O(grau(@p id))
 */

std::vector<int> InterferenceGraph::neighbors(int id) const{
    std::vector<int> nbrs;
    Vertex<int>* v = baseGraph_.findVertex(id);
    if (v != nullptr) {
        for (const auto& edge : v->getAdj()) {
            nbrs.push_back(edge.getDest()->getInfo());
        }
    }

    return nbrs;
}

/**
 * @brief Devolve o grau de interferência da web @p id.
 *
 * @details
 * Retorna o número de arestas de interferência incidentes na web @p id,
 * obtido diretamente do tamanho da lista de adjacência do vértice
 * correspondente em @c baseGraph_.
 *
 * @param id  Id da web.
 * @return    Grau inteiro (0 se o id não existir no grafo).
 *
 * @par Complexidade
 * O(1)
 */

int InterferenceGraph::degree(int id) const{
    Vertex<int>* v = baseGraph_.findVertex(id);
    if (v != nullptr){
        return (int)v->getAdj().size();
    }

    return 0;
}

/**
 * @brief Devolve o número de webs (nós) no grafo.
 * @return Número inteiro de nós em @c baseGraph_.
 * @par Complexidade O(1)
 */

int InterferenceGraph::numNodes() const{
    return baseGraph_.getNumVertex();
}

/**
 * @brief Imprime o grafo de interferência para @c std::cout.
 *
 * @details
 * Escreve:
 *  1. A lista de webs com id, nome de variável e pontos de programa.
 *  2. A lista de arestas de interferência, cada aresta impressa uma única
 *     vez (garante id_menor < id_maior).
 *
 * Útil para depuração e validação manual dos resultados.
 *
 * @par Complexidade
 * O(W + E), onde W = número de webs, E = número de arestas.
 */

void InterferenceGraph::print() const{
    std::cout << "=== Interference Graph ===\n";
    std::cout << "Webs (" << webs_.size() << "):\n";
    for (const auto& w : webs_)
        std::cout << "  web" << w.getId() << " [" << w.getVarName() << "]: " << w.toString() << "\n";

    std::cout << "Edges:\n";
    for (auto v : baseGraph_.getVertexSet()){
        int id = v->getInfo();
        for (const auto& edge : v->getAdj()){
            int nb = edge.getDest()->getInfo();
            if (id < nb) std::cout << "  web" << id << " -- web" << nb << "\n";
        }
    }
    std::cout << "==========================\n";
}
