#ifndef LIVE_RANGE_H
#define LIVE_RANGE_H

#include "ProgramPoint.h"
#include <string>
#include <vector>
#include <set>

class LiveRange {
public:
    LiveRange(const std::string& varName, const std::vector<ProgramPoint>& points);

    const std::string& getVarName() const;
    const std::vector<ProgramPoint>& getPoints() const;
    const std::set<int>& getLineSet() const;

    bool intersects(const LiveRange& other) const;
    std::string toString() const;

private:
    std::string              varName;
    std::vector<ProgramPoint> points;
    std::set<int>            lineSet;   // pre construido para que as procuras sejam O(log n)
};

#endif // LIVE_RANGE_H