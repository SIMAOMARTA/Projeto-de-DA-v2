#include "Web.h"
#include <sstream>
#include <map>
#include <algorithm>

// Construction

Web::Web(int id, const LiveRange& range) : id_(id), varName_(range.getVarName()){
    ranges_.push_back(range);
    for (int line : range.getLineSet())
        lineSet_.insert(line);
}

// Getters

int Web::getId() const { return id_; }
void Web::setId(int newId) { id_ = newId; }

const std::string& Web::getVarName() const { return varName_; }

const std::vector<LiveRange>& Web::getRanges() const { return ranges_; }

const std::set<int>& Web::getLineSet() const { return lineSet_; }

// Merge

void Web::absorb(const Web& other) {
    for (const auto& r : other.getRanges()) {
        ranges_.push_back(r);
        for (int line : r.getLineSet())
            lineSet_.insert(line);
    }
}

// Overlap / Interference

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

std::map<int, Web::LineAnnotation> Web::buildAnnotationMap() const {
    std::map<int, LineAnnotation> m;
    for (const auto& r : ranges_) {
        for (const auto& p : r.getPoints()) {
            if (p.isStart) m[p.line].hasStart = true;
            if (p.isEnd)   m[p.line].hasEnd   = true;
        }
    }
    return m;
}

bool Web::interferesWith(const Web& other) const {
    if (!overlaps(other)) return false;

    // Para cada linha partilhada, verifica se APENAS
    // um começa por definição (+) e o outro termina por uso (-)
    // Nesse caso específico NÃO interferem (secção 2.5 do enunciado)

    auto mapA = buildAnnotationMap();
    auto mapB = other.buildAnnotationMap();

    for (int line : lineSet_) {
        if (other.lineSet_.count(line) == 0) continue;

        // Anotações desta linha em cada web
        LineAnnotation annA = mapA.count(line) ? mapA.at(line) : LineAnnotation{};
        LineAnnotation annB = mapB.count(line) ? mapB.at(line) : LineAnnotation{};

        // Caso de não interferência:
        // A começa por definição NESTA linha E B termina por uso NESTA linha
        // → a definição de A "mata" o valor de B, não há sobreposição real
        bool aStartsHere = annA.hasStart;
        bool bEndsHere   = annB.hasEnd;
        bool bStartsHere = annB.hasStart;
        bool aEndsHere   = annA.hasEnd;

        // Se numa linha A só define e B só termina → não interfere nessa linha
        if (aStartsHere && !aEndsHere && bEndsHere && !bStartsHere) continue;
        // Simétrico: B define e A termina → não interfere nessa linha
        if (bStartsHere && !bEndsHere && aEndsHere && !aStartsHere) continue;

        // Em qualquer outro caso de sobreposição → interferem
        return true;
    }
    return false;
}

// Output

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