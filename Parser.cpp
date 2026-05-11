#include "Parser.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>

//funcoes auxiliares

/**
 * @brief remove espaços e '\r' do início e fim de uma string
 * complexidade: O(n) onde n = comprimento da string
 */

std::string Parser::trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";           // string só com espaços
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}


/**
 * @brief devolve true se a linha for um comentário (#) ou vazia
 * complexidade: O(n)
 */

bool Parser::isCommentOrEmpty(const std::string& line) {
    std::string t = trim(line);
    return t.empty() || t[0] == '#';
}


/**
 * @brief faz o parse de um token individual de ponto de programa
 * complexidade: O(n) onde n = comprimento do token
 */

ProgramPoint Parser::parsePoint(const std::string& token) {
    std::string t = trim(token);

    bool isStart = false;
    bool isEnd   = false;

    // Extrai os sufixos '+' e '-' (podem aparecer ambos, ex: "12+-")
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


//parseRanges  –  lê o ficheiro de live ranges

/**
 * @brief Lê e faz parse do ficheiro de live ranges
 * Formato esperado de cada linha de dados:
 * @code
 *   <varName>: <ponto>[,<ponto>]*
 * @endcode
 *
 * @param filename  Caminho para o ficheiro de ranges.
 * @return          Vetor com todos os LiveRange lidos (um por linha de dados).
 * @throws std::runtime_error      se o ficheiro não abrir.
 * @throws std::invalid_argument   se uma linha tiver formato inválido.
 * Complexidade: O(L * P), onde L = número de linhas, P = pontos por linha.
 */

//exemplo de input: @code / i: 1+,2,3,4,5,6- / @endcode

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
            if (token.empty()) continue;            // vírgula dupla → ignora
            points.push_back(parsePoint(token));
        }

        if (points.empty())
            throw std::invalid_argument("Live range sem pontos para a variável \"" + varName + "\"");

        // cria o LiveRange e adiciona ao resultado
        result.emplace_back(varName, points);
    }

    return result;
}


//  parseRegisters  –  lê o ficheiro de config

/**
 * @brief Lê e faz parse do ficheiro de configuração de registos.
 * Formato esperado:
 * @code
 *   registers: N
 *   algorithm: basic
 *   algorithm: spilling, 2
 *   algorithm: splitting, 3
 *   algorithm: free
 * @endcode
 *
 * A ordem das linhas não importa. Linhas com '#' ou vazias são ignoradas.
 *
 * @param filename  Caminho para o ficheiro de configuração.
 * @return          AlgorithmConfig preenchido.
 * @throws std::runtime_error    se o ficheiro não abrir.
 * @throws std::invalid_argument se o formato for inválido.
 *
 * Complexidade: O(L) onde L = número de linhas do ficheiro.
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