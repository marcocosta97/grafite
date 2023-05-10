/*
 * This file is part of Grafite <https://github.com/marcocosta97/grafite>.
 * Copyright (C) 2023 Marco Costa.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "../bench_template.hpp"

#include <stack>
#include <bits/stdc++.h>
#include <immintrin.h>

#include "../include/REncoder/src/BOBHash32.h"
#include "../include/REncoder/src/RBF.h"
#include "../include/REncoder/src/REncoder.h"

#define USE_SIMD

long long cache_hit = 0;
long long query_count = 0;

/**
 * This file contains the benchmark for the REncoder filter.
 */

template <typename t_itr>
inline RENCODER init_rencoder(const t_itr begin, const t_itr end, const double bpk)
{
    int HASH_NUM = 3;
    int SELF_ADAPT_STEP = 1;
    auto memory = bpk * std::distance(begin, end);
    auto keys = std::vector(begin, end);
    start_timer(build_time);
    RENCODER f = RENCODER();
    f.init(memory, HASH_NUM, 64);
    f.Insert_SelfAdapt(keys, 1);
    stop_timer(build_time);
    return f;
}

template <typename value_type>
inline bool query_rencoder(RENCODER &f, const value_type left, const value_type right)
{
    return f.RangeQuery(left, right);
}

inline size_t size_rencoder(RENCODER &f)
{
    auto [_, size] = f.serialize();
    return size;
}

int main(int argc, char const *argv[])
{
    auto parser = init_parser("bench-rencoder");
    try
    {
        parser.parse_args(argc, argv);
    }
    catch (const std::runtime_error &err)
    {
        std::cerr << err.what() << std::endl;
        std::cerr << parser;
        std::exit(1);
    }

    auto [ keys, queries, arg ] = read_parser_arguments(parser);
    experiment(pass_fun(init_rencoder),pass_ref(query_rencoder),
               pass_ref(size_rencoder), arg, keys, queries);
    print_test();

    return 0;
}