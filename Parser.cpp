/**
 * @file Parser.cpp
 * @brief Implementação da classe Parser.
 *
 * @details
 * Implementa a leitura e validação dos dois ficheiros de entrada do
 * alocador de registos:
 * - parseRanges()    : lê e parseia o ficheiro de live ranges.
 * - parseRegisters() : lê e parseia o ficheiro de configuração de registos.
 *
 * Os auxiliares privados trim(), isCommentOrEmpty() e parsePoint() são
 * partilhados pelas duas operações de parsing e documentados aqui com
 * detalhe suficiente para servir de referência à análise de complexidade.
 *
 * @see Parser.h para a documentação completa da interface pública.
 */

#include "Parser.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>

std::string Parser::trim(const std::string& s){
    // Utiliza std::string::find_first_not_of e find_last_not_of para
    // localizar o primeiro e o último carácter que não sejam espaço, tab,
    // carriage-return ou newline.
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");

    return s.substr(start, end - start + 1);
}

bool Parser::isCommentOrEmpty(const std::string& line){
    // Aplica trim() e verifica se o resultado está vazio ou começa por '#'.
    // Evita erros de parsing por indentação acidental antes do comentário.
    std::string t = trim(line);

    return t.empty() || t[0] == '#';
}

ProgramPoint Parser::parsePoint(const std::string& token){
    // Um token de ponto de programa tem o formato geral "<dígitos>[+][-]",
    // onde os sufixos '+' e '-' são opcionais e podem aparecer ambos (ex: "7+-").

    std::string t = trim(token);

    bool isStart = false;
    bool isEnd   = false;
    std::string digits;

    for (char c : t) {
        if (c == '+') isStart = true;
        else if (c == '-') isEnd   = true;
        else if (std::isdigit(c)) digits += c;
        // outros caracteres são ignorados
    }

    if (digits.empty()) throw std::invalid_argument("Token sem número: \"" + token + "\"");

    // std::stoi propaga std::invalid_argument ou std::out_of_range em caso de erro
    int line = std::stoi(digits);

    return ProgramPoint(line, isStart, isEnd);
}

std::vector<LiveRange> Parser::parseRanges(const std::string& filename){
    std::ifstream file(filename);
    if (!file.is_open()) throw std::runtime_error("Não foi possível abrir o ficheiro: " + filename);

    std::vector<LiveRange> result;
    std::string line;

    // Algoritmo linha a linha:
    // 1. Ignora comentários e linhas vazias via isCommentOrEmpty().
    // 2. Localiza o primeiro ':' e divide a linha em varName e pointsPart.
    // 3. Valida que varName não está vazio após trim().
    // 4. Divide pointsPart por ',' usando um std::istringstream e parseia cada token.
    // 5. Valida que pelo menos um ponto foi encontrado e constrói o LiveRange.

    while (std::getline(file, line)){
        if (isCommentOrEmpty(line)) continue;

        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos)
            throw std::invalid_argument("Linha sem ':' no ficheiro de ranges: \"" + line + "\"");

        std::string varName = trim(line.substr(0, colonPos));
        std::string pointsPart = trim(line.substr(colonPos + 1));

        if (varName.empty()) throw std::invalid_argument("Nome de variável vazio na linha: \"" + line + "\"");

        std::vector<ProgramPoint> points;
        std::istringstream ss(pointsPart);
        std::string token;

        while (std::getline(ss, token, ',')){
            token = trim(token);
            if (token.empty()) continue;
            points.push_back(parsePoint(token));
        }

        if (points.empty()) throw std::invalid_argument("Live range sem pontos para a variável \"" + varName + "\"");

        result.emplace_back(varName, points);
    }

    return result;
}

AlgorithmConfig Parser::parseRegisters(const std::string& filename){
    std::ifstream file(filename);
    if (!file.is_open()) throw std::runtime_error("Não foi possível abrir o ficheiro: " + filename);

    AlgorithmConfig config;
    std::string line;

    // Algoritmo linha a linha:
    // 1. Ignora comentários e linhas vazias via isCommentOrEmpty().
    // 2. Localiza o primeiro ':' e divide a linha em key e value.
    // 3. Para a chave "registers": converte para inteiro.
    // 4. Para a chave "algorithm": verifica se há vírgula no valor para separar nome e parâmetro.
    // 5. Chaves desconhecidas são ignoradas silenciosamente.

    while (std::getline(file, line)){
        if (isCommentOrEmpty(line)) continue;

        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos)
            throw std::invalid_argument("Linha sem ':' no ficheiro de registos: \"" + line + "\"");

        std::string key = trim(line.substr(0, colonPos));
        std::string value = trim(line.substr(colonPos + 1));

        if (key == "registers"){
            if (value.empty()) throw std::invalid_argument("Valor em falta para 'registers'");
            config.numRegisters = std::stoi(value);
        }

        else if (key == "algorithm"){
            size_t commaPos = value.find(',');

            if (commaPos == std::string::npos) {
                config.algorithm = trim(value);
                config.algorithmParam = 0;
            }
            else {
                config.algorithm = trim(value.substr(0, commaPos));
                std::string paramStr = trim(value.substr(commaPos + 1));
                if (paramStr.empty()) throw std::invalid_argument("Parâmetro em falta após vírgula em 'algorithm'");
                config.algorithmParam = std::stoi(paramStr);
            }
            const std::string& alg = config.algorithm;
            if (alg != "basic" && alg != "spilling" && alg != "splitting" && alg != "free")
                throw std::invalid_argument("Algoritmo desconhecido: \"" + alg + "\"");
        }
    }

    if (config.numRegisters <= 0) {
        throw std::invalid_argument("Número de registos deve ser > 0 (lido: "
            + std::to_string(config.numRegisters) + ")");
    }

    return config;
}
