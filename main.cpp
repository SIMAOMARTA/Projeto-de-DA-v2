#include <iostream>
#include "LiveRange.cpp"
#include "Web.cpp"
#include "Web.h"
#include "LiveRange.h"

#ifdef _WIN32
#include <windows.h> // isto é só por causa dos acento e letras em windows dont worry
#endif

int main() {

    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    #endif

    Web sum(
        0,
        LiveRange("sum", {
            {1, true, false},
            {2, false, true}
        })
    );

    Web i(
        1,
        LiveRange("i", {
            {2, true, false},
            {3, false, true}
        })
    );

    Web t0(
        2,
        LiveRange("t0", {
            {3, true, false},
            {4, false, false},
            {5, false, true}
        })
    );

    Web t1(
        3,
        LiveRange("t1", {
            {4, true, false},
            {5, false, true}
        })
    );

    Web res(
        4,
        LiveRange("res", {
            {5, true, false},
            {6, false, true}
        })
    );

    std::cout << "sum vs t0: " << sum.interferesWith(t0) << '\n';
    std::cout << "sum vs t1: " << sum.interferesWith(t1) << '\n';
    std::cout << "i vs t0: " << i.interferesWith(t0) << '\n';
    std::cout << "t0 vs t1: " << t0.interferesWith(t1) << '\n';
    std::cout << "t1 vs res: " << t1.interferesWith(res) << '\n';

    return 0;
}