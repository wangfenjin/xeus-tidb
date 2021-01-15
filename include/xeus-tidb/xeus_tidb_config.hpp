/***************************************************************************
* Copyright (c) 2020, QuantStack and xeus-SQLite contributors              *
*                                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XTIDB_CONFIG_HPP
#define XTIDB_CONFIG_HPP

// Project version
#define XTIDB_VERSION_MAJOR 0
#define XTIDB_VERSION_MINOR 0
#define XTIDB_VERSION_PATCH 5

// Composing the version string from major, minor and patch
#define XTIDB_CONCATENATE(A, B) XTIDB_CONCATENATE_IMPL(A, B)
#define XTIDB_CONCATENATE_IMPL(A, B) A##B
#define XTIDB_STRINGIFY(a) XTIDB_STRINGIFY_IMPL(a)
#define XTIDB_STRINGIFY_IMPL(a) #a

#define XTIDB_VERSION XTIDB_STRINGIFY(XTIDB_CONCATENATE(XTIDB_VERSION_MAJOR,     \
                      XTIDB_CONCATENATE(.,XTIDB_CONCATENATE(XTIDB_VERSION_MINOR, \
                      XTIDB_CONCATENATE(.,XTIDB_VERSION_PATCH)))))

#ifdef _WIN32
    #ifdef XEUS_TIDB_EXPORTS
        #define XEUS_TIDB_API __declspec(dllexport)
    #else
        #define XEUS_TIDB_API __declspec(dllimport)
    #endif
#else
    #define XEUS_TIDB_API
#endif

#endif
