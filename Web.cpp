#include "Web.h"
#include <sstream>
#include <algorithm>

// ---------- Construção ----------
Web::Web(int id, const LiveRange& range) : id_(id), varName_(range.getVarName()) {
    ranges_.push_back(range);
    for (int line : range.getLineSet())
        lineSet_.insert(line);
    buildAnnotationMap();   // cache inicial
}

// ---------- Getters ----------
int Web::getId() const { return id_; }
void Web::setId(int newId) { id_ = newId; }
const std::string& Web::getVarName() const { return varName_; }
const std::vector<LiveRange>& Web::getRanges() const { return ranges_; }
const std::set<int>& Web::getLineSet() const { return lineSet_; }

// ---------- Fusão ----------
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

// ---------- Sobreposição ----------
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

// ---------- Construção da cache (privada) ----------
void Web::buildAnnotationMap() {
    annotationMap_.clear();
    for (const auto& r : ranges_) {
        for (const auto& p : r.getPoints()) {
            if (p.isStart) annotationMap_[p.line].hasStart = true;
            if (p.isEnd)   annotationMap_[p.line].hasEnd   = true;
        }
    }
}

// ---------- Interferência ----------
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

// ---------- Representação textual ----------
std::string Web::toString() const {
    std::map<int, std::pair<bool,bool>> merged; // line → {isStart, isEnd}
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