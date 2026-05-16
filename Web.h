/**
 * @file Web.h
 * @brief Declaração da classe Web.
 *
 * Uma web é a união de uma ou mais LiveRanges da mesma variável cujos
 * intervalos de programa se sobrepõem. É o nó fundamental do grafo de
 * interferência: cada web corresponde a um valor distinto da variável
 * que pode ser atribuído a um registo físico independente.
 *
 * @details
 * A construção de webs segue um algoritmo greedy de fusão descrito em
 * InterferenceGraph::buildWebs(): parte de uma web por live range e funde
 * iterativamente pares de webs da mesma variável que se sobreponham,
 * até nenhum par se sobrepor. O resultado é o conjunto minimal de webs
 * tal que cada web é uma live range maximal conexa.
 *
 * Internamente, a Web mantém:
 *  - @c ranges_    : as live ranges originais fundidas nesta web.
 *  - @c lineSet_   : união dos @c lineSets de todas as ranges (para pesquisas rápidas).
 *  - @c annotationMap_ : cache de anotações (+/-) por linha, usada em interferesWith().
 *
 * @see LiveRange
 * @see InterferenceGraph
 */

#ifndef WEB_H
#define WEB_H

#include "LiveRange.h"
#include <string>
#include <vector>
#include <set>
#include <map>

/**
 * @class Web
 * @brief Nó do grafo de interferência — união de live ranges sobrepostas de uma variável.
 *
 * Cada Web é identificada por um inteiro único no grafo (@c id_) e está
 * associada a uma variável (@c varName_). Pode conter uma ou mais LiveRanges
 * fundidas, cujo conjunto de linhas é mantido de forma incremental.
 *
 * A classe disponibiliza três operações principais:
 *  - @c overlaps()      : determina se esta web partilha linhas com outra.
 *  - @c absorb()        : funde outra web nesta (fusão destrutiva unilateral).
 *  - @c interferesWith(): determina se existe interferência real com outra web.
 *
 * A distinção entre sobreposição e interferência é importante: duas webs
 * podem partilhar uma linha sem interferir, caso uma termine precisamente
 * onde a outra começa (def/use na mesma instrução).
 */

class Web {
public:

    /**
     * @brief Constrói uma Web a partir de uma LiveRange inicial.
     *
     * @details
     * Inicializa a web com a live range fornecida, copia as suas linhas
     * para @c lineSet_ e constrói a cache @c annotationMap_ a partir dos
     * pontos da range.
     *
     * @param id     Identificador numérico único da web no grafo (0-based).
     * @param range  LiveRange inicial que compõe esta web.
     *
     * @par Complexidade
     * O(P log P), onde P = número de pontos de @p range.
     */

    Web(int id, const LiveRange& range);

    /**
     * @brief Devolve o identificador numérico desta web.
     * @return Id inteiro (0-based no grafo de interferência).
     * @par Complexidade O(1)
     */

    int getId() const;

    /**
     * @brief Redefine o identificador desta web.
     *
     * Chamado por InterferenceGraph::buildWebs() após a renumeração final
     * das webs, que ocorre após a fusão greedy de live ranges sobrepostas.
     *
     * @param newId  Novo identificador (deve ser >= 0).
     * @par Complexidade O(1)
     */

    void setId(int newId);

    /**
     * @brief Devolve o nome da variável associada a esta web.
     * @return Referência constante para o nome da variável.
     * @par Complexidade O(1)
     */

    const std::string& getVarName() const;

    /**
     * @brief Devolve todas as LiveRanges que compõem esta web.
     *
     * Inclui a range inicial e todas as ranges absorvidas por chamadas
     * a absorb(). A ordem corresponde à sequência de absorções.
     *
     * @return Referência constante para o vetor de LiveRange.
     * @par Complexidade O(1)
     */

    const std::vector<LiveRange>& getRanges() const;

    /**
     * @brief Devolve o conjunto de linhas cobertas por esta web.
     *
     * Equivale à união dos @c lineSets de todas as ranges fundidas.
     * Este conjunto é mantido incrementalmente em absorb() e pode ser
     * consultado em O(1).
     *
     * @return Referência constante para o @c std::set<int> de linhas ordenadas.
     * @par Complexidade O(1)
     */

    const std::set<int>& getLineSet() const;

    /**
     * @brief Funde a web @p other nesta web (absorção unilateral).
     *
     * @details
     * Para cada LiveRange de @p other:
     *  1. Adiciona a range ao vetor @c ranges_.
     *  2. Insere todas as suas linhas em @c lineSet_.
     *
     * Após processar todas as ranges, reconstrói @c annotationMap_ a
     * partir do estado completo para garantir a consistência da cache
     * usada em interferesWith().
     *
     * @note @p other não é modificada. Apenas esta web é alterada.
     *
     * @param other  Web a absorver.
     *
     * @par Complexidade
     * O(P log P), onde P = total de pontos em ambas as webs após fusão.
     */

