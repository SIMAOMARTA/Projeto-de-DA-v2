/**
 * @file Parser.h
 * @brief Declaração da classe Parser.
 *
 * O Parser é responsável por ler, validar e interpretar os dois ficheiros
 * de texto que constituem a entrada do alocador de registos:
 *  1. **Ficheiro de live ranges** — uma linha por range, formato @c "var: pts".
 *  2. **Ficheiro de configuração** — número de registos e algoritmo a usar.
 *
 * @details
 * Todos os métodos são estáticos: Parser é uma classe utilitária sem
 * estado (não é instanciável). Os erros de formato lançam
 * @c std::invalid_argument; ficheiros inacessíveis lançam
 * @c std::runtime_error.
 *
 * Linhas começadas por @c '#' (incluindo após espaços iniciais) ou
 * inteiramente compostas por espaços são silenciosamente ignoradas em
 * ambos os ficheiros.
 *
 * @par Formato do ficheiro de live ranges
 * @code
 * # comentário
 * sum: 1+,2,3,4-
 * i: 5+,6,7-
 * i: 9+,10,11-
 * @endcode
 *
 * @par Formato do ficheiro de configuração
 * @code
 * # comentário
 * registers: 4
 * algorithm: spilling, 2
 * @endcode
 *
 * @see LiveRange
 * @see AlgorithmConfig
 * @see ProgramPoint
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
 * @brief Classe utilitária estática para leitura e parsing dos ficheiros de entrada.
 *
 * Fornece dois métodos públicos estáticos para os dois ficheiros de entrada:
 *  - parseRanges()    : lê o ficheiro de live ranges e devolve um vetor de LiveRange.
 *  - parseRegisters() : lê o ficheiro de configuração e devolve um AlgorithmConfig.
 *
 * Os métodos privados trim(), isCommentOrEmpty() e parsePoint() são auxiliares
 * partilhados pelas duas operações de parsing.
 */

class Parser {
public:
    /**
     * @brief Lê e faz o parse do ficheiro de live ranges.
     *
     * @details
     * Para cada linha não-comentário e não-vazia:
     *  1. Divide pelo primeiro @c ':' para separar o nome da variável da
     *     lista de pontos de programa.
     *  2. Divide a lista de pontos por @c ',' e parseia cada token com
     *     parsePoint().
     *  3. Constrói um LiveRange e adiciona-o ao vetor de resultado.
     *
     * Cada linha de dados origina exatamente um LiveRange, mesmo que a
     * mesma variável apareça em múltiplas linhas. A fusão em webs é
     * da responsabilidade de InterferenceGraph::buildWebs().
     *
     * @param filename  Caminho para o ficheiro de live ranges.
     * @return          Vetor de LiveRange (um elemento por linha de dados).
     *
     * @throws std::runtime_error    Se o ficheiro não puder ser aberto.
     * @throws std::invalid_argument Se uma linha não contiver @c ':', se o
     *                               nome da variável estiver vazio, ou se não
     *                               houver nenhum ponto de programa válido.
     *
     * @par Complexidade
     * O(L × P), onde L = número de linhas de dados, P = pontos por linha.
     */
    static std::vector<LiveRange> parseRanges(const std::string& filename);

    /**
     * @brief Lê e faz o parse do ficheiro de configuração de registos.
     *
     * @details
     * Para cada linha não-comentário e não-vazia:
     *  1. Divide pelo primeiro @c ':' para obter chave e valor.
     *  2. Para a chave @c "registers": converte o valor para inteiro.
     *  3. Para a chave @c "algorithm": verifica se há vírgula no valor:
     *     - Sem vírgula → algoritmo sem parâmetro (@c basic ou @c free).
     *     - Com vírgula → algoritmo com parâmetro K (@c spilling ou @c splitting).
     *  4. Chaves desconhecidas são ignoradas silenciosamente.
     *
     * Após ler todas as linhas, valida que @c numRegisters > 0.
     *
     * @param filename  Caminho para o ficheiro de configuração.
     * @return          Estrutura AlgorithmConfig preenchida.
     *
     * @throws std::runtime_error    Se o ficheiro não puder ser aberto.
     * @throws std::invalid_argument Se uma linha não contiver @c ':', se o
     *                               valor de @c registers estiver vazio ou
     *                               for <= 0, se o parâmetro após a vírgula
     *                               estiver vazio, ou se o algoritmo for
     *                               desconhecido.
     *
     * @par Complexidade
     * O(L), onde L = número de linhas do ficheiro.
     */

    static AlgorithmConfig parseRegisters(const std::string& filename);

private:
    /**
     * @brief Remove espaços em branco do início e do fim de uma string.
     *
     * @details
     * Procura o primeiro e o último carácter que não seja espaço, tab,
     * carriage-return ou newline e devolve a substring entre eles. Se
     * todos os caracteres forem espaços, devolve a string vazia.
     *
     * @param s  String de entrada.
     * @return   Substring sem espaços/tabs/\\r/\\n nas extremidades;
     *           string vazia se @p s for composta apenas por espaços.
     *
     * @par Complexidade
     * O(n), onde n = comprimento da string.
     */

    static std::string trim(const std::string& s);

    /**
     * @brief Verifica se uma linha deve ser ignorada (comentário ou vazia).
     *
     * @details
     * Aplica trim() e verifica se o resultado está vazio ou se o primeiro
     * carácter é @c '#'. Desta forma, linhas com espaços antes de @c '#'
     * são corretamente identificadas como comentários.
     *
     * @param line  Linha a verificar.
     * @return      @c true se a linha deve ser ignorada.
     *
     * @par Complexidade
     * O(n), onde n = comprimento da linha.
     */

    static bool isCommentOrEmpty(const std::string& line);

    /**
     * @brief Faz o parse de um token individual de ponto de programa.
     *
     * @details
     * Percorre o token carácter a carácter:
     *  - Dígitos são acumulados na string @c digits.
     *  - @c '+' ativa a flag @c isStart.
     *  - @c '-' ativa a flag @c isEnd.
     *  - Outros caracteres são ignorados.
     *
     * Se não for encontrado nenhum dígito, é lançada uma exceção.
     * Tokens do tipo @c "7+-" (início e fim na mesma instrução, como em
     * @c "i = i + 1") são corretamente tratados com ambas as flags ativas.
     *
     * @param token  Token a parsear (e.g., @c "1+", @c "6-", @c "7+-", @c "4").
     * @return       ProgramPoint construído a partir do token.
     *
     * @throws std::invalid_argument Se o token não contiver nenhum dígito.
     * @throws std::invalid_argument / @c std::out_of_range Propagados por
     *         @c std::stoi se a conversão do número falhar.
     *
     * @par Complexidade
     * O(n), onde n = comprimento do token.
     */

    static ProgramPoint parsePoint(const std::string& token);
};

#endif // PARSER_H
