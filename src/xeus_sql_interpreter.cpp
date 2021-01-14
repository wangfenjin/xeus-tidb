/***************************************************************************
* Copyright (c) 2020, QuantStack and xeus-sql contributors                *
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

#include "tabulate/table.hpp"
#include "xeus/xinterpreter.hpp"

#include "xeus-sql/xeus_sql_interpreter.hpp"
#include "xeus-sql/soci_handler.hpp"

namespace xeus_sql
{

    void interpreter::configure_impl()
    {
    }

    nl::json interpreter::process_SQL_input(const std::string& code,
                                            xv::df_type& xv_sql_df)
    {
        nl::json pub_data;
        std::vector<std::string> plain_table_header;
        std::vector<std::string> plain_table_row;

        tabulate::Table plain_table;
        std::stringstream html_table("");

        soci::rowset<soci::row> rows = ((*this->sql).prepare << code);

        /* Builds table header */
        const soci::row& first_row = *rows.begin();
        if (first_row.size() == 0)
            return pub_data;

        html_table << "<table>\n<tr>\n";
        for(std::size_t i = 0; i < first_row.size(); ++i)
        {
            std::string name = first_row.get_properties(i).get_name();
            html_table << "<th>" << name << "</th>\n";
            xv_sql_df[name] = { "name" };
            plain_table_header.push_back(name);
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
                    case soci::dt_double:
                        cell = std::to_string(r.get<double>(i));
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
                    case soci::dt_date:
                        std::tm when = r.get<std::tm>(i);
                        cell = std::asctime(&when);
                        break;
                }
                html_table << "<td>" << cell << "</td>\n";
                plain_table_row.push_back(cell);
                xv_sql_df[r.get_properties(i).get_name()].push_back(cell);
            }
                html_table << "</tr>\n";
        }
        html_table << "</table>";

        pub_data["text/plain"] = plain_table.str();
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
        std::string sanitized_code = xv_bindings::sanitize_string(code);
        std::vector<std::string> tokenized_input = xv_bindings::tokenizer(sanitized_code);

        xv::df_type xv_sql_df;
        try
        {
            /* Runs magic */
            if(xv_bindings::is_magic(tokenized_input))
            {
                /* Removes "%" symbol */
                tokenized_input[0].erase(0, 1);

                /* Runs xvega magic and SQL code */
                if(xv_bindings::is_xvega(tokenized_input))
                {
                    /* Removes XVEGA_PLOT command */
                    tokenized_input.erase(tokenized_input.begin());

                    nl::json chart;
                    std::vector<std::string> xvega_input, sql_input;

                    std::tie(xvega_input, sql_input) = split_xv_sql_input(tokenized_input);
                    std::stringstream stringfied_sql_input;
                    for (size_t i = 0; i < sql_input.size(); i++) {
                        stringfied_sql_input << " " << sql_input[i];
                    }

                    process_SQL_input(stringfied_sql_input.str(), xv_sql_df);

                    chart = xv_bindings::process_xvega_input(xvega_input,
                                                             xv_sql_df);

                    publish_execution_result(execution_counter,
                                             std::move(chart),
                                             nl::json::object());

                    nl::json jresult;
                    jresult["status"] = "ok";
                    jresult["payload"] = nl::json::array();
                    jresult["user_expressions"] = nl::json::object();
                    return jresult;
                }

                /* Parses LOAD magic */
                this->sql = parse_SQL_magic(tokenized_input);
            }
            /* Runs SQL code */
            else
            {
                if (this->sql)
                {
                    /* Shows rich output for tables */
                    if (true || xv_bindings::case_insentive_equals("SELECT", tokenized_input[0]))
                    {
                        nl::json data = process_SQL_input(code, xv_sql_df);

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
                    throw("Database was not loaded.");
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
        result["implementation"] = "xsql";
        result["implementation_version"] = XSQL_VERSION;

        /* The jupyter-console banner for xeus-sql is the following:
                                            _ 
                                           | |
            __  _____ _   _ ___   ___  __ _| |
            \ \/ / _ \ | | / __| / __|/ _` | |
             >  <  __/ |_| \__ \ \__ \ (_| | |
            /_/\_\___|\__,_|___/ |___/\__, |_|
                                         | |  
                                         |_| 
           xeus-sql: a Jupyter kernel for SOCI
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
            "  xeus-sql: a Jupyter kernel for SOCI\n"
            "  SOCI version: ";
        banner.append(XSQL_VERSION);

        result["banner"] = banner;
        //TODO: This should change with the language
        result["language_info"]["name"] = "mysql";
        result["language_info"]["codemirror_mode"] = "sql";
        result["language_info"]["version"] = XSQL_VERSION;
        result["language_info"]["mimetype"] = "";
        result["language_info"]["file_extension"] = "";
        return result;
    }

    void interpreter::shutdown_request_impl()
    {
    }

}
