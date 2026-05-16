/**
 * @file Web.cpp
 * @brief Implementação da classe Web.
 *
 * Contém a lógica de fusão de live ranges, deteção de sobreposição,
 * determinação de interferência e representação textual.
 */

#include "Web.h"
#include <sstream>
#include <algorithm>

//Construção
/**
 * @brief Constrói uma Web a partir de uma LiveRange inicial.
 * @details Inicializa a web com a LiveRange fornecida, copia as suas linhas
 * para lineSet_ e constrói a cache inicial.
 *
 * @par Complexidade
 * O(P log P) onde P = número de pontos da range.
 */
Web::Web(int id, const LiveRange& range) : id_(id), varName_(range.getVarName()) {
    ranges_.push_back(range);
    for (int line : range.getLineSet())
        lineSet_.insert(line);
    buildAnnotationMap();   // cache inicial
}

// Getters
/**
 * @brief Devolve o identificador numérico desta web.
 * @par Complexidade O(1)
 */
int Web::getId() const { return id_; }

/**
 * @brief Redefine o identificador desta web.
 * @par Complexidade O(1)
 */
void Web::setId(int newId) { id_ = newId; }

/**
 * @brief Devolve o nome da variável associada a esta web.
 * @par Complexidade O(1)
 */
const std::string& Web::getVarName() const { return varName_; }

/**
 * @brief Devolve todas as LiveRanges que compõem esta web.
 * @par Complexidade O(1)
 */
const std::vector<LiveRange>& Web::getRanges() const { return ranges_; }

/**
 * @brief Devolve o conjunto de linhas cobertas por esta web.
 * @par Complexidade O(1)
 */
const std::set<int>& Web::getLineSet() const { return lineSet_; }

// Fusão
/**
 * @brief Funde a web @p other nesta web.
 * @details Para cada LiveRange de @p other:
 *  1. Adiciona a range ao vetor ranges_.
 *  2. Insere todas as suas linhas em lineSet_.
 * Após processar todas as ranges, reconstrói annotationMap_ a partir do
 * estado completo (para garantir consistência da cache).
 *
 * @par Complexidade
 * O(P log P), onde P = total de pontos de ambas as webs.
 */
void Web::absorb(const Web& other) {
    // Adiciona os ranges do outro
    for (const auto& r : other.getRanges()) {
        ranges_.push_back(r);
        for (int line : r.getLineSet())
            lineSet_.insert(line);
    }
    // Recalcula a cache com todas as ranges
    buildAnnotationMap();
}

// Sobreposição
/**
 * @brief Verifica se esta web partilha pelo menos uma linha com @p other.
 * @details Utiliza dois iteradores sobre os sets ordenados.
 * Se em algum passo os dois iteradores apontam para o mesmo
 * valor, há sobreposição.
 *
 * Esta função é chamada por InterferenceGraph::buildWebs() para decidir se
 * duas webs parciais devem ser fundidas, e por interferesWith() como
 * condição de saída rápida.
 *
 * @par Complexidade
 * O(min(|L_this|, |L_other|)) no melhor caso; O(|L_this| + |L_other|) no pior caso.
 */
bool Web::overlaps(const Web& other) const {
    const auto& a = lineSet_;
    const auto& b = other.lineSet_;
    auto it1 = a.begin(), it2 = b.begin();
    while (it1 != a.end() && it2 != b.end()) {
        if (*it1 == *it2) return true;
        else if (*it1 < *it2) ++it1;
        else ++it2;
    }
    return false;
}

// Construção da cache (privada)
/**
 * @brief Reconstrói annotationMap_ a partir das ranges atuais.
 * @details
 * Limpa o mapa anterior e itera sobre todos os pontos de todas as ranges.
 * Para cada ponto:
 *  - se isStart, ativa hasStart na entrada correspondente do mapa.
 *  - se isEnd, ativa hasEnd na entrada correspondente do mapa.
 *
 * O mapa resultante permite que interferesWith() consulte as anotações
 * de qualquer linha em O(log L) em vez de percorrer todos os pontos.
 * Deve ser chamada após cada absorb() para manter a cache consistente.
 *
 * @par Complexidade
 * O(P), onde P = total de pontos em todas as ranges.
 */
void Web::buildAnnotationMap() {
    annotationMap_.clear();
    for (const auto& r : ranges_) {
        for (const auto& p : r.getPoints()) {
            if (p.isStart) annotationMap_[p.line].hasStart = true;
            if (p.isEnd)   annotationMap_[p.line].hasEnd   = true;
        }
    }
}

// Interferência
/**
 * @brief Determina se esta web interfere com @p other.
 *
 * @details
 * Algoritmo:
 *  1. Se as webs não se sobrepõem (overlaps() == false), devolve @c false imediatamente.
 *  2. Para cada linha partilhada, obtém as anotações de cada web
 *     usando a annotationMap_ pré-computada.
 *  3. Regra de não-interferência (definição/uso na mesma linha):
 *     - Se a web A inicia na linha (isStart=true, isEnd=false) e a web B
 *       termina na linha (isEnd=true, isStart=false), não interferem nessa
 *       linha — A define depois de B terminar, pelo que não há simultaneidade.
 *     - O simétrico (B inicia, A termina) também não interfere.
 *  4. Em qualquer outro caso (ambas vivas, ou ambas definem/usam na mesma
 *     linha), há interferência e a função devolve @c true imediatamente.
 *
 * @par Complexidade
 * O(L log L), onde L = número de linhas partilhadas pelas duas webs.
 */
bool Web::interferesWith(const Web& other) const {
    // Pré‑condição: overlaps() garante que há linhas partilhadas
    if (!overlaps(other)) return false;

    // Usa as caches já calculadas em vez de construir mapas temporários
    const auto& mapA = annotationMap_;
    const auto& mapB = other.annotationMap_;

    for (int line : lineSet_) {
        if (other.lineSet_.count(line) == 0) continue;

        // Obtém anotações da linha nas duas webs
        LineAnnotation annA = mapA.count(line) ? mapA.at(line) : LineAnnotation{};
        LineAnnotation annB = mapB.count(line) ? mapB.at(line) : LineAnnotation{};

        bool aStartsHere = annA.hasStart;
        bool bEndsHere   = annB.hasEnd;
        bool bStartsHere = annB.hasStart;
        bool aEndsHere   = annA.hasEnd;

        // Casos de não interferência
        if (aStartsHere && !aEndsHere && bEndsHere && !bStartsHere) continue;
        if (bStartsHere && !bEndsHere && aEndsHere && !aStartsHere) continue;

        return true;
    }
    return false;
}

// Representação textual
/**
 * @brief Devolve uma representação textual da web.
 *
 * @details
 * Constrói um mapa temporário linha -> (isStart, isEnd) percorrendo todos
 * os pontos de todas as ranges, combinando os flags com OR lógico.
 * O std::map ordena automaticamente as linhas por ordem crescente.
 * O resultado é a concatenação dos pares linha+sufixos separados por vírgulas.
 *
 * @par Complexidade
 * O(P log P) onde P = total de pontos de programa em todas as ranges.
 */
std::string Web::toString() const {
    std::map<int, std::pair<bool,bool>> merged;
    for (const auto& r : ranges_)
        for (const auto& p : r.getPoints()) {
            merged[p.line].first  |= p.isStart;
            merged[p.line].second |= p.isEnd;
        }

    std::ostringstream oss;
    bool first = true;
    for (const auto& [line, ann] : merged) {
        if (!first) oss << ',';
        first = false;
        oss << line;
        if (ann.first)  oss << '+';
        if (ann.second) oss << '-';
    }
    return oss.str();
}