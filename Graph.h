/*
 * Graph.h
 * (Modificado para suportar arestas bidirecionais e verificação de adjacência
 * necessárias para o Grafo de Interferência no Projeto 2)
 */
#ifndef GRAPH_H_
#define GRAPH_H_

#include <cstddef>
#include <vector>
#include <queue>
#include <fstream>
#include <iostream>
#include <limits>
#include <algorithm>
// #include "../data_structures/MutablePriorityQueue.h" // Comentado se não estiveres a usar

using namespace std;

template <class T> class Edge;
template <class T> class Graph;
template <class T> class Vertex;

/****************** Provided structures  ********************/

template <class T>
class Vertex {
	T info;                // contents
	vector<Edge<T> > adj;  // list of outgoing edges
	bool visited;          // auxiliary field
    bool processing;       // auxiliary field
    int indegree;          // auxiliary field
    int num;               // auxiliary field
    int low;               // auxiliary field

    void addEdge(Vertex<T> *dest, double w);
	bool removeEdgeTo(Vertex<T> *d);

public:
	Vertex(T in);
    T getInfo() const;
    void setInfo(T in);
    bool isVisited() const;
    void setVisited(bool v);
    bool isProcessing() const;
    void setProcessing(bool p);
    int getIndegree() const;
    void setIndegree(int indegree);
    void setLow(int low);
    void setNum(int num);
    int getNum() const;
    int getLow() const;

    const vector<Edge<T>> &getAdj() const;
    void setAdj(const vector<Edge<T>> &adj);

    // ADIÇÃO: Verificação de aresta existente
    bool hasEdgeTo(Vertex<T> *d) const;

    friend class Graph<T>;
};

template <class T>
class Edge {
	Vertex<T> * dest;      // destination vertex
	double weight;         // edge weight
public:
	Edge(Vertex<T> *d, double w);
    Vertex<T> *getDest() const;
    void setDest(Vertex<T> *dest);
    double getWeight() const;
    void setWeight(double weight);
    friend class Graph<T>;
	friend class Vertex<T>;
};

template <class T>
class Graph {
	vector<Vertex<T> *> vertexSet;    // vertex set
    void dfsVisit(Vertex<T> *v,  vector<T> & res) const;
public:
    // ... Destrutor recomendado para evitar memory leaks (Opcional, mas boa prática)
    ~Graph();

    Vertex<T> *findVertex(const T &in) const;
    int getNumVertex() const;
	bool addVertex(const T &in);
	bool removeVertex(const T &in);
	bool addEdge(const T &sourc, const T &dest, double w);
	bool removeEdge(const T &sourc, const T &dest);
    vector<Vertex<T> * > getVertexSet() const;
	vector<T> dfs() const;
	vector<T> dfs(const T & source) const;
	vector<T> bfs(const T & source) const;
    void emitDOTFile(string gname);

    // ADIÇÃO: Suporte prático para grafos não direcionados
    bool addBidirectionalEdge(const T &sourc, const T &dest, double w = 0);
    bool hasEdge(const T &sourc, const T &dest) const;
};

/****************** Provided constructors and functions ********************/

template <class T>
Vertex<T>::Vertex(T in): info(in), visited(false), processing(false), indegree(0), num(0), low(0) {}

template <class T>
Edge<T>::Edge(Vertex<T> *d, double w): dest(d), weight(w) {}

template <class T>
Graph<T>::~Graph() {
    for (auto v : vertexSet) {
        delete v;
    }
}

template <class T>
int Graph<T>::getNumVertex() const {
	return vertexSet.size();
}

template <class T>
vector<Vertex<T> * > Graph<T>::getVertexSet() const {
    return vertexSet;
}

template<class T>
T Vertex<T>::getInfo() const {
    return info;
}

template<class T>
void Vertex<T>::setInfo(T in) {
    Vertex::info = in;
}

template<class T>
bool Vertex<T>::isProcessing() const {
    return processing;
}

template<class T>
void Vertex<T>::setNum(int num) {
    Vertex::num = num;
}

template<class T>
void Vertex<T>::setLow(int low) {
    Vertex::low = low;
}