    void absorb(const Web& other);

    /**
     * @brief Verifica se esta web partilha pelo menos uma linha com @p other.
     *
     * @details
     * Usa o algoritmo de merge sobre os @c lineSets_ ordenados (dois
     * iteradores avançando simultaneamente). É chamado por:
     *  - InterferenceGraph::buildWebs() para decidir fusão.
     *  - interferesWith() como condição de saída rápida.
     *
     * @param other  Web a comparar.
     * @return       @c true se houver pelo menos uma linha comum.
     *
     * @par Complexidade
     * O(min(|L_this|, |L_other|)) melhor caso; O(|L_this| + |L_other|) pior caso.
     */

    bool overlaps(const Web& other) const;

    /**
     * @brief Determina se esta web interfere com @p other.
     *
     * @details
     * Duas webs interferem se existir pelo menos uma linha em que ambas
     * estão simultaneamente vivas, ou seja, em que não se pode demonstrar
     * que uma termina exatamente onde a outra começa.
     *
     * O algoritmo:
     *  1. Se overlaps() for @c false, retorna @c false imediatamente.
     *  2. Para cada linha partilhada, consulta @c annotationMap_ de ambas
     *     as webs para obter as anotações @c hasStart / @c hasEnd.
     *  3. Aplica a regra de não-interferência:
     *     - Se a web A começa (@c isStart, sem @c isEnd) na linha e B
     *       termina (@c isEnd, sem @c isStart) na mesma linha → A define
     *       um novo valor após B consumir o seu último; não há sobreposição.
     *     - O simétrico (B começa, A termina) também não interfere.
     *  4. Em qualquer outro caso (ambas vivas, ambas definem/usam, etc.)
     *     há interferência → retorna @c true imediatamente.
     *
     * @param other  Web a comparar.
     * @return       @c true se as webs interferem (não podem partilhar registo).
     *
     * @par Complexidade
     * O(L log L), onde L = número de linhas partilhadas pelas duas webs.
     */

    bool interferesWith(const Web& other) const;

    /**
     * @brief Devolve uma representação textual da web no formato do enunciado.
     *
     * @details
     * Constrói um @c std::map temporário linha → (isStart, isEnd),
     * combinando as anotações de todos os pontos de todas as ranges com
     * OR lógico. O @c std::map ordena automaticamente as linhas por ordem
     * crescente. O resultado é a concatenação dos pares linha+sufixos
     * separados por vírgulas, no formato exigido para o ficheiro de saída.
     *
     * @return String com os pontos de programa ordenados.
     *         Exemplo: @c "1+,2,3,4,5,6-,9+,10,11-"
     *
     * @par Complexidade
     * O(P log P) onde P = total de pontos de programa em todas as ranges.
     */

    std::string toString() const;

private:

    /** @brief Identificador único desta web no grafo de interferência (0-based). */
    int id_;

    /** @brief Nome da variável original (ou nome derivado após splitting, e.g., @c "i_s0"). */
    std::string varName_;

    /** @brief Conjunto de todas as live ranges que foram fundidas nesta web. */
    std::vector<LiveRange> ranges_;

    /**
     * @brief União dos números de linha de todas as ranges fundidas.
     *
     * Mantido incrementalmente em absorb(). Permite overlaps() em O(P)
     * e é usado como conjunto de iteração em interferesWith().
     */

    std::set<int> lineSet_;

    /**
     * @struct LineAnnotation
     * @brief Cache de anotações (isStart / isEnd) por linha de instrução.
     *
     * Pré-computada por buildAnnotationMap() e consultada em
     * interferesWith() para determinar, em O(log L), se numa dada linha
     * a web começa, termina, ou simplesmente está viva.
     *
     * Evita iterar sobre todos os pontos de todas as ranges a cada chamada
     * a interferesWith(), reduzindo a complexidade de O(L × P) para O(L log L).
     */

    struct LineAnnotation{
        bool hasStart = false; ///< @c true se alguma range começa ('+') nesta linha.
        bool hasEnd   = false; ///< @c true se alguma range termina ('-') nesta linha.
    };

    /**
     * @brief Mapa linha → anotação (+/-), construído e atualizado por buildAnnotationMap().
     *
     * Usado exclusivamente por interferesWith() para consultas O(log L).
     */

    std::map<int, LineAnnotation> annotationMap_;

    /**
     * @brief Reconstrói @c annotationMap_ a partir do estado atual de @c ranges_.
     *
     * @details
     * Limpa o mapa e itera sobre todos os pontos de todas as ranges.
     * Para cada ponto com @c isStart=true, ativa @c hasStart na entrada
     * correspondente do mapa. Analogamente para @c isEnd. Deve ser chamada
     * após cada absorb() para manter a cache consistente.
     *
     * @par Complexidade
     * O(P log L), onde P = total de pontos, L = número de linhas distintas.
     */

    void buildAnnotationMap();
};

#endif // WEB_H
