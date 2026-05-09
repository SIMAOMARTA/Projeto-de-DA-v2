#ifndef WEB_H
#define WEB_H

#include "LiveRange.h"
#include <string>
#include <vector>
#include <set>
#include <map>

class Web {
public:
    Web(int id, const LiveRange& range);

    int getId() const;
    const std::string& getVarName() const;
    const std::vector<LiveRange>& getRanges() const;
    const std::set<int>& getLineSet() const;

    void absorb(const Web& other);
    bool overlaps(const Web& other) const;
    bool interferesWith(const Web& other) const;
    std::string toString() const;

    void setId(int newId);

private:
    int                   id_;
    std::string           varName_;
    std::vector<LiveRange> ranges_;
    std::set<int>         lineSet_;

    struct LineAnnotation { bool hasStart = false; bool hasEnd = false; };
    std::map<int, LineAnnotation> buildAnnotationMap() const;
};

#endif // WEB_H