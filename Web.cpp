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

Web::Web(int id, const LiveRange& range) : id_(id), varName_(range.getVarName()){
    // Inicializa a web copiando o lineSet e pré-calculando a cache de anotações
    ranges_.push_back(range);
    for (int line : range.getLineSet())
        lineSet_.insert(line);
    buildAnnotationMap();
}

int Web::getId() const { return id_; }

void Web::setId(int newId) { id_ = newId; }

const std::string& Web::getVarName() const { return varName_; }

const std::vector<LiveRange>& Web::getRanges() const { return ranges_; }

const std::set<int>& Web::getLineSet() const { return lineSet_; }

void Web::absorb(const Web& other){
    // Funde destrutivamente e unilateralmente
    for (const auto& r : other.getRanges()){
        ranges_.push_back(r);
        for (int line : r.getLineSet())
            lineSet_.insert(line);
    }

    // Reconstrói a cache garantindo que ela reflete a união de todas as ranges
    buildAnnotationMap();
}

bool Web::overlaps(const Web& other) const{
    // Algoritmo de merge iterando sobre os std::set. Se houver valores iguais, há sobreposição.
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

void Web::buildAnnotationMap(){
    // Cria uma cache que permite a interferesWith() consultar as anotações
    // em O(log L) vez de iterar por todos os pontos de todas as ranges.
    annotationMap_.clear();
    for (const auto& r : ranges_){
        for (const auto& p : r.getPoints()){
            if (p.isStart) annotationMap_[p.line].hasStart = true;
            if (p.isEnd) annotationMap_[p.line].hasEnd = true;
        }
    }
}

bool Web::interferesWith(const Web& other) const{
    // Algoritmo de interferência:
    // 1. Sem sobreposição = sem interferência.
    // 2. Itera apenas sobre as linhas partilhadas usando as caches O(log L).
    // 3. Regra de não interferência (def/uso na mesma instrução): se A termina e B começa, não interferem.
    // 4. Em qualquer outro caso (ambas vivas, ambas definem, etc), a interferência é confirmada.

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

        // Casos de não-interferência (Definição e Uso na mesma linha)
        if (aStartsHere && !aEndsHere && bEndsHere && !bStartsHere) continue;
        if (bStartsHere && !bEndsHere && aEndsHere && !aStartsHere) continue;

        return true;
    }

    return false;
}

std::string Web::toString() const{
    // Constrói um std::map combinando as flags com um OR lógico.
    // O std::map garante que os pontos estarão ordenados automaticamente de forma crescente.
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