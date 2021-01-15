/***************************************************************************
* Copyright (c) 2020, QuantStack and xeus-tidb contributors                *
*                                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef TEST_DB_HPP
#define TEST_DB_HPP

#include "gtest/gtest.h"

#include "xeus-tidb/xeus_tidb_interpreter.hpp"
#include "xeus-tidb/soci_handler.hpp"

namespace xeus_tidb
{

TEST(xeus_tidb_interpreter, tokenizer)
{
    std::string code = "\%LOAD database.db";
    std::vector<std::string> tokenized_code;
    tokenized_code = tokenizer(code);
    EXPECT_EQ(tokenized_code[1], "database.db");
}

}

#endif
