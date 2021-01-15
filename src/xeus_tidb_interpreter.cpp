/***************************************************************************
* Copyright (c) 2020, QuantStack and xeus-tidb contributors                *
*                                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <cctype>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <vector>
#include <tuple>

#include "xeus/xinterpreter.hpp"

#include "xeus-tidb/xeus_tidb_interpreter.hpp"
#include "xeus-tidb/soci_handler.hpp"

namespace xeus_tidb
{
    void interpreter::configure_impl()
    {
    }

    nl::json interpreter::process_SQL_input(const std::string& code,
                                            std::vector<std::string>& row_headers,
                                            xv::df_type& xv_sql_df)
    {
        nl::json pub_data;
        std::stringstream html_table("");

        soci::rowset<soci::row> rows = ((*this->sql).prepare << code);

        /* Builds table header */
        const soci::row& first_row = *rows.begin();

        html_table << "<table>\n<tr>\n";
        for(std::size_t i = 0; i < first_row.size(); ++i)
        {
            std::string name = first_row.get_properties(i).get_name();
            html_table << "<th>" << name << "</th>\n";
            xv_sql_df[name] = { "name" };
            row_headers.push_back(name);
        }
        html_table << "</tr>\n";

        /* Builds table body */
        for (const soci::row& r : rows)
        {
            /* Iterates through cols' rows and builds different kinds of
               outputs
            */
            html_table << "<tr>\n";
            for(std::size_t i = 0; i != r.size(); ++i)
            {
                std::string cell;

                soci::column_properties props = r.get_properties(i);
                switch(props.get_data_type())
                {
                    case soci::dt_string:
                        cell = r.get<std::string>(i);
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
                        cell.erase ( cell.find_last_not_of('0') + 1, std::string::npos );
                        if (cell.back() == '.') {
                            cell.pop_back();
                        }
                        break;
                    case soci::dt_date:
                        std::tm when = r.get<std::tm>(i);
                        cell = std::asctime(&when);
                        break;
                }
                html_table << "<td>" << cell << "</td>\n";
                xv_sql_df[r.get_properties(i).get_name()].push_back(cell);
            }
                html_table << "</tr>\n";
        }
        html_table << "</table>";

        // pub_data["text/plain"] = plain_table.str();
        pub_data["text/html"] = html_table.str();

        return pub_data;
        }

    nl::json interpreter::execute_request_impl(int execution_counter,
                                               const std::string& code,
                                               bool /*silent*/,
                                               bool /*store_history*/,
                                               nl::json /*user_expressions*/,
                                               bool /*allow_stdin*/)
    {

        std::vector<std::string> traceback;
        std::vector<std::string> tokenized_code = tokenizer(code);
        xv::df_type xv_sql_df;
        std::vector<std::string> row_headers;
        try
        {
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
                        nl::json data = process_SQL_input(sql, row_headers, xv_sql_df);
                        xv::data_frame data_frame;
                        data_frame.values = xv_sql_df;
                        j["data"] = data_frame;
                        // if (row_headers.size() > 0 && j["encoding"].contains("x")) {
                        //     j["encoding"]["x"]["field"] = row_headers[0];
                        //     j["encoding"]["x"].erase("title");
                        // }
                        // if (row_headers.size() > 1 && j["encoding"].contains("y")) {
                        //     j["encoding"]["y"]["field"] = row_headers[0];
                        //     j["encoding"]["y"].erase("title");
                        // }
                    }
                    auto bundle = nl::json::object();
                    bundle["application/vnd.vegalite.v3+json"] = j;
                    publish_execution_result(execution_counter,
                                             std::move(bundle),
                                             nl::json::object());
                } else {
                    /* Parses LOAD magic */
                    tokenized_code.erase(tokenized_code.begin());
                    this->sql = parse_SQL_magic(tokenized_code);
                }
            } else {
                if (this->sql)
                {
                    /* Shows rich output for tables */
                    if (case_insentive_equals("SELECT", tokenized_code[1]) ||
                        case_insentive_equals("SHOW", tokenized_code[1]))
                    {
                        //log(logINFO) << "running sql\n";
                        nl::json data = process_SQL_input(code, row_headers, xv_sql_df);

                        publish_execution_result(execution_counter,
                                                 std::move(data),
                                                 nl::json::object());

                    }
                    /* Execute all SQL commands that don't output tables */
                    else
                    {
                        *this->sql << code;
                    }
                }
                else
                {
                    throw std::runtime_error("Database was not loaded.");
                }
            }

            nl::json jresult;
            jresult["status"] = "ok";
            jresult["payload"] = nl::json::array();
            jresult["user_expressions"] = nl::json::object();
            return jresult;
        }

        catch (const std::runtime_error& err)
        {
            nl::json jresult;
            jresult["status"] = "error";
            jresult["ename"] = "Error";
            jresult["evalue"] = err.what();
            traceback.push_back((std::string)jresult["ename"] + ": " + (std::string)err.what());
            publish_execution_error(jresult["ename"], jresult["evalue"], traceback);
            traceback.clear();
            return jresult;
        }
    }

    nl::json interpreter::complete_request_impl(const std::string& /*code*/,
                                                int /*cursor_pos*/)
    {
        nl::json jresult;
        jresult["status"] = "ok";
        return jresult;
    };

    nl::json interpreter::inspect_request_impl(const std::string& /*code*/,
                                               int /*cursor_pos*/,
                                               int /*detail_level*/)
    {
        nl::json jresult;
        jresult["status"] = "ok";
        return jresult;
    };

    nl::json interpreter::is_complete_request_impl(const std::string& /*code*/)
    {
        nl::json jresult;
        jresult["status"] = "complete";
        return jresult;
    };

    nl::json interpreter::kernel_info_request_impl()
    {
        nl::json result;
        result["implementation"] = "xtidb";
        result["implementation_version"] = XTIDB_VERSION;

        /* The jupyter-console banner for xeus-tidb is the following:
                                            _ 
                                           | |
            __  _____ _   _ ___   ___  __ _| |
            \ \/ / _ \ | | / __| / __|/ _` | |
             >  <  __/ |_| \__ \ \__ \ (_| | |
            /_/\_\___|\__,_|___/ |___/\__, |_|
                                         | |  
                                         |_| 
           xeus-tidb: a Jupyter kernel for SOCI
           SOCI version: x.x.x
        */

        std::string banner = ""
            "                                _  "
            "                               | | "
            "__  _____ _   _ ___   ___  __ _| | "
            "\\ \\/ / _ \\ | | / __| / __|/ _` | | "
            " >  <  __/ |_| \\__ \\ \\__ \\ (_| | | "
            "/_/\\_\\___|\\__,_|___/ |___/\\__, |_| "
            "                             | |   "
            "                             |_|  "
            "  xeus-tidb: a Jupyter kernel for SOCI\n"
            "  SOCI version: ";
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

    void interpreter::shutdown_request_impl()
    {
    }

}