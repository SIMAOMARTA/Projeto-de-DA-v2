/**
 * @file LiveRange.h
 * @brief Declaração da classe LiveRange.
 *
 * Uma live range representa o intervalo de execução durante o qual o valor
 * de uma variável (ou temporária do compilador) é necessário. É a unidade
 * fundamental de entrada para a construção de webs e do grafo de
 * interferência.
 *
 * @details
 * No contexto do alocador de registos, o compilador analisa o fluxo de
 * controlo e determina, para cada variável, os pontos de programa em que
 * o seu valor está "vivo" (ainda pode vir a ser lido no futuro). Esses
 * pontos formam uma live range. Uma variável pode ter várias live ranges
 * correspondentes a distintos valores que assume ao longo da execução.
 *
 * Live ranges da mesma variável que se sobreponham (partilham pelo menos
 * uma linha) são fundidas em webs pelo InterferenceGraph::buildWebs().
 *
 * @see Web
 * @see InterferenceGraph
 * @see ProgramPoint
 */

#ifndef LIVE_RANGE_H
#define LIVE_RANGE_H

#include "ProgramPoint.h"
#include <string>
#include <vector>
#include <set>

/**
 * @class LiveRange
 * @brief Modela uma live range de uma variável como sequência de pontos de programa.
 *
 * Cada LiveRange está associada a uma variável identificada pelo seu nome
 * (@c varName) e contém uma sequência ordenada de ProgramPoint — as
 * instruções onde o valor da variável está vivo.
 *
 * Internamente mantém dois contêineres paralelos:
 *  - @c points : sequência de ProgramPoint na ordem de inserção (preserva
 *    a informação de anotação '+'/'-' para cada ponto).
 *  - @c lineSet : conjunto de números de linha, pré-construído no construtor,
 *    que acelera a verificação de interseção para O(P) em vez de O(P²).
 *
 * @note Esta classe é imutável após construção: não expõe mutadores.
 *       A fusão de live ranges em webs é responsabilidade da classe Web.
 */

class LiveRange {
public:
    /**
     * @brief Constrói uma LiveRange para uma variável com os pontos fornecidos.
     *
     * @details
     * Itera sobre @p points e insere cada número de linha em @c lineSet,
     * pré-construindo o conjunto para que chamadas futuras a intersects()
     * sejam O(P) em vez de O(P²).
     *
     * @param varName  Nome da variável associada a esta live range.
     *                 Não deve estar vazio; o Parser valida esta condição.
     * @param points   Lista de pontos de programa que compõem a live range.
     *                 Deve conter pelo menos um elemento.
     *
     * @pre @p varName não deve ser vazio.
     * @pre @p points não deve estar vazio.
     *
     * @par Complexidade
     * O(P log P), onde P = número de pontos, dominado pelas inserções no @c std::set.
     */

    LiveRange(const std::string& varName, const std::vector<ProgramPoint>& points);

    /**
     * @brief Devolve o nome da variável associada a esta live range.
     *
     * @return Referência constante para o nome da variável.
     *
     * @par Complexidade
     * O(1)
     */

    const std::string& getVarName() const;

    /**
     * @brief Devolve a lista de pontos de programa desta live range.
     *
     * Os pontos estão na ordem em que foram fornecidos ao construtor,
     * preservando as anotações @c isStart e @c isEnd de cada um.
     *
     * @return Referência constante para o vetor de ProgramPoint.
     *
     * @par Complexidade
     * O(1)
     */

    const std::vector<ProgramPoint>& getPoints() const;

    /**
     * @brief Devolve o conjunto de números de linha desta live range.
     *
     * Estrutura auxiliar pré-construída no construtor que permite que
     * intersects() opere em O(P) usando dois iteradores sobre conjuntos
     * ordenados, em vez de uma pesquisa quadrática.
     *
     * @return Referência constante para o @c std::set<int> de linhas ordenadas.
     *
     * @par Complexidade
     * O(1)
     */

    const std::set<int>& getLineSet() const;

    /**
     * @brief Verifica se esta live range partilha pelo menos uma linha com @p other.
     *
     * @details
     * Utiliza o algoritmo de merge de dois iteradores sobre os @c lineSets
     * ordenados: em cada passo avança o iterador com o valor mais pequeno;
     * se em algum momento os dois valores forem iguais, há interseção.
     * Esta técnica garante complexidade linear no tamanho dos conjuntos,
     * equivalente ao passo de merge do merge-sort.
     *
     * Este método é utilizado por InterferenceGraph::buildWebs() para
     * decidir se duas live ranges parciais da mesma variável devem ser
     * fundidas numa única web.
     *
     * @param other  A outra live range a comparar.
     * @return       @c true se houver pelo menos uma linha comum;
     *               @c false caso contrário.
     *
     * @par Complexidade
     * O(min(|A|, |B|)) no melhor caso (primeira linha já comum);
     * O(|A| + |B|) no pior caso (sem interseção), onde |A| e |B| são
     * os tamanhos dos respetivos @c lineSets.
     */

    bool intersects(const LiveRange& other) const;

    /**
     * @brief Devolve uma representação textual da live range.
     *
     * @details
     * Constrói a string iterando sobre @c points na ordem de inserção.
     * Cada ponto é representado pelo seu número de linha, seguido de
     * @c '+' se @c isStart for verdadeiro e de @c '-' se @c isEnd for
     * verdadeiro. Os pontos são separados por vírgulas.
     *
     * @return String com os pontos separados por vírgulas e anotados.
     *         Exemplo: @c "1+,2,3,4,5,6-"
     *
     * @par Complexidade
     * O(P) onde P = número de pontos de programa.
     */

    std::string toString() const;

private:
    /** @brief Nome da variável associada a esta live range. */

    std::string varName;

    /**
     * @brief Sequência de pontos de programa na ordem de inserção.
     *
     * Preserva as anotações @c isStart e @c isEnd de cada ponto, que
     * são necessárias para determinar interferência em Web::interferesWith().
     */

     std::vector<ProgramPoint> points;

    /**
     * @brief Conjunto pré-construído dos números de linha desta live range.
     *
     * Permite que intersects() opere em O(P) em vez de O(P²).
     * Construído no construtor a partir de @c points e mantido imutável
     * após a inicialização.
     */

     std::set<int> lineSet;
};

#endif // LIVE_RANGE_H
