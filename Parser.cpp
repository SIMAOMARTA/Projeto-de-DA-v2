/**
 * @file Parser.cpp
 * @brief Implementação da classe Parser.
 *
 * Implementa a leitura e validação dos ficheiros de entrada:
 *  - Ficheiro de live ranges (parseRanges).
 *  - Ficheiro de configuração de registos (parseRegisters).
 */

#include "Parser.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>

//funcoes auxiliares (privadas e estáticas)

/**
 * @brief Remove espaços e returns do início e fim de uma string.
 *
 * @details
 * Procura o primeiro e último carácter que não seja espaço, tab,
 * carriage-return ou newline, e devolve a substring entre eles.
 * Se todos os caracteres forem espaços, devolve a string vazia.
 *
 * @param s  String de entrada.
 * @return   Substring sem espaços/tabs/\\r/\\n nas extremidades;
 *           string vazia se @p s for composta apenas por espaços.
 *
 * @par Complexidade
 * O(n) onde n = comprimento da string.
 */
std::string Parser::trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return ""; // string só com espaços
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/**
 * @brief Verifica se uma linha é um comentário ou está vazia.
 *
 * @details
 * Aplica trim() e verifica se o resultado está vazio ou começa por '#'.
 * Desta forma, linhas com apenas espaços antes do '#' também são
 * corretamente identificadas como comentários.
 *
 * @param line  Linha a verificar.
 * @return      @c true se a linha deve ser ignorada (vazia ou comentário).
 *
 * @par Complexidade
 * O(n) onde n = comprimento da linha.
 */
bool Parser::isCommentOrEmpty(const std::string& line) {
    std::string t = trim(line);
    return t.empty() || t[0] == '#';
}


/**
 * @brief Faz o parse de um token individual de ponto de programa.
 *
 * @details
 * Percorre o token carácter a carácter:
 *  - Dígitos são acumulados na string @c digits.
 *  - '+' ativa @c isStart.
 *  - '-' ativa @c isEnd.
 *  - Outros caracteres são ignorados silenciosamente.
 *
 * Se não for encontrado nenhum dígito, é lançada uma exceção.
 * A conversão para int é feita com @c std::stoi, que lança
 * @c std::invalid_argument ou @c std::out_of_range em caso de erro.
 *
 * @param token  Token a parsear (ex: @c "1+", @c "6-", @c "7+-").
 * @return       ProgramPoint construído a partir do token.
 *
 * @throws std::invalid_argument Se o token não contiver nenhum dígito.
 *
 * @par Complexidade
 * O(n) onde n = comprimento do token.
 */
ProgramPoint Parser::parsePoint(const std::string& token) {
    std::string t = trim(token);

    bool isStart = false;
    bool isEnd   = false;

    // extrai os sufixos '+' e '-' (podem aparecer ambos, ex: "12+-")
    std::string digits;
    for (char c : t) {
        if      (c == '+') isStart = true;
        else if (c == '-') isEnd   = true;
        else if (std::isdigit(c)) digits += c;
        // ignora qualquer outro caracter inesperado
    }

    if (digits.empty())
        throw std::invalid_argument("Token sem número: \"" + token + "\"");

    int line = std::stoi(digits);
    return ProgramPoint(line, isStart, isEnd);
}


//parseRanges

/**
 * @brief Lê e faz o parse do ficheiro de live ranges.
 *
 * @details
 * Algoritmo:
 *  1. Ignora linhas de comentário e vazias via isCommentOrEmpty().
 *  2. Divide a linha pelo primeiro ':' para obter varName e a lista de pontos.
 *  3. Divide a lista de pontos por ',' e parseia cada token com parsePoint().
 *  4. Cria um LiveRange e adiciona ao vetor de resultado.
 *
 * Cada linha do ficheiro gera exatamente um LiveRange, mesmo que a mesma
 * variável apareça em múltiplas linhas. A fusão em webs é da
 * responsabilidade do InterferenceGraph.
 *
 * @param filename  Caminho para o ficheiro de live ranges.
 * @return          Vetor de LiveRange (um elemento por linha de dados).
 *
 * @throws std::runtime_error    Se o ficheiro não puder ser aberto.
 * @throws std::invalid_argument Se uma linha não contiver ':', se o nome
 *                               da variável estiver vazio, ou se não houver
 *                               nenhum ponto de programa válido na linha.
 *
 * @par Complexidade
 * O(L × P) onde L = número de linhas do ficheiro, P = pontos por linha.
 */
