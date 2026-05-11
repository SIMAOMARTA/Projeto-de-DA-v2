#include "LiveRange.h"
#include <sstream>
#include <algorithm>

LiveRange::LiveRange(const std::string& varName,
                     const std::vector<ProgramPoint>& points)
    : varName(varName), points(points)
{
    for (const auto& p : points)
        lineSet.insert(p.line);
}

const std::string& LiveRange::getVarName() const {
    return varName;
}

const std::vector<ProgramPoint>& LiveRange::getPoints() const {
    return points;
}

const std::set<int>& LiveRange::getLineSet() const {
    return lineSet;
}

bool LiveRange::intersects(const LiveRange& other) const {
    const auto& a = lineSet;
    const auto& b = other.lineSet;
    auto it1 = a.begin(), it2 = b.begin();
    while (it1 != a.end() && it2 != b.end()) {
        if (*it1 == *it2) return true;
        if (*it1 < *it2) ++it1;
        else ++it2;
    }
    return false;
}

std::string LiveRange::toString() const {
    std::ostringstream oss;
    for (size_t i = 0; i < points.size(); ++i) {
        if (i > 0) oss << ',';
        oss << points[i].line;
        if (points[i].isStart) oss << '+';
        if (points[i].isEnd)   oss << '-';
    }
    return oss.str();
}