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

/**
 * @brief Constrói uma LiveRange para uma variável com os pontos fornecidos.
 *
 * @details
 * Inicializa os campos @c varName e @c points com os argumentos recebidos.
 * De seguida itera sobre @p points e insere o número de linha de cada um
 * em @c lineSet, pré-construindo o conjunto ordenado para que chamadas
 * futuras a intersects() sejam O(P) em vez de O(P²).
 *
 * @param varName  Nome da variável associada a esta live range.
 * @param points   Lista de pontos de programa que compõem a live range.
 *
 * @par Complexidade
 * O(P log P), onde P = número de pontos, dominado pelas inserções no @c std::set.
 */

LiveRange::LiveRange(const std::string& varName, const std::vector<ProgramPoint>& points) : varName(varName),
                                                                                            points(points){
    for (const auto& p : points)
        lineSet.insert(p.line);
}

/**
 * @brief Devolve o nome da variável associada a esta live range.
 *
 * @return Referência constante para o nome da variável.
 *
 * @par Complexidade
 * O(1)
 */

const std::string& LiveRange::getVarName() const{
    return varName;
}

/**
 * @brief Devolve a lista de pontos de programa desta live range.
 *
 * Os pontos estão na ordem em que foram fornecidos ao construtor,
 * preservando as anotações @c isStart e @c isEnd de cada ponto.
 *
 * @return Referência constante para o vetor de ProgramPoint.
 *
 * @par Complexidade
 * O(1)
 */

const std::vector<ProgramPoint>& LiveRange::getPoints() const{
    return points;
}

/**
 * @brief Devolve o conjunto de números de linha desta live range.
 *
 * @return Referência constante para o @c std::set<int> de linhas ordenadas.
 *
 * @par Complexidade
 * O(1)
 */

const std::set<int>& LiveRange::getLineSet() const{
    return lineSet;
}

/**
 * @brief Verifica se esta live range partilha pelo menos uma linha com @p other.
 *
 * @details
 * Utiliza o algoritmo de merge sobre dois iteradores dos @c lineSets
 * ordenados, equivalente ao passo de merge do merge-sort:
 *  -# Em cada passo, avança o iterador cujo valor corrente é menor.
 *  -# Se em algum ponto os dois valores forem iguais, retorna @c true.
 *  -# Se algum iterador chegar ao fim sem igualdade, retorna @c false.
 *
 * Esta abordagem evita a construção de estruturas auxiliares e opera
 * diretamente nos @c std::set já ordenados, obtendo complexidade linear.
 *
 * @param other  A outra live range a comparar.
 * @return       @c true se houver pelo menos uma linha comum;
 *               @c false caso contrário.
 *
 * @par Complexidade
 * O(min(|A|, |B|)) no melhor caso (primeira linha já comum);
 * O(|A| + |B|) no pior caso (sem qualquer linha comum).
 */

bool LiveRange::intersects(const LiveRange& other) const{
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

/**
 * @brief Devolve uma representação textual da live range.
 *
 * @details
 * Itera sobre o vetor @c points e constrói a string de saída no formato
 * exigido pelo enunciado. Cada ponto é representado pelo número de linha,
 * seguido opcionalmente dos sufixos @c '+' (se @c isStart) e @c '-'
 * (se @c isEnd). Os pontos são separados por vírgulas. Um ponto pode
 * ter ambos os sufixos se a variável é definida e usada na mesma instrução.
 *
 * @return String com os pontos separados por vírgulas e anotados.
 *         Exemplo: @c "1+,2,3,4,5,6-"
 *
 * @par Complexidade
 * O(P) onde P = número de pontos de programa.
 */

std::string LiveRange::toString() const{
    std::ostringstream oss;

    for (size_t i = 0; i < points.size(); ++i){
        if (i > 0) oss << ',';
        oss << points[i].line;
        if (points[i].isStart) oss << '+';
        if (points[i].isEnd) oss << '-';
    }

    return oss.str();
}
