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

    auto mapA = buildAnnotationMap();
    auto mapB = other.buildAnnotationMap();

    for (int line : lineSet_) {
        if (other.lineSet_.count(line) == 0) continue;

        const LineAnnotation& annA = mapA.count(line) ? mapA[line] : LineAnnotation{};
        const LineAnnotation& annB = mapB.count(line) ? mapB[line] : LineAnnotation{};

        bool aOnlyStart = annA.hasStart && !annA.hasEnd;
        bool bOnlyEnd   = annB.hasEnd   && !annB.hasStart;
        if (aOnlyStart && bOnlyEnd) continue;

        bool bOnlyStart = annB.hasStart && !annB.hasEnd;
        bool aOnlyEnd   = annA.hasEnd   && !annA.hasStart;
        if (bOnlyStart && aOnlyEnd) continue;

        return true;
    }
    return false;
}

// Output

std::string Web::toString() const {
    struct Entry {
        int  line;
        bool isStart;
        bool isEnd;
    };
    std::vector<Entry> entries;
    for (const auto& r : ranges_) {
        for (const auto& p : r.getPoints()) {
            entries.push_back({p.line, p.isStart, p.isEnd});
        }
    }
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        if (a.line != b.line) return a.line < b.line;
        if (a.isStart != b.isStart) return a.isStart > b.isStart;
        return a.isEnd < b.isEnd;
    });

    std::ostringstream oss;
    bool first = true;
    for (const auto& e : entries) {
        if (!first) oss << ',';
        first = false;
        oss << e.line;
        if (e.isStart) oss << '+';
        if (e.isEnd)   oss << '-';
    }
    return oss.str();
}