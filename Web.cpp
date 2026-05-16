/**
 * @file Web.cpp
 * @brief Implementação da classe Web.
 *
 * @details
 * Contém a lógica de fusão de live ranges (absorb()), deteção de
 * sobreposição (overlaps()), determinação de interferência real
 * (interferesWith()) e representação textual (toString()). A cache
 * @c annotationMap_ é mantida consistente pela função privada
 * buildAnnotationMap(), que é invocada sempre que o estado das ranges
 * muda.
 *
 * @see Web.h para a documentação completa da interface pública.
 */

#include "Web.h"
#include <sstream>

/**
 * @brief Constrói uma Web a partir de uma LiveRange inicial.
 *
 * @details
 * Inicializa a web com a live range fornecida: copia o seu @c lineSet
 * para @c lineSet_ e invoca buildAnnotationMap() para pré-calcular a
 * cache de anotações usada em interferesWith().
 *
 * @param id     Identificador numérico único da web no grafo (0-based).
 * @param range  LiveRange inicial que compõe esta web.
 *
 * @par Complexidade
 * O(P log P), onde P = número de pontos de @p range.
 */

Web::Web(int id, const LiveRange& range) : id_(id), varName_(range.getVarName()){
    ranges_.push_back(range);
    for (int line : range.getLineSet())
        lineSet_.insert(line);
    buildAnnotationMap();
}

/**
 * @brief Devolve o identificador numérico desta web.
 * @return Id inteiro (0-based).
 * @par Complexidade O(1)
 */

int Web::getId() const { return id_; }

/**
 * @brief Redefine o identificador desta web.
 *
 * Chamado por InterferenceGraph::buildWebs() durante a renumeração final
 * das webs (após a fusão greedy das live ranges sobrepostas), de forma
 * a garantir que os ids são contíguos e coerentes com os índices usados
 * na estrutura @c Graph<int> subjacente.
 *
 * @param newId  Novo identificador (deve ser >= 0).
 * @par Complexidade O(1)
 */

void Web::setId(int newId) { id_ = newId; }

/**
 * @brief Devolve o nome da variável associada a esta web.
 * @return Referência constante para o nome da variável.
 * @par Complexidade O(1)
 */

const std::string& Web::getVarName() const { return varName_; }

/**
 * @brief Devolve todas as LiveRanges que compõem esta web.
 * @return Referência constante para o vetor de LiveRange.
 * @par Complexidade O(1)
 */

const std::vector<LiveRange>& Web::getRanges() const { return ranges_; }

/**
 * @brief Devolve o conjunto de linhas cobertas por esta web.
 * @return Referência constante para o @c std::set<int> de linhas ordenadas.
 * @par Complexidade O(1)
 */

const std::set<int>& Web::getLineSet() const { return lineSet_; }

/**
 * @brief Funde a web @p other nesta web (absorção unilateral).
 *
 * @details
 * Para cada LiveRange de @p other:
 *  1. Adiciona a range ao vetor @c ranges_.
 *  2. Insere todas as suas linhas em @c lineSet_ (usando a referência
 *     ao @c lineSet da range, sem iterar sobre os pontos).
 *
 * Após processar todas as ranges, reconstrói @c annotationMap_ invocando
 * buildAnnotationMap() sobre o estado completo, garantindo que a cache
 * reflita a união de todas as ranges após a absorção.
 *
 * @note @p other não é modificada — apenas esta web é alterada.
 *
 * @param other  Web a absorver.
 *
 * @par Complexidade
 * O(P log P), onde P = total de pontos em ambas as webs após fusão,
 * dominado pela reconstrução de @c annotationMap_.
 */

void Web::absorb(const Web& other){
    for (const auto& r : other.getRanges()){
        ranges_.push_back(r);
        for (int line : r.getLineSet())
            lineSet_.insert(line);
    }
    buildAnnotationMap();
}

/**
 * @brief Verifica se esta web partilha pelo menos uma linha com @p other.
 *
 * @details
 * Usa o algoritmo de merge sobre dois iteradores dos @c lineSet_ ordenados.
 * Em cada passo avança o iterador cujo valor corrente é menor. Se em
 * algum ponto ambos os iteradores apontam para o mesmo valor, há
 * sobreposição e a função retorna @c true imediatamente.
 *
 * Este método é chamado em dois contextos distintos:
 *  - Por InterferenceGraph::buildWebs() para decidir se duas webs parciais
 *    da mesma variável devem ser fundidas.
 *  - Por interferesWith() como verificação preliminar: se não há
 *    sobreposição, não pode haver interferência.
 *
 * @param other  Web a comparar.
 * @return       @c true se houver pelo menos uma linha comum.
 *
 * @par Complexidade
 * O(min(|L_this|, |L_other|)) melhor caso (primeira linha já comum);
 * O(|L_this| + |L_other|) pior caso (sem linhas comuns).
 */

