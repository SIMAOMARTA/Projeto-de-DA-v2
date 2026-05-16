/**
 * @file ProgramPoint.h
 * @brief Definição de um ponto de programa numa live range.
 *
 * Um ponto de programa representa uma linha de código onde uma variável
 * está viva. Pode estar anotado com '+' (início de definição) e/ou '-'
 * (fim de utilização).
 */

#ifndef PROGRAM_POINT_H
#define PROGRAM_POINT_H

/**
 * @struct ProgramPoint
 * @brief Representa um ponto de programa (linha) dentro de uma live range.
 *
 * Cada ponto contém o número da linha e duas flags que indicam se a
 * variável é definida (isStart / '+') ou consumida pela última vez
 * (isEnd / '-') nessa linha.
 *
 * Exemplo:
 * @code
 *   ProgramPoint p(5, true, false);  // linha 5, definição da variável
 *   ProgramPoint q(9, false, true);  // linha 9, última utilização
 * @endcode
 */
struct ProgramPoint {
    /** @brief Número da linha do programa (1-based). */
    int line = 0;

    /**
     * @brief Indica que a variável é definida (escrita) nesta linha.
     * Corresponde ao sufixo '+' no ficheiro de live ranges.
     */
    bool isStart = false;

    /**
     * @brief Indica que esta é a última utilização da variável nesta linha.
     * Corresponde ao sufixo '-' no ficheiro de live ranges.
     */
    bool isEnd = false;

    /**
     * @brief Constrói um ProgramPoint com todos os campos explícitos.
     *
     * @param l  Número da linha (deve ser > 0).
     * @param s  @c true se a variável é definida nesta linha (@c '+').
     * @param e  @c true se esta é a última utilização (@c '-').
     * @par Complexidade O(1)
     */
    ProgramPoint(int l, bool s, bool e) : line(l), isStart(s), isEnd(e) {}
};

#endif // PROGRAM_POINT_H