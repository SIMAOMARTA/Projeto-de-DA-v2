/**
 * @file AlgorithmConfig.h
 * @brief Configuração do algoritmo de alocação de registos.
 *
 * Esta estrutura é preenchida pelo Parser a partir do ficheiro de
 * configuração de registos e é passada ao RegisterAllocator para
 * selecionar o algoritmo a executar e os respetivos parâmetros.
 *
 * @details
 * O fluxo de utilização típico é:
 * @code
 *   AlgorithmConfig cfg = Parser::parseRegisters("registers.txt");
 *   auto [allocs, graph, meta] = RegisterAllocator::allocate(ig, cfg);
 * @endcode
 *
 * @see Parser::parseRegisters()
 * @see RegisterAllocator::allocate()
 */

#ifndef ALGORITHM_CONFIG_H
#define ALGORITHM_CONFIG_H

#include <string>

/**
 * @struct AlgorithmConfig
 * @brief Configuração parseada a partir do ficheiro de registos.
 *
 * Armazena o número de registos físicos disponíveis e a variante do
 * algoritmo de coloração de grafos a utilizar na alocação de registos.
 *
 * @par Variantes suportadas
 *
 * | Valor de @c algorithm | Parâmetro | Descrição |
 * |-----------------------|-----------|-----------|
 * | @c "basic"     | ignorado (0) | Coloração greedy de Kempe sem limite de spills (T2.1). Tenta colorir o grafo com N cores; as webs não coloridas são marcadas como spilled. |
 * | @c "spilling"  | K ≥ 0 | Coloração basic + até K webs em spill (T2.2). Itera de 0 a K spills, parando assim que a alocação for viável sem exceder esse limite. |
 * | @c "splitting" | K ≥ 0 | Divisão iterativa de webs antes da coloração (T2.3). Em cada iteração divide a web com maior grau; repete até K vezes ou até a coloração ser viável. |
 * | @c "free"      | ignorado (0) | Heurística de Chaitin: seleciona candidato a spill pela razão grau/tamanho da live range (T2.4). |
 *
 * @par Formato do ficheiro de configuração
 * @code
 * # comment line
 * registers: 4
 * algorithm: spilling, 2
 * @endcode
 */

struct AlgorithmConfig {
    /**
     * @brief Número de registos físicos disponíveis para a alocação.
     *
     * Corresponde ao número de cores usadas na coloração do grafo de
     * interferência. Deve ser estritamente maior que zero; o Parser
     * valida esta condição e lança @c std::invalid_argument caso
     * contrário.
     *
     * Os registos são nomeados @c r0, @c r1, …, @c r(N-1) na saída.
     */

    int numRegisters = 0;

    /**
     * @brief Identificador textual da variante do algoritmo a executar.
     *
     * O valor deve pertencer ao conjunto:
     * @c "basic", @c "spilling", @c "splitting", @c "free".
     * O Parser valida que o valor é um destes quatro e lança
     * @c std::invalid_argument se não reconhecer o algoritmo.
     *
     * @see RegisterAllocator::allocate() para a semântica de cada variante.
     */

    std::string algorithm = "basic";

    /**
     * @brief Parâmetro numérico inteiro para as variantes @c spilling e @c splitting.
     *
     * - Para @c "spilling": número máximo de webs que podem ser enviadas
     *   para memória (K spills). O RegisterAllocator itera de 0 a K spills,
     *   retornando na primeira iteração viável.
     *
     * - Para @c "splitting": número máximo de webs que podem ser divididas
     *   antes de tentar a coloração. O RegisterAllocator tenta dividir a
     *   web com maior grau em cada iteração, até K vezes.
     *
     * Para @c "basic" e @c "free" este campo é ignorado pelo alocador.
     * O Parser define este campo a 0 quando não existe vírgula na linha
     * do algoritmo no ficheiro de entrada.
     */

    int algorithmParam = 0;
};

#endif // ALGORITHM_CONFIG_H