bool Web::overlaps(const Web& other) const{
    const auto& a = lineSet_;
    const auto& b = other.lineSet_;
    auto it1 = a.begin(), it2 = b.begin();

    while (it1 != a.end() && it2 != b.end()){
        if (*it1 == *it2) return true;
        else if (*it1 < *it2) ++it1;
        else ++it2;
    }

    return false;
}

/**
 * @brief Reconstrói @c annotationMap_ a partir do estado atual de @c ranges_.
 *
 * @details
 * Limpa o mapa e itera sobre todos os pontos de todas as ranges.
 * Para cada ponto com @c isStart=true, ativa @c hasStart na entrada
 * correspondente do mapa. Analogamente para @c isEnd.
 *
 * A cache resultante permite que interferesWith() consulte as anotações
 * de qualquer linha em O(log L), em vez de percorrer todos os pontos
 * a cada chamada.
 *
 * Deve ser invocada após cada operação que altere @c ranges_:
 * construtor e absorb().
 *
 * @par Complexidade
 * O(P log L), onde P = total de pontos em todas as ranges,
 * L = número de linhas distintas.
 */

void Web::buildAnnotationMap(){
    annotationMap_.clear();
    for (const auto& r : ranges_){
        for (const auto& p : r.getPoints()){
            if (p.isStart) annotationMap_[p.line].hasStart = true;
            if (p.isEnd) annotationMap_[p.line].hasEnd = true;
        }
    }
}

/**
 * @brief Determina se esta web interfere com @p other.
 *
 * @details
 * Duas webs interferem se, numa linha que partilham, não se puder
 * garantir que uma termina exatamente quando a outra começa (sem
 * sobreposição simultânea de valores vivos).
 *
 * Algoritmo:
 *  1. Se overlaps() == @c false, não há linhas partilhadas → retorna @c false.
 *  2. Para cada linha do @c lineSet_ desta web que também pertença ao
 *     @c lineSet_ de @p other, consulta as anotações de ambas através
 *     de @c annotationMap_ (O(log L) por consulta).
 *  3. Regra de não-interferência (definição/uso na mesma instrução):
 *     - Se a web A apenas começa na linha (@c hasStart=true, @c hasEnd=false)
 *       e a web B apenas termina (@c hasEnd=true, @c hasStart=false) →
 *       A define um novo valor depois de B consumir o seu último; não
 *       há simultaneidade → @c continue.
 *     - O simétrico (B começa, A termina) também não interfere.
 *  4. Qualquer outro caso (ambas vivas, ambas com def+uso, etc.) →
 *     interferência confirmada → retorna @c true.
 *  5. Se nenhuma linha partilhada originou interferência → retorna @c false.
 *
 * @param other  Web a comparar.
 * @return       @c true se as webs interferem (não podem partilhar registo).
 *
 * @par Complexidade
 * O(L log L), onde L = número de linhas partilhadas pelas duas webs.
 */

bool Web::interferesWith(const Web& other) const{
    if (!overlaps(other)) return false;

    const auto& mapA = annotationMap_;
    const auto& mapB = other.annotationMap_;

    for (int line : lineSet_){
        if (other.lineSet_.count(line) == 0) continue;

        LineAnnotation annA = mapA.count(line) ? mapA.at(line) : LineAnnotation{};
        LineAnnotation annB = mapB.count(line) ? mapB.at(line) : LineAnnotation{};

        bool aStartsHere = annA.hasStart;
        bool aEndsHere = annA.hasEnd;
        bool bStartsHere = annB.hasStart;
        bool bEndsHere = annB.hasEnd;

        // Casos de não-interferência
        if (aStartsHere && !aEndsHere && bEndsHere && !bStartsHere) continue;
        if (bStartsHere && !bEndsHere && aEndsHere && !aStartsHere) continue;

        return true;
    }

    return false;
}

/**
 * @brief Devolve uma representação textual da web no formato do enunciado.
 *
 * @details
 * Constrói um @c std::map temporário linha → (isStart, isEnd), iterando
 * sobre todos os pontos de todas as ranges e combinando as flags com OR
 * lógico (um ponto num @c range já absorvido pode ter isStart; outro range
 * pode ter isEnd para a mesma linha). O @c std::map garante ordenação
 * automática por número de linha. O resultado é a concatenação dos pares
 * linha+sufixos separados por vírgulas.
 *
 * @return String com os pontos de programa ordenados em ordem crescente.
 *         Exemplo: @c "1+,2,3,4,5,6-,9+,10,11-"
 *
 * @par Complexidade
 * O(P log P) onde P = total de pontos de programa em todas as ranges.
 */

std::string Web::toString() const{
    std::map<int, std::pair<bool,bool>> merged;
    for (const auto& r : ranges_)
        for (const auto& p : r.getPoints()){
            merged[p.line].first |= p.isStart;
            merged[p.line].second |= p.isEnd;
        }

    std::ostringstream oss;
    bool first = true;
    for (const auto& [line, ann] : merged){
        if (!first) oss << ',';
        first = false;
        oss << line;
        if (ann.first) oss << '+';
        if (ann.second) oss << '-';
    }

    return oss.str();
}
