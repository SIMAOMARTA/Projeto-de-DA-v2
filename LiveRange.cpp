/**
 * @file LiveRange.cpp
 * @brief Implementação da classe LiveRange.
 */

#include "LiveRange.h"
#include <sstream>
#include <algorithm>

/**
 * @brief Constrói uma LiveRange para uma variável com os pontos fornecidos.
 * @details
 * Inicializa os campos varName e points com os argumentos recebidos.
 * Itera sobre @p points e insere cada número de linha em lineSet,
 * pré-construindo o conjunto para que pesquisas posteriores sejam O(log P)
 * em vez de O(P).
 *
 * @par Complexidade
 * O(P log P), onde P = número de pontos.
 */
LiveRange::LiveRange(const std::string& varName,
                     const std::vector<ProgramPoint>& points)
    : varName(varName), points(points)
{
    for (const auto& p : points)
        lineSet.insert(p.line);
}

/**
 * @brief Devolve o nome da variável associada a esta live range.
 *
 * @par Complexidade
 * O(1)
 */
const std::string& LiveRange::getVarName() const {
    return varName;
}

/**
 * @brief Devolve a lista de pontos de programa desta live range.
 *
 * @par Complexidade
 * O(1)
 */
const std::vector<ProgramPoint>& LiveRange::getPoints() const {
    return points;
}

/**
 * @brief Devolve o conjunto de números de linha desta live range.
 *
 * @par Complexidade
 * O(1)
 */
const std::set<int>& LiveRange::getLineSet() const {
    return lineSet;
}

/**
 * @brief Verifica se esta live range partilha pelo menos uma linha com @p other.
 *
 * @details
 * Utiliza dois iteradores sobre os lineSets ordenados (passo de merge
 * do merge-sort). Em cada passo avança o iterador com o valor mais
 * pequeno. Se em algum ponto os dois valores forem iguais, as ranges
 * intersetam e a função retorna imediatamente.
 *
 * @par Complexidade
 * O(min(|a|, |b|)) no melhor caso (primeira linha já comum);
 * O(|a| + |b|) no pior caso (sem interseção).
 */
bool LiveRange::intersects(const LiveRange& other) const {
    const auto& a = lineSet;
    const auto& b = other.lineSet;
    auto it1 = a.begin(), it2 = b.begin();
    while (it1 != a.end() && it2 != b.end()) {
        if (*it1 == *it2) return true;
        if (*it1 < *it2) ++it1;
        else ++it2;
    }
    return false;
}


/**
 * @brief Devolve uma representação textual da live range.
 *
 * @details
 * Itera sobre o vetor points e constrói a string de saída. Cada ponto
 * é separado por vírgula; o sufixo '+' é adicionado se isStart for
 * verdadeiro e '-' se isEnd for verdadeiro.
 *
 * @par Complexidade
 * O(P) onde P = número de pontos de programa.
 */
std::string LiveRange::toString() const {
    std::ostringstream oss;
    for (size_t i = 0; i < points.size(); ++i) {
        if (i > 0) oss << ',';
        oss << points[i].line;
        if (points[i].isStart) oss << '+';
        if (points[i].isEnd)   oss << '-';
    }
    return oss.str();
}