#ifndef INTERFERENCEGRAPH_H
#define INTERFERENCEGRAPH_H

#include <vector>
#include <string>
#include "LiveRange.h"
#include "Web.h"
#include "Graph.h" // A estrutura do professor

class InterferenceGraph {
public:
    explicit InterferenceGraph(const std::vector<LiveRange>& ranges);

    InterferenceGraph(const InterferenceGraph& other);

    int numNodes() const;
    const std::vector<Web>& getWebs() const;
    bool hasEdge(int idA, int idB) const;

    // Devolve vector<int> em vez de set para acoplar bem ao Graph
    std::vector<int> neighbors(int id) const;

    int degree(int id) const;
    void print() const;

private:
    std::vector<Web> webs_;

    // Aqui usamos o grafo do professor (mapeando o Web ID)
    Graph<int> baseGraph_;

    void buildWebs(const std::vector<LiveRange>& ranges);
    void buildEdges();
    void addEdge(int idA, int idB);
};

#endif // INTERFERENCEGRAPH_H