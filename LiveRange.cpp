/**
 * @file LiveRange.cpp
 * @brief Implementação da classe LiveRange.
 *
 * @details
 * Implementa o construtor, os getters e os dois métodos computacionais
 * da classe LiveRange: intersects() e toString(). Toda a complexidade
 * algorítmica relevante está no construtor (pré-construção do @c lineSet)
 * e em intersects() (pesquisa linear por merge de dois conjuntos ordenados).
 *
 * @see LiveRange.h para a documentação completa da interface pública.
 */

#include "LiveRange.h"
#include <sstream>
#include <algorithm>

LiveRange::LiveRange(const std::string& varName, const std::vector<ProgramPoint>& points) : varName(varName),
                                                                                            points(points){
    // Itera sobre points e insere o número de linha de cada um
    // em lineSet, pré-construindo o conjunto ordenado para que chamadas
    // futuras a intersects() sejam O(P) em vez de O(P²).
    for (const auto& p : points)
        lineSet.insert(p.line);
}

const std::string& LiveRange::getVarName() const{
    return varName;
}

const std::vector<ProgramPoint>& LiveRange::getPoints() const{
    return points;
}

const std::set<int>& LiveRange::getLineSet() const{
    return lineSet;
}

bool LiveRange::intersects(const LiveRange& other) const{
    // Utiliza o algoritmo de merge sobre dois iteradores dos lineSets
    // ordenados, equivalente ao passo de merge do merge-sort:
    // 1. Em cada passo, avança o iterador cujo valor corrente é menor.
    // 2. Se em algum ponto os dois valores forem iguais, retorna true.
    // 3. Se algum iterador chegar ao fim sem igualdade, retorna false.
    // Esta abordagem opera diretamente nos std::set já ordenados, obtendo complexidade linear.

    const auto& a = lineSet;
    const auto& b = other.lineSet;
    auto it1 = a.begin(), it2 = b.begin();

    while (it1 != a.end() && it2 != b.end()){
        if (*it1 == *it2) return true;
        if (*it1 < *it2) ++it1;
        else ++it2;
    }

    return false;
}

std::string LiveRange::toString() const{
    // Constrói a string no formato exigido. Cada ponto é representado pelo
    // número de linha, seguido opcionalmente dos sufixos '+' (se isStart)
    // e '-' (se isEnd). Um ponto pode ter ambos os sufixos.

    std::ostringstream oss;

    for (size_t i = 0; i < points.size(); ++i){
        if (i > 0) oss << ',';
        oss << points[i].line;
        if (points[i].isStart) oss << '+';
        if (points[i].isEnd) oss << '-';
    }

    return oss.str();
}
