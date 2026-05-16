#include "InterferenceGraph.h"
#include <iostream>
#include <map>
#include <algorithm>

InterferenceGraph::InterferenceGraph(const std::vector<LiveRange>& ranges) {
    buildWebs(ranges);
    buildEdges();
}

void InterferenceGraph::buildWebs(const std::vector<LiveRange>& ranges) {
    std::map<std::string, std::vector<LiveRange>> byVar;
    for (const auto& lr : ranges)
        byVar[lr.getVarName()].push_back(lr);

    int nextId = 0;

    for (auto& [varName, lrList] : byVar) {
        std::vector<Web> partialWebs;
        partialWebs.reserve(lrList.size());
        for (const auto& lr : lrList)
            partialWebs.emplace_back(nextId++, lr);

        bool merged = true;
        while (merged) {
            merged = false;
            for (size_t i = 0; i < partialWebs.size() && !merged; ++i) {
                for (size_t j = i + 1; j < partialWebs.size(); ++j) {
                    if (partialWebs[i].overlaps(partialWebs[j])) {
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

    // Renumera os IDs e INSERE OS VÉRTICES NO GRAPH DO PROFESSOR
    for (int i = 0; i < (int)webs_.size(); ++i) {
        webs_[i].setId(i);
        baseGraph_.addVertex(i); // Adiciona o nó à estrutura nativa
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

void InterferenceGraph::addEdge(int idA, int idB) {
    // Adiciona no grafo bidirecional do professor
    baseGraph_.addBidirectionalEdge(idA, idB, 0.0);
}

const std::vector<Web>& InterferenceGraph::getWebs() const {
    return webs_;
}

bool InterferenceGraph::hasEdge(int idA, int idB) const {
    return baseGraph_.hasEdge(idA, idB);
}

std::vector<int> InterferenceGraph::neighbors(int id) const {
    std::vector<int> nbrs;
    Vertex<int>* v = baseGraph_.findVertex(id);
    if (v != nullptr) {
        for (const auto& edge : v->getAdj()) {
            nbrs.push_back(edge.getDest()->getInfo());
        }
    }
    return nbrs;
}

int InterferenceGraph::degree(int id) const {
    Vertex<int>* v = baseGraph_.findVertex(id);
    if (v != nullptr) {
        return v->getAdj().size();
    }
    return 0;
}

int InterferenceGraph::numNodes() const {
    return baseGraph_.getNumVertex();
}

void InterferenceGraph::print() const {
    std::cout << "=== Interference Graph ===\n";
    std::cout << "Webs (" << webs_.size() << "):\n";
    for (const auto& w : webs_) {
        std::cout << "  web" << w.getId()
                  << " [" << w.getVarName() << "]: "
                  << w.toString() << "\n";
    }
    std::cout << "Edges:\n";
    for (auto v : baseGraph_.getVertexSet()) {
        int id = v->getInfo();
        for (const auto& edge : v->getAdj()) {
            int nb = edge.getDest()->getInfo();
            if (id < nb) { // Imprime cada aresta só uma vez
                std::cout << "  web" << id << " -- web" << nb << "\n";
            }
        }
    }
    std::cout << "==========================\n";
}