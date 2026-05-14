/**
 * @file LiveRange.h
 * @brief Declaração da classe LiveRange.
 *
 * Uma live range representa o intervalo de execução durante o qual o valor
 * de uma variável é necessário. É a unidade fundamental de entrada para a
 * construção de webs e do grafo de interferência.
 */

#ifndef LIVE_RANGE_H
#define LIVE_RANGE_H

#include "ProgramPoint.h"
#include <string>
#include <vector>
#include <set>

/**
 * @class LiveRange
 * @brief Modela uma live range de uma variável como conjunto de pontos de programa.
 *
 * Cada LiveRange está associada a uma variável (identificada pelo nome) e
 * contém uma sequência ordenada de ProgramPoint — as linhas onde o valor
 * da variável está vivo.
 *
 * Uma variável pode ter várias LiveRanges.
 * Ranges sobrepostas da mesma variável são fundidas em webs pelo
 * InterferenceGraph::buildWebs().
 */
class LiveRange {
public:
    /**
     * @brief Constrói uma LiveRange para uma variável com os pontos fornecidos.
     * Preenche automaticamente o lineSet a partir de @p points.
     * @param varName  Nome da variável (não pode ser vazio).
     * @param points   Lista ordenada de pontos de programa.
     * @pre  @p points não deve estar vazio.
     */
    LiveRange(const std::string& varName, const std::vector<ProgramPoint>& points);

    /**
     * @brief Devolve o nome da variável associada a esta live range.
     * @return Referência constante para o nome da variável.
     */
    const std::string& getVarName() const;

    /**
     * @brief Devolve a lista de pontos de programa desta live range.
     * Os pontos estão na ordem em que foram fornecidos ao construtor.
     * @return Referência constante para o vetor de ProgramPoint.
     */
    const std::vector<ProgramPoint>& getPoints() const;

    /**
     * @brief Devolve o conjunto de números de linha desta live range.
     * Estrutura pré-construída no construtor para pesquisas O(log P).
     * @return Referência constante para o set de inteiros.
     */
    const std::set<int>& getLineSet() const;

    /**
     * @brief Verifica se esta live range partilha pelo menos uma linha com @p other.
     * Utiliza dois iteradores sobre os conjuntos ordenados para obter
     * complexidade linear em vez de quadrática.
     * @param other  A outra live range a comparar.
     * @return       @c true se houver pelo menos uma linha comum.
     */
    bool intersects(const LiveRange& other) const;

    /**
     * @brief Devolve uma representação textual da live range.
     * Exemplo: @c "1+,2,3,4,5,6-"
     * @return String com a representação da live range.
     */
    std::string toString() const;

private:
    /** @brief Nome da variável. */
    std::string              varName;

    /** @brief Sequência de pontos de programa (na ordem de inserção). */
    std::vector<ProgramPoint> points;

    /**
     * @brief Conjunto pré-construído de números de linha.
     * Permite que intersects() corra em O(P) em vez de O(P²).
     */
    std::set<int>            lineSet;   // pre construido para que as procuras sejam O(log n)
};

#endif // LIVE_RANGE_H