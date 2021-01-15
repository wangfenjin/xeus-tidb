/***************************************************************************
 * Copyright (c) 2020, QuantStack and xeus-tidb contributors                *
 *                                                                          *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#ifndef XEUS_TIDB_HANDLER_HPP
#define XEUS_TIDB_HANDLER_HPP

#include <string>
#include <vector>

#include "nlohmann/json.hpp"
#include "soci/soci.h"
#include "xeus/xinterpreter.hpp"
#include "xeus_tidb_interpreter.hpp"

namespace nl = nlohmann;

namespace xeus_tidb {
static std::string sanitize_string(const std::string& code) {
    /*
      Cleans the code from inputs that are acceptable in a jupyter notebook.
    */
    std::string aux = code;
    aux.erase(std::remove_if(aux.begin(), aux.end(),
                             [](char const c) { return '\n' == c || '\r' == c || '\0' == c || '\x1A' == c; }),
              aux.end());
    return aux;
}

static std::vector<std::string> tokenizer(const std::string& code) {
    /*
      Separetes the code on spaces so it's easier to execute the commands.
    */
    // std::stringstream input(sanitize_string(code));
    std::stringstream input(code);
    std::string segment;
    std::vector<std::string> tokenized_str;
    std::string is_magic(1, input.str()[0]);
    tokenized_str.push_back(is_magic);

    while (std::getline(input, segment, ' ')) {
        tokenized_str.push_back(segment);
    }

    return tokenized_str;
}

static bool is_magic(std::vector<std::string>& tokenized_code) {
    /*
      Returns true if the code input is magic and false if isn't.
    */
    if (tokenized_code[0] == "%") {
        tokenized_code[1].erase(0, 1);
        std::transform(tokenized_code[1].begin(), tokenized_code[1].end(), tokenized_code[1].begin(), ::toupper);
        return true;
    } else {
        return false;
    }
}

static bool case_insentive_equals(const std::string& a, const std::string& b) {
    return std::equal(a.begin(), a.end(), b.begin(),
                      [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
}

static std::string to_lower(const std::string& input) {
    std::string lower_case_input;
    lower_case_input.resize(input.length());
    std::transform(input.begin(), input.end(), lower_case_input.begin(), ::tolower);
    return lower_case_input;
}

static std::string to_upper(const std::string& input) {
    std::string upper_case_input;
    upper_case_input.resize(input.length());
    std::transform(input.begin(), input.end(), upper_case_input.begin(), ::toupper);
    return upper_case_input;
}

static std::unique_ptr<soci::session> load_db(const std::vector<std::string> tokenized_input) {
    std::string aux;
    for (std::size_t i = 2; i < tokenized_input.size(); i++) {
        aux += tokenized_input[i] + ' ';
    }
    return std::make_unique<soci::session>(to_lower(tokenized_input[1]), aux);
}

static std::unique_ptr<soci::session> parse_SQL_magic(const std::vector<std::string>& tokenized_input) {
    if (case_insentive_equals(tokenized_input[0], "LOAD")) {
        return load_db(tokenized_input);
    }
    throw std::runtime_error("Command is not valid: " + tokenized_input[0]);
}

static std::pair<std::vector<std::string>, std::vector<std::string>> split_xv_sql_input(
    std::vector<std::string> complete_input) {
    // TODO: test edge cases
    auto found = std::find(complete_input.begin(), complete_input.end(), "<>");

    std::vector<std::string> xvega_input(complete_input.begin(), found);
    std::vector<std::string> sql_input(found + 1, complete_input.end());

    return std::make_pair(xvega_input, sql_input);
}
}  // namespace xeus_tidb

#endif
