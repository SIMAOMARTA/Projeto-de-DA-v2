/**
 * @file Parser.h
 * @brief Declaração da classe Parser.
 *
 * O Parser é responsável por ler e validar os dois ficheiros de entrada
 * do programa:
 *  1. Ficheiro de live ranges — uma linha por range, formato @c "var: pts".
 *  2. Ficheiro de configuração — número de registos e algoritmo.
 */

#ifndef PARSER_H
#define PARSER_H

#include "LiveRange.h"
#include "AlgorithmConfig.h"
#include <string>
#include <vector>
#include <stdexcept>

/**
 * @class Parser
 * @brief Classe utilitária para leitura e parsing dos ficheiros de entrada.
 *
 * Fornece dois métodos públicos estáticos:
 *  - parseRanges()    : lê o ficheiro de live ranges.
 *  - parseRegisters() : lê o ficheiro de configuração.
 *
 * Linhas começadas por '#' ou vazias são ignoradas em ambos os ficheiros.
 * Erros de formato lançam std::invalid_argument; ficheiros inacessíveis
 * lançam std::runtime_error.
 */
class Parser {
public:
    /**
     * @brief Lê e faz o parse do ficheiro de live ranges.
     * @param filename  Caminho para o ficheiro de live ranges.
     * @return          Vetor de LiveRange (um elemento por linha de dados).
     */
    static std::vector<LiveRange> parseRanges(const std::string& filename);

    /**
     * @brief Lê e faz o parse do ficheiro de configuração de registos.
     * @param filename  Caminho para o ficheiro de configuração.
     * @return          Estrutura AlgorithmConfig preenchida.
     */
    static AlgorithmConfig parseRegisters(const std::string& filename);

private:
    /**
     * @brief Faz o parse de um token individual de ponto de programa.
     * Extrai o número inteiro e os sufixos '+' (isStart) e '-' (isEnd)
     * de uma string como @c "12+" ou @c "6-" ou @c "7+-".
     * @param token  Token a parsear (espaços são ignorados).
     * @return       ProgramPoint construído a partir do token.
     */
    static ProgramPoint parsePoint(const std::string& token);

    /**
     * @brief Remove espaços e carriage returns do início e fim de uma string.
     * @param s  String de entrada.
     * @return   Substring sem espaços/tabs/\\r/\\n nas extremidades.
     *           Devolve @c "" se a string for composta apenas por espaços.
     */
    static std::string trim(const std::string& s);

    /**
     * @brief Verifica se uma linha é um comentário ou está vazia.
     * Uma linha é considerada comentário se, após trim(), o primeiro
     * carácter for '#'. Linhas vazias (após trim) também devolvem @c true.
     * @param line  Linha a verificar.
     * @return      @c true se deve ser ignorada.
     */
    static bool isCommentOrEmpty(const std::string& line);
};

#endif // PARSER_H