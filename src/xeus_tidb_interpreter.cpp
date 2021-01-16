/***************************************************************************
 * Copyright (c) 2020, QuantStack and xeus-tidb contributors                *
 *                                                                          *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#include "xeus-tidb/xeus_tidb_interpreter.hpp"

#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <tuple>
#include <vector>

#include "soci/mysql/soci-mysql.h"
#include "xeus-tidb/soci_handler.hpp"
#include "xeus/xinterpreter.hpp"

namespace xeus_tidb {
void interpreter::configure_impl() {}

using clock = std::chrono::system_clock;
using sec = std::chrono::duration<double>;

void interpreter::publish_ok(int execution_counter) {
    nl::json pub_data;
    pub_data["text/plain"] = "Ok.";
    publish_execution_result(execution_counter, std::move(pub_data), nl::json::object());
}

nl::json interpreter::process_SQL_input(const std::string& code, std::vector<std::string>& row_headers,
                                        xv::df_type& xv_sql_df, bool getdf) {
    const auto before = clock::now();
    soci::rowset<soci::row> rows = ((*this->sql).prepare << code);

    nl::json pub_data;
    std::stringstream html_table("");
    int count = 0;
    for (const soci::row& r : rows) {
        if (!getdf && count == 0) {
            html_table << "<table>\n<tr>\n";
            for (std::size_t i = 0; i != r.size(); ++i) {
                std::string name = r.get_properties(i).get_name();
                html_table << "<th>" << name << "</th>\n";
                row_headers.push_back(name);
            }
            html_table << "</tr>\n";
        }
        count++;

        // Iterates through cols' rows and builds different kinds of outputs
        html_table << "<tr>\n";
        for (std::size_t i = 0; i != r.size(); ++i) {
            std::string cell;
            soci::column_properties props = r.get_properties(i);
            try {
                switch (props.get_data_type()) {
                    case soci::dt_string:
                        cell = r.get<std::string>(i, "NULL");
                        break;
                    case soci::dt_integer:
                        cell = std::to_string(r.get<int>(i));
                        break;
                    case soci::dt_long_long:
                        cell = std::to_string(r.get<long long>(i));
                        break;
                    case soci::dt_unsigned_long_long:
                        cell = std::to_string(r.get<unsigned long long>(i));
                        break;
                    case soci::dt_double:
                        cell = std::to_string(r.get<double>(i));
                        cell.erase(cell.find_last_not_of('0') + 1, std::string::npos);
                        if (cell.back() == '.') {
                            cell.pop_back();
                        }
                        break;
                    case soci::dt_blob:
                    case soci::dt_xml:
                        break;
                    case soci::dt_date:
                        std::tm when = r.get<std::tm>(i);
                        cell = std::asctime(&when);
                        break;
                }
            } catch (...) {
                cell = "NULL";
            }
            if (getdf) {
                xv_sql_df[props.get_name()].push_back(cell);
            } else {
                html_table << "<td>" << cell << "</td>\n";
            }
        }

        html_table << "</tr>\n";
    }
    const sec duration = clock::now() - before;

    if (!getdf) {
        html_table << "</table>";
        if (count == 0) {
            html_table << std::fixed << std::setprecision(2) << "Empty set (" << duration.count() << " sec)";
        } else if (count == 1) {
            html_table << std::fixed << std::setprecision(2) << "1 row in set (" << duration.count() << " sec)";
        } else {
            html_table << std::fixed << std::setprecision(2) << count << " rows in set (" << duration.count()
                       << " sec)";
        }

        pub_data["text/html"] = html_table.str();
    }

    return pub_data;
}

nl::json interpreter::execute_request_impl(int execution_counter, const std::string& code, bool /*silent*/,
                                           bool /*store_history*/, nl::json /*user_expressions*/,
                                           bool /*allow_stdin*/) {
    std::vector<std::string> tokenized_code = tokenizer(code);
    xv::df_type xv_sql_df;
    std::vector<std::string> row_headers;
    try {
        /* Runs magic */
        // for (size_t i = 0; i < tokenized_code.size(); i++) {
        //     std::cerr<< i<<":"<<tokenized_code[i]<<std::endl;
        // }
        if (is_magic(tokenized_code)) {
            if (tokenized_code[1] == "VEGA-LITE") {
                if (tokenized_code.size() < 3) {
                    throw std::runtime_error("invalid input: " + code);
                }
                std::ifstream i(tokenized_code[2]);
                nl::json j;
                i >> j;
                if (tokenized_code.size() > 3) {
                    // we have SQL
                    std::string sql = code;
                    sql.erase(0, code.find("\n") + 1);
                    process_SQL_input(sql, row_headers, xv_sql_df, true);
                    if (xv_sql_df.size() == 0) {
                        throw std::runtime_error("empty result from sql, can't render");
                    }
                    xv::data_frame data_frame;
                    data_frame.values = xv_sql_df;
                    j["data"] = data_frame;
                }
                auto bundle = nl::json::object();
                bundle["application/vnd.vegalite.v3+json"] = j;
                publish_execution_result(execution_counter, std::move(bundle), nl::json::object());
            } else {
                /* Parses LOAD magic */
                tokenized_code.erase(tokenized_code.begin());
                this->sql = parse_SQL_magic(tokenized_code);
                publish_ok(execution_counter);
            }
        } else {
            if (this->sql) {
                /* Shows rich output for tables */
                if (case_insentive_equals("SELECT", tokenized_code[1]) ||
                    case_insentive_equals("DESC", tokenized_code[1]) ||
                    case_insentive_equals("DESCRIBE", tokenized_code[1]) ||
                    case_insentive_equals("SHOW", tokenized_code[1])) {
                    // log(logINFO) << "running sql\n";
                    nl::json data = process_SQL_input(code, row_headers, xv_sql_df);

                    publish_execution_result(execution_counter, std::move(data), nl::json::object());

                }
                /* Execute all SQL commands that don't output tables */
                else {
                    *this->sql << code;
                    publish_ok(execution_counter);
                }
            } else {
                throw std::runtime_error("Database was not loaded.");
            }
        }
    } catch (const std::runtime_error& err) {
        return handle_exception((std::string)err.what());
    } catch (std::exception const& err) {
        return handle_exception((std::string)err.what());
    } catch (soci::mysql_soci_error const& err) {
        return handle_exception((std::string)err.what());
    } catch (...) {
        // https:  // stackoverflow.com/a/54242936/1203241
        try {
            std::exception_ptr curr_excp;
            if ((curr_excp = std::current_exception())) {
                std::rethrow_exception(curr_excp);
            }
        } catch (const std::exception& err) {
            return handle_exception((std::string)err.what());
        }
    }
    nl::json jresult;
    jresult["status"] = "ok";
    jresult["payload"] = nl::json::array();
    jresult["user_expressions"] = nl::json::object();
    return jresult;
}

