/**
 * @file ProgramPoint.h
 * @brief Definição de um ponto de programa numa live range.
 *
 * Um ponto de programa representa uma instrução (linha) do código
 * intermédio de 3 endereços onde uma variável está viva. Pode estar
 * anotado com '+' para marcar o início de uma definição (escrita) e/ou
 * com '-' para marcar o último uso (leitura) da variável nessa instrução.
 *
 * @details
 * Estas anotações são essenciais para determinar se duas webs interferem
 * numa linha partilhada. Especificamente, se a web A começa (@c isStart)
 * numa linha em que a web B termina (@c isEnd), e não há sobreposição
 * inversa, as duas webs **não interferem** nessa linha — A define um
 * novo valor apenas depois de B ter consumido o seu último valor.
 *
 * @see Web::interferesWith()
 * @see LiveRange
 */

#ifndef PROGRAM_POINT_H
#define PROGRAM_POINT_H

/**
 * @struct ProgramPoint
 * @brief Representa um ponto de programa (instrução) dentro de uma live range.
 *
 * Cada ponto contém o número da linha de instrução e duas flags booleanas
 * que anotam eventos de definição (@c isStart / '+') e de último uso
 * (@c isEnd / '-') nessa instrução.
 *
 * @note O mesmo ponto pode ter ambas as flags ativas simultaneamente,
 * como acontece em instruções do tipo @c "i = i + 1", onde a variável
 * @c i é lida (último uso da live range anterior) e definida (início
 * da nova live range) na mesma instrução.
 *
 * @par Exemplo de uso
 * @code
 *   ProgramPoint def(5, true, false);   // linha 5: definição de variável
 *   ProgramPoint use(9, false, true);   // linha 9: última utilização
 *   ProgramPoint both(7, true, true);   // linha 7: def. e último uso
 * @endcode
 */

struct ProgramPoint {
    /**
     * @brief Número da linha da instrução (1-based).
     *
     * Corresponde diretamente ao número de linha do ficheiro de live
     * ranges de entrada. Deve ser estritamente positivo.
     */

    int line = 0;

    /**
     * @brief Indica que a variável é definida (escrita) nesta instrução.
     *
     * Corresponde ao sufixo @c '+' no ficheiro de live ranges.
     * Quando @c true, esta instrução é o ponto de início de uma nova
     * live range para a variável — o compilador gera aqui uma escrita
     * para o registo ou posição de memória associada.
     */

    bool isStart = false;

    /**
     * @brief Indica que esta é a última utilização da variável nesta instrução.
     *
     * Corresponde ao sufixo @c '-' no ficheiro de live ranges.
     * Quando @c true, esta instrução é o ponto de fim da live range —
     * o registo que contém o valor pode ser libertado imediatamente após
     * esta instrução ser executada.
     */

    bool isEnd = false;

    /**
     * @brief Constrói um ProgramPoint com todos os campos explícitos.
     *
     * @param l  Número da linha de instrução (deve ser > 0).
     * @param s  @c true se a variável é definida nesta instrução (@c '+').
     * @param e  @c true se esta é a última utilização da variável (@c '-').
     *
     * @pre @p l deve ser maior que zero.
     *
     * @par Complexidade
     * O(1) — inicialização direta dos três campos.
     */

     ProgramPoint(int l, bool s, bool e) : line(l), isStart(s), isEnd(e) {}
};

#endif // PROGRAM_POINT_H
