/**
 * @file Web.h
 * @brief Declaração da classe Web.
 * Uma web é a união de uma ou mais LiveRanges da mesma variável cujos
 * intervalos se sobrepõem. É o nó fundamental do grafo de interferência.
 */

#ifndef WEB_H
#define WEB_H

#include "LiveRange.h"
#include <string>
#include <vector>
#include <set>
#include <map>

class Web {
public:
    /**
     * @brief Constrói uma Web a partir de uma LiveRange inicial.
     * @param id     Identificador numérico único (0-based no grafo).
     * @param range  LiveRange inicial que compõe esta web.
     */
    Web(int id, const LiveRange& range);

    /**
     * @brief Devolve o identificador numérico desta web.
     * @return Id inteiro (0-based).
     */
    int getId() const;

    /**
     * @brief Devolve o nome da variável associada a esta web.
     * @return Referência constante para o nome.
     */
    const std::string& getVarName() const;

    /**
     * @brief Devolve todas as LiveRanges que compõem esta web.
     * @return Referência constante para o vetor de ranges.
     */
    const std::vector<LiveRange>& getRanges() const;

    /**
     * @brief Devolve o conjunto de linhas cobertas por esta web.
     * Equivale à união dos lineSets de todas as ranges fundidas.
     * @return Referência constante para o set de inteiros.
     */
    const std::set<int>& getLineSet() const;


    /**
     * @brief Funde a web @p other nesta web.
     * @param other  Web a fundir (não é modificada, só lida).
     */
    void absorb(const Web& other);

    /**
     * @brief Verifica se esta web partilha pelo menos uma linha com @p other.
     * @param other  Web a comparar.
     * @return       @c true se houver pelo menos uma linha comum.
     */
    bool overlaps(const Web& other) const;

    /**
    * @brief Determina se esta web interfere com @p other.
    * @param other  Web a comparar.
    * @return       @c true se as webs interferem.
    */
    bool interferesWith(const Web& other) const;

    /**
    * @brief Devolve uma representação textual da web.
    * @return String com os pontos de programa ordenados.
    */
    std::string toString() const;

    /**
    * @brief Redefine o identificador desta web.
    * @param newId  Novo identificador (deve ser >= 0).
    */
    void setId(int newId);

private:
    int                   id_;
    std::string           varName_;
    std::vector<LiveRange> ranges_;
    std::set<int>         lineSet_;

    /**
    * @struct LineAnnotation
    * @brief Cache de anotações (isStart / isEnd) por linha de programa.
    * Pré-computada por buildAnnotationMap() e consultada em
    * interferesWith() para evitar iterar sobre todos os pontos a cada
    * chamada.
    */
    struct LineAnnotation { bool hasStart = false; bool hasEnd = false; };
    std::map<int, LineAnnotation> annotationMap_;

    /**
     * @brief Reconstrói annotationMap_ a partir das ranges atuais.
     */
    void buildAnnotationMap();
};

#endif