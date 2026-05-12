#include "InterferenceGraph.h"
#include <iostream>
#include <map>
#include <algorithm>

//construtor principal
InterferenceGraph::InterferenceGraph(const std::vector<LiveRange>& ranges) {
    buildWebs(ranges);
    buildEdges();
}

//agrupar e fundir webs


/**
 * @brief Agrupa os live ranges por variável e funde os que se sobrepõem.
 * Para cada variável pode haver vários live ranges (por exemplo, variável "i" em
 * diferentes laços). Se dois desses ranges partilham pelo menos uma linha,
 * pertencem à mesma web e são fundidos.
 * Algoritmo greedy O(R^2) por variável, onde R = nº de ranges da variável.
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
 * @brief Para cada par de webs verifica se interferem e, se sim, adiciona aresta.
 * Complexidade: O(W^2) chamadas a interferesWith(), cada uma O(L log L).
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

void InterferenceGraph::addEdge(int idA, int idB) {
    adjList_[idA].insert(idB);
    adjList_[idB].insert(idA);
}

// getters

const std::vector<Web>& InterferenceGraph::getWebs() const {
    return webs_;
}

bool InterferenceGraph::hasEdge(int idA, int idB) const {
    auto it = adjList_.find(idA);
    if (it == adjList_.end()) return false;
    return it->second.count(idB) > 0;
}

const std::set<int>& InterferenceGraph::neighbors(int id) const {
    return adjList_.at(id);
}

int InterferenceGraph::degree(int id) const {
    auto it = adjList_.find(id);
    if (it == adjList_.end()) return 0;
    return (int)it->second.size();
}

int InterferenceGraph::numNodes() const {
    return (int)webs_.size();
}

//debug

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