template<class T>
int Vertex<T>::getNum() const {
    return num;
}

template<class T>
int Vertex<T>::getLow() const {
    return low;
}

template<class T>
void Vertex<T>::setProcessing(bool p) {
    Vertex::processing = p;
}

template<class T>
Vertex<T> *Edge<T>::getDest() const {
    return dest;
}

template<class T>
void Edge<T>::setDest(Vertex<T> *d) {
    Edge::dest = d;
}

template<class T>
double Edge<T>::getWeight() const {
    return weight;
}

template<class T>
void Edge<T>::setWeight(double weight) {
    Edge::weight = weight;
}

template <class T>
Vertex<T> * Graph<T>::findVertex(const T &in) const {
	for (auto v : vertexSet)
		if (v->info == in)
			return v;
	return NULL;
}

template <class T>
bool Vertex<T>::isVisited() const {
    return visited;
}

template <class T>
void Vertex<T>::setVisited(bool v) {
    Vertex::visited = v;
}

template<class T>
int Vertex<T>::getIndegree() const {
    return indegree;
}

template<class T>
void Vertex<T>::setIndegree(int indegree) {
    Vertex::indegree = indegree;
}

template<class T>
const vector<Edge<T>> &Vertex<T>::getAdj() const {
    return adj;
}

template <class T>
void Vertex<T>::setAdj(const vector<Edge<T>> &adj) {
    Vertex::adj = adj;
}

template <class T>
bool Graph<T>::addVertex(const T &in) {
	if (findVertex(in) != NULL)
		return false;
	vertexSet.push_back(new Vertex<T>(in));
	return true;
}

template <class T>
bool Graph<T>::addEdge(const T &sourc, const T &dest, double w) {
	auto v1 = findVertex(sourc);
	auto v2 = findVertex(dest);
	if (v1 == NULL || v2 == NULL)
		return false;
	v1->addEdge(v2, w);
	return true;
}

// ADIÇÃO: Criar ligação nos dois sentidos (necessário para Web Interference)
template <class T>
bool Graph<T>::addBidirectionalEdge(const T &sourc, const T &dest, double w) {
    bool res1 = addEdge(sourc, dest, w);
    bool res2 = addEdge(dest, sourc, w);
    return res1 && res2;
}

// ADIÇÃO: Verifica se uma aresta direcionada já existe neste Vertex
template <class T>
bool Vertex<T>::hasEdgeTo(Vertex<T> *d) const {
    for (const auto& edge : adj) {
        if (edge.getDest() == d) return true;
    }
    return false;
}

// ADIÇÃO: Verifica se existe aresta entre source e dest
template <class T>
bool Graph<T>::hasEdge(const T &sourc, const T &dest) const {
    auto v1 = findVertex(sourc);
    auto v2 = findVertex(dest);
    if (v1 == NULL || v2 == NULL) return false;
    return v1->hasEdgeTo(v2);
}

template <class T>
void Vertex<T>::addEdge(Vertex<T> *d, double w) {
	adj.push_back(Edge<T>(d, w));
}

template <class T>
bool Graph<T>::removeEdge(const T &sourc, const T &dest) {
	auto v1 = findVertex(sourc);
	auto v2 = findVertex(dest);
	if (v1 == NULL || v2 == NULL)
		return false;
	return v1->removeEdgeTo(v2);
}

template <class T>
bool Vertex<T>::removeEdgeTo(Vertex<T> *d) {
	for (auto it = adj.begin(); it != adj.end(); it++)
		if (it->dest == d) {
			adj.erase(it);
			return true;
		}
	return false;
}

template <class T>
bool Graph<T>::removeVertex(const T &in) {
	for (auto it = vertexSet.begin(); it != vertexSet.end(); it++)
		if ((*it)->info == in) {
			auto v = *it;
			vertexSet.erase(it);
			for (auto u : vertexSet)
				u->removeEdgeTo(v);
			delete v;
			return true;
		}
	return false;
}

template <class T>
inline void Graph<T>::emitDOTFile(string gname) {
    // ... Mantido original ...
}

#endif /* GRAPH_H_ */