nl::json interpreter::handle_exception(std::string what) {
    std::vector<std::string> traceback;
    nl::json jresult;
    jresult["status"] = "error";
    jresult["ename"] = "Error";
    jresult["evalue"] = what;
    traceback.push_back((std::string)jresult["ename"] + ": " + what);
    publish_execution_error(jresult["ename"], jresult["evalue"], traceback);
    return jresult;
}

nl::json interpreter::complete_request_impl(const std::string& /*code*/, int /*cursor_pos*/) {
    nl::json jresult;
    jresult["status"] = "ok";
    return jresult;
};

nl::json interpreter::inspect_request_impl(const std::string& /*code*/, int /*cursor_pos*/, int /*detail_level*/) {
    nl::json jresult;
    jresult["status"] = "ok";
    return jresult;
};

nl::json interpreter::is_complete_request_impl(const std::string& /*code*/) {
    nl::json jresult;
    jresult["status"] = "complete";
    return jresult;
};

// clang-format off
    nl::json interpreter::kernel_info_request_impl()
    {
        nl::json result;
        result["implementation"] = "xtidb";
        result["implementation_version"] = XTIDB_VERSION;

        std::string banner = R"V0G0N(
            "                          _   _     _ _             "
            " _  _____ _   _ ___      | |_(_) __| | |__          "
            "\ \/ / _ \ | | / __|_____| __| |/ _` | '_ \         "
            " >  <  __/ |_| \__ \_____| |_| | (_| | |_) |        "
            "/_/\_\___|\__,_|___/      \__|_|\__,_|_.__/         "
            "                                                    "
            "  xeus-tidb: a Jupyter kernel for SOCI\n"
            "  SOCI version: ")V0G0N";
        banner.append(XTIDB_VERSION);

        result["banner"] = banner;
        //TODO: This should change with the language
        result["language_info"]["name"] = "mysql";
        result["language_info"]["codemirror_mode"] = "sql";
        result["language_info"]["version"] = XTIDB_VERSION;
        result["language_info"]["mimetype"] = "";
        result["language_info"]["file_extension"] = "";
        return result;
    }
// clang-format on

void interpreter::shutdown_request_impl() {}

}  // namespace xeus_tidb