std::vector<LiveRange> Parser::parseRanges(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("Não foi possível abrir o ficheiro: " + filename);

    std::vector<LiveRange> result;
    std::string line;

    while (std::getline(file, line)) {

        // ignorar comentarios e linhas vazias
        if (isCommentOrEmpty(line)) continue;

        // separa "varName" de "lista de pontos" com ':'
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos)
            throw std::invalid_argument("Linha sem ':' no ficheiro de ranges: \"" + line + "\"");

        std::string varName   = trim(line.substr(0, colonPos));
        std::string pointsPart = trim(line.substr(colonPos + 1));

        if (varName.empty())
            throw std::invalid_argument("Nome de variável vazio na linha: \"" + line + "\"");

        // parte a lista de pontos em vetor
        // ex: "1+,2,3,4,5,6-"  →  ["1+", "2", "3", "4", "5", "6-"]
        std::vector<ProgramPoint> points;
        std::istringstream ss(pointsPart);
        std::string token;

        while (std::getline(ss, token, ',')) {
            token = trim(token);
            if (token.empty()) continue; // vírgula dupla, ignora
            points.push_back(parsePoint(token));
        }

        if (points.empty())
            throw std::invalid_argument("Live range sem pontos para a variável \"" + varName + "\"");

        // cria o LiveRange e adiciona ao resultado
        result.emplace_back(varName, points);
    }

    return result;
}


// parseRegisters
/**
 * @brief Lê e faz o parse do ficheiro de configuração de registos.
 *
 * @details
 * Algoritmo:
 *  1. Ignora comentários e linhas vazias via isCommentOrEmpty().
 *  2. Divide pelo primeiro ':' para obter chave e valor.
 *  3. Para a chave @c "registers": converte o valor para inteiro.
 *  4. Para a chave @c "algorithm": verifica se há vírgula no valor:
 *     - Sem vírgula: algoritmo sem parâmetro (@c basic ou @c free).
 *     - Com vírgula: algoritmo com parâmetro K (@c spilling ou @c splitting).
 *  5. Valida que o algoritmo é um dos quatro suportados.
 *  6. Após ler todas as linhas, valida que numRegisters > 0.
 *
 * Chaves desconhecidas são ignoradas silenciosamente para tolerar
 * ficheiros com campos extra (extensibilidade futura).
 *
 * @param filename  Caminho para o ficheiro de configuração.
 * @return          Estrutura AlgorithmConfig preenchida.
 *
 * @throws std::runtime_error    Se o ficheiro não puder ser aberto.
 * @throws std::invalid_argument Se uma linha não contiver ':', se o valor
 *                               de @c registers estiver vazio ou for <= 0,
 *                               se o parâmetro após a vírgula estiver vazio,
 *                               ou se o algoritmo for desconhecido.
 *
 * @par Complexidade
 * O(L) onde L = número de linhas do ficheiro.
 */
AlgorithmConfig Parser::parseRegisters(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("Não foi possível abrir o ficheiro: " + filename);

    AlgorithmConfig config;
    std::string line;

    while (std::getline(file, line)) {

        // ignorar comentarios e linhas vazias
        if (isCommentOrEmpty(line)) continue;

        // separa chave de valor com ':'
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos)
            throw std::invalid_argument("Linha sem ':' no ficheiro de registos: \"" + line + "\"");

        std::string key   = trim(line.substr(0, colonPos));
        std::string value = trim(line.substr(colonPos + 1));

        // processa cada chave conhecida

        if (key == "registers") {
            // "registers: 4" -> config.numRegisters = 4
            if (value.empty())
                throw std::invalid_argument("Valor em falta para 'registers'");
            config.numRegisters = std::stoi(value);   // stoi lança se não for número

        } else if (key == "algorithm") {
            // pode ser: "basic" -> param=0; "free" -> param=0; "spilling" -> param=2; "splitting" -> param=3

            size_t commaPos = value.find(',');

            if (commaPos == std::string::npos) {
                // sem vírgula -> basic ou free
                config.algorithm     = trim(value);
                config.algorithmParam = 0;
            } else {
                // com vírgula -> spilling ou splitting com parâmetro
                config.algorithm      = trim(value.substr(0, commaPos));
                std::string paramStr  = trim(value.substr(commaPos + 1));
                if (paramStr.empty())
                    throw std::invalid_argument("Parâmetro em falta após vírgula em 'algorithm'");
                config.algorithmParam = std::stoi(paramStr);
            }

            // Valida que o algoritmo é um dos suportados
            const std::string& alg = config.algorithm;
            if (alg != "basic" && alg != "spilling" &&
                alg != "splitting" && alg != "free")
                throw std::invalid_argument("Algoritmo desconhecido: \"" + alg + "\"");

        } else {
            // Chave desconhecida — avisa mas não aborta (tolerância)
        }
    }

    //validação final
    if (config.numRegisters <= 0)
        throw std::invalid_argument("Número de registos deve ser > 0 (lido: "
                                    + std::to_string(config.numRegisters) + ")");

    return config;
}