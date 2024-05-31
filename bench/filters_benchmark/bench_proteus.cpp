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
#include "../include/Proteus/include/proteus.hpp"
#include "../include/Proteus/include/util.hpp"
#include "../include/Proteus/include/modeling.hpp"

/**
 * This file contains the benchmark for the Proteus filter.
 */

#define SUPPRESS_STDOUT
const auto default_sample_rate = 0.2;

template <typename t_itr, typename... Args>
inline proteus::Proteus init_proteus(const t_itr begin, const t_itr end, const double bpk, Args... args)
{
    std::vector<typename t_itr::value_type> keys(begin, end);
    auto&& t = std::forward_as_tuple(args...);
    auto queries_temp = std::get<0>(t);
    auto sample_rate = std::get<1>(t);
    auto klen = 64;
    auto queries = std::vector<std::pair<uint64_t, uint64_t>>(queries_temp.size());
    std::transform(queries_temp.begin(), queries_temp.end(), queries.begin(), [](auto x) {
        auto [left, right, result] = x;
        return std::make_pair(left, right);
    });

    auto sample_queries = proteus::sampleQueries(queries, sample_rate);
    start_timer(modelling_time);
    std::tuple<size_t, size_t, size_t> parameters = proteus::modeling(keys, sample_queries, bpk, klen);
    stop_timer(modelling_time);
    start_timer(build_time)
    auto p = proteus::Proteus(keys, std::get<0>(parameters), // Trie Depth
                              std::get<1>(parameters),       // Sparse-Dense Cutoff
                              std::get<2>(parameters),       // Bloom Filter Prefix Length
                              bpk);
    stop_timer(build_time);

    return p;
}

template <typename value_type>
inline bool query_proteus(proteus::Proteus &f, const value_type left, const value_type right)
{
    if (left == right)
        return f.Query(left);
    /* query ranges are [x, y) for Proteus, so we sum up 1 to the right endpoint */
    return f.Query(left, right + 1);
}

inline size_t size_proteus(proteus::Proteus &f)
{
    auto ser = f.serialize();
    return ser.second;
}

int main(int argc, char const *argv[])
{
    argparse::ArgumentParser parser("bench-proteus");
    init_parser(parser);
    try
    {
        parser.parse_args(argc, argv);
    }
    catch (const std::exception& err)
    {
        std::cerr << err.what() << std::endl;
        std::cerr << parser;
        std::exit(1);
    }

    auto [ keys, queries, arg ] = read_parser_arguments(parser);

    experiment(pass_fun(init_proteus),pass_ref(query_proteus),
               pass_ref(size_proteus), arg, keys, queries, queries, default_sample_rate);
    print_test();

    return 0;
}
