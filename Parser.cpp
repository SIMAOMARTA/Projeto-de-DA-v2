/**
 * @file Parser.cpp
 * @brief Implementação da classe Parser.
 *
 * @details
 * Implementa a leitura e validação dos dois ficheiros de entrada do
 * alocador de registos:
 *  - parseRanges()    : lê e parseia o ficheiro de live ranges.
 *  - parseRegisters() : lê e parseia o ficheiro de configuração de registos.
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

/**
 * @brief Remove espaços em branco do início e do fim de uma string.
 *
 * @details
 * Utiliza @c std::string::find_first_not_of e @c find_last_not_of para
 * localizar o primeiro e o último carácter que não sejam espaço, tab,
 * carriage-return ou newline e devolve a substring entre eles. Se todos
 * os caracteres forem espaços, devolve a string vazia.
 *
 * Esta função é chamada em múltiplos pontos de parseRanges() e
 * parseRegisters() para sanitizar nomes de variáveis, chaves, valores
 * e tokens de pontos de programa antes de os processar.
 *
 * @param s  String de entrada.
 * @return   Substring sem espaços/tabs/\\r/\\n nas extremidades;
 *           string vazia se @p s for composta apenas por espaços.
 *
 * @par Complexidade
 * O(n), onde n = comprimento da string @p s.
 */

std::string Parser::trim(const std::string& s){
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");

    return s.substr(start, end - start + 1);
}

/**
 * @brief Verifica se uma linha deve ser ignorada (comentário ou vazia).
 *
 * @details
 * Aplica trim() e verifica se o resultado está vazio ou começa por @c '#'.
 * Desta forma, linhas com espaços antes do símbolo de comentário são
 * corretamente identificadas como comentários, evitando erros de parsing
 * por indentação acidental.
 *
 * @param line  Linha a verificar (pode conter @c \\r no final em sistemas Windows).
 * @return      @c true se a linha deve ser ignorada (vazia ou comentário).
 *
 * @par Complexidade
 * O(n), onde n = comprimento da linha (dominado por trim()).
 */

bool Parser::isCommentOrEmpty(const std::string& line){
    std::string t = trim(line);

    return t.empty() || t[0] == '#';
}

/**
 * @brief Faz o parse de um token individual de ponto de programa.
 *
 * @details
 * Um token de ponto de programa tem o formato geral @c "<dígitos>[+][-]",
 * onde os sufixos @c '+' e @c '-' são opcionais e podem aparecer ambos
 * (ex.: @c "7+-" numa instrução @c "i = i + 1").
 *
 * O algoritmo percorre o token carácter a carácter:
 *  - Dígitos são acumulados na string @c digits.
 *  - @c '+' ativa @c isStart.
 *  - @c '-' ativa @c isEnd.
 *  - Outros caracteres (espaços residuais após trim()) são ignorados.
 *
 * A conversão para inteiro usa @c std::stoi, que propaga
 * @c std::invalid_argument ou @c std::out_of_range em caso de erro de
 * formato ou de overflow.
 *
 * @param token  Token a parsear, após trim() (e.g., @c "1+", @c "6-",
 *               @c "7+-", @c "12").
 * @return       ProgramPoint construído a partir do token.
 *
 * @throws std::invalid_argument Se o token não contiver nenhum dígito.
 * @throws std::invalid_argument / @c std::out_of_range  Propagados por
 *         @c std::stoi se a parte numérica não for um inteiro válido.
 *
 * @par Complexidade
 * O(n), onde n = comprimento do token.
 */

ProgramPoint Parser::parsePoint(const std::string& token){
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

    int line = std::stoi(digits);

    return ProgramPoint(line, isStart, isEnd);
}

/**
 * @brief Lê e faz o parse do ficheiro de live ranges.
 *
 * @details
 * Algoritmo linha a linha:
 *  1. Ignora comentários e linhas vazias via isCommentOrEmpty().
 *  2. Localiza o primeiro @c ':' e divide a linha em @p varName (esquerda)
 *     e @p pointsPart (direita).
 *  3. Valida que @p varName não está vazio após trim().
 *  4. Divide @p pointsPart por @c ',' usando um @c std::istringstream e
 *     parseia cada token não vazio com parsePoint().
 *  5. Valida que pelo menos um ponto foi encontrado.
 *  6. Constrói um LiveRange e adiciona ao vetor de resultado.
 *
 * Cada linha de dados origina exatamente um LiveRange, mesmo que a mesma
 * variável apareça em múltiplas linhas do ficheiro. A fusão de ranges
 * sobrepostas em webs é feita posteriormente pelo InterferenceGraph.
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

std::vector<LiveRange> Parser::parseRanges(const std::string& filename){
    std::ifstream file(filename);
    if (!file.is_open()) throw std::runtime_error("Não foi possível abrir o ficheiro: " + filename);

    std::vector<LiveRange> result;
    std::string line;

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

/**
 * @brief Lê e faz o parse do ficheiro de configuração de registos.
 *
 * @details
 * Algoritmo linha a linha:
 *  1. Ignora comentários e linhas vazias via isCommentOrEmpty().
 *  2. Localiza o primeiro @c ':' e divide a linha em @p key (esquerda)
 *     e @p value (direita).
 *  3. Para a chave @c "registers":
 *     - Valida que o valor não está vazio.
 *     - Converte para inteiro com @c std::stoi.
 *  4. Para a chave @c "algorithm":
 *     - Verifica se há vírgula no valor.
 *     - Sem vírgula → nome do algoritmo apenas (@c basic ou @c free).
 *     - Com vírgula → nome + parâmetro K (@c spilling ou @c splitting).
 *     - Valida que o nome pertence ao conjunto de algoritmos suportados.
 *  5. Chaves desconhecidas são ignoradas silenciosamente para extensibilidade futura.
 *  6. Após o loop, valida que @c numRegisters > 0.
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

AlgorithmConfig Parser::parseRegisters(const std::string& filename){
    std::ifstream file(filename);
    if (!file.is_open()) throw std::runtime_error("Não foi possível abrir o ficheiro: " + filename);

    AlgorithmConfig config;
    std::string line;

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
