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

#include <utility>

#include "../bench_template.hpp"
#include "../include/Rosetta/dst.h"
/**
 * This file contains the benchmark for the Rosetta filter.
 */

template <typename t_itr, typename... Args>
inline DstFilter<BloomFilter<>> init_rosetta(const t_itr begin, const t_itr end, const double bpk, Args... args)
{
    auto sample_rate = 0.1;
    auto maxlen = 64, cutoff = 0, dfs_diff = 100, bfs_diff = 32;

    auto&& t = std::forward_as_tuple(args...);
    auto queries = std::get<0>(t);
    auto model_queries = std::round(queries.size() * sample_rate);
    start_timer(modelling_time);
    DstFilter<BloomFilter<true>, true> dst_stat(dfs_diff, bfs_diff,
                                                [](vector<size_t> x) -> vector<size_t> {
                                                    for (size_t i=0; i<x.size(); ++i) {x[i]*=1.44;} return x; });

    vector<Bitwise> tmp;
    tmp.emplace_back(false, maxlen); //maxlen = 64?
    dst_stat.AddKeys(tmp);

    for (auto it = queries.begin(); it < queries.begin() + model_queries; ++it)
    {
        auto [first, second, _] = *it;
        (void)dst_stat.Query(first, second);
    }
    auto qdist = std::vector<size_t>(dst_stat.qdist_.begin(), dst_stat.qdist_.end());
    stop_timer(modelling_time);
    std::vector<Bitwise> string_keys(begin, end);
    start_timer(build_time)
    auto dst_inst = DstFilter<BloomFilter<>>(dfs_diff, bfs_diff,
                                       [&](vector<size_t> x) ->
                                       vector<size_t> { return calc_dst(std::move(x), bpk, qdist, cutoff); });
    dst_inst.AddKeys(string_keys);
    stop_timer(build_time)
    return dst_inst;
}

template <typename value_type>
inline bool query_rosetta(DstFilter<BloomFilter<>> &f, const value_type left, const value_type right)
{
    if (left == right)
        return f.Query(left);
    /* query ranges are [x, y) for Rosetta, so we sum up 1 to the right endpoint */
    return f.Query(left, right + 1);
}

inline size_t size_rosetta(DstFilter<BloomFilter<>> &f)
{
    auto ser = f.serialize();
    return ser.second;
}

int main(int argc, char const *argv[])
{
    auto parser = init_parser("bench-rosetta");

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

    auto [keys, queries, arg] = read_parser_arguments(parser);

    experiment(pass_fun(init_rosetta), pass_ref(query_rosetta),
               pass_ref(size_rosetta), arg, keys, queries, queries);
    print_test();

    return 0;
}
