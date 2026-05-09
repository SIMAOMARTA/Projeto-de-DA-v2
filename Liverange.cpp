#include "LiveRange.h"
#include <sstream>
#include <algorithm>

LiveRange::LiveRange(const std::string& varName,
                     const std::vector<ProgramPoint>& points)
    : varName_(varName), points_(points)
{
    for (const auto& p : points_)
        lineSet_.insert(p.line);
}

const std::string& LiveRange::getVarName() const {
    return varName_;
}

const std::vector<ProgramPoint>& LiveRange::getPoints() const {
    return points_;
}

const std::set<int>& LiveRange::getLineSet() const {
    return lineSet_;
}

bool LiveRange::intersects(const LiveRange& other) const {
    const auto& a = lineSet_;
    const auto& b = other.lineSet_;
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
    for (size_t i = 0; i < points_.size(); ++i) {
        if (i > 0) oss << ',';
        oss << points_[i].line;
        if (points_[i].isStart) oss << '+';
        if (points_[i].isEnd)   oss << '-';
    }
    return oss.str();
}