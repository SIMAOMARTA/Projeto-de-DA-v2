#ifndef PROGRAM_POINT_H
#define PROGRAM_POINT_H

struct ProgramPoint {
    int line = 0;
    bool isStart = false;
    bool isEnd = false;

    // l numero da linha
    // s se é '+'
    // e se é '-'
    ProgramPoint(int l, bool s, bool e) : line(l), isStart(s), isEnd(e) {}
};

#endif // PROGRAM_POINT_H