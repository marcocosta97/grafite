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
#include "grafite/grafite.hpp"

/**
 * This file contains the benchmark for the Bucketing approach heuristic filter.
 */
template <typename t_itr>
inline grafite::bucket<> init_bucketing(const t_itr begin, const t_itr end, const double s)
{
    start_timer(build_time);
    grafite::bucket<> filter(begin, end, s);
    stop_timer(build_time);
    return filter;
}

template <typename value_type>
inline bool query_bucketing(grafite::bucket<> &f, const value_type left, const value_type right)
{
    return f.query(left, right);
}

inline size_t size_bucketing(const grafite::bucket<> &f)
{
    return f.size();
}

int main(int argc, char const *argv[])
{
    auto parser = init_parser("bench-bucketing");
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
    if (keys.back() == std::numeric_limits<uint64_t>::max())
        keys.resize(keys.size() - 1);
    auto L = (std::get<1>(queries[0]) - std::get<0>(queries[0]));
    if (L == 0) ++L;

    std::cout << "[+] expected fpr: " << (double) L/std::exp2(arg - 2) << std::endl;
    experiment(pass_fun(init_bucketing),pass_ref(query_bucketing),
            pass_ref(size_bucketing), arg, keys, queries);
    print_test();

    return 0;
}


