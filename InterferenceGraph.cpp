/**
 * @file InterferenceGraph.cpp
 * @brief Implementação da classe InterferenceGraph.
 *
 * @details
 * Implementa a construção do grafo de interferência a partir de um
 * conjunto de live ranges. A implementação divide-se em duas fases:
 * - buildWebs()  : fusão greedy de live ranges sobrepostas em webs.
 * - buildEdges() : determinação de interferência entre todos os pares de webs.
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

InterferenceGraph::InterferenceGraph(const std::vector<LiveRange>& ranges){
    buildWebs(ranges);
    buildEdges();
}

void InterferenceGraph::buildWebs(const std::vector<LiveRange>& ranges){
    // Algoritmo (por variável):
    // 1. Cria uma web parcial por cada live range.
    // 2. Entra num ciclo externo que percorre todos os pares (i, j) com i < j.
    //    Se a web i e a web j se sobrepõem (overlaps()), a web j é
    //    absorvida pela i e removida do vetor. O ciclo reinicia (flag merged).
    // 3. Repete até nenhum par de webs se sobrepor (convergência garantida).
    // 4. As webs resultantes são adicionadas ao vetor global webs_.

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

    // Após processar todas as variáveis, renumera os ids (0..W-1) e adiciona
    // cada web como vértice em baseGraph_.
    for (int i = 0; i < (int)webs_.size(); ++i){
        webs_[i].setId(i);
        baseGraph_.addVertex(i);
    }
}

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

void InterferenceGraph::addEdge(int idA, int idB){
    // O peso 0.0 não tem significado semântico no grafo de interferência
    baseGraph_.addBidirectionalEdge(idA, idB, 0.0);
}

InterferenceGraph::InterferenceGraph(const InterferenceGraph& other): webs_(other.webs_){
    for (int i = 0; i < (int)webs_.size(); ++i){
        webs_[i].setId(i);
        baseGraph_.addVertex(i);
    }
    buildEdges();
}

const std::vector<Web>& InterferenceGraph::getWebs() const{
    return webs_;
}

bool InterferenceGraph::hasEdge(int idA, int idB) const{
    return baseGraph_.hasEdge(idA, idB);
}

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

int InterferenceGraph::degree(int id) const{
    Vertex<int>* v = baseGraph_.findVertex(id);
    if (v != nullptr){
        return (int)v->getAdj().size();
    }

    return 0;
}

int InterferenceGraph::numNodes() const{
    return baseGraph_.getNumVertex();
}

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
            // Garante que cada aresta é impressa uma única vez (id_menor < id_maior)
            if (id < nb) std::cout << "  web" << id << " -- web" << nb << "\n";
        }
    }
    std::cout << "==========================\n";
}
