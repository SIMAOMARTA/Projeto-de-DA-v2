/**
* @file AlgorithmConfig.h
 * @brief Configuração do algoritmo de alocação de registos.
 *
 * Esta estrutura é preenchida pelo Parser a partir do ficheiro de registos
 * e é passada ao RegisterAllocator para selecionar o algoritmo a executar
 * e os respetivos parâmetros.
 */

#ifndef ALGORITHM_CONFIG_H
#define ALGORITHM_CONFIG_H

#include <string>

/**
 * @struct AlgorithmConfig
 * @brief Configuração parseada a partir do ficheiro de registos.
 *
 * Armazena o número de registos físicos disponíveis e a variante do
 * algoritmo a utilizar na alocação de registos.
 *
 * Variantes suportadas:
 *  - @c "basic"     : coloração greedy sem fallback (T2.1). Tenta colorir
 *                     o grafo com N cores; se não for possível, faz spill
 *                     ilimitado.
 *  - @c "spilling"  : basic + até K spills de webs (T2.2). Itera de 0 a K
 *                     spills parando assim que a alocação for viável.
 *  - @c "splitting" : basic + até K splits de webs (T2.3). Divide webs com
 *                     grau elevado antes de tentar a coloração.
 *  - @c "free"      : algoritmo próprio baseado na heurística de Chaitin
 *                     (grau / tamanho da live range) para selecção do
 *                     candidato a spill (T2.4).
 */
struct AlgorithmConfig {
    /**
     * @brief Número de registos físicos disponíveis.
     * Deve ser > 0; o Parser valida esta condição.
     */
    int numRegisters = 0;

    /**
     * @brief Identificador da variante do algoritmo.
     * Valor é um de: @c "basic", @c "spilling", @c "splitting", @c "free".
     * O Parser valida que o valor pertence a este conjunto.
     */
    std::string algorithm = "basic";

    /**
     * @brief Parâmetro numérico para as variantes spilling e splitting.
     *
     * Para @c "spilling": número máximo de webs que podem ser enviadas
     * para memória (spilled).
     *
     * Para @c "splitting": número máximo de webs que podem ser divididas
     * (split) antes de tentar a coloração.
     *
     * Ignorado para @c "basic" e @c "free".
     * O Parser define este campo a 0 quando não existe vírgula na linha
     * do algoritmo.
     */
    int algorithmParam = 0;
};

#endif // ALGORITHM_CONFIG_H