#include <iostream>

#ifdef _WIN32
#include <windows.h> // isto é só por causa dos acento e letras em windows dont worry
#endif

int main(int argc, char* argv[]) {

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    return 0;
}
