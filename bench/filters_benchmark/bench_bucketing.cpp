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

auto default_container = "sux";

template <typename REContainer, typename t_itr>
inline grafite::bucket<REContainer> init_bucketing(const t_itr begin, const t_itr end, const double s)
{
    start_timer(build_time);
    grafite::bucket<REContainer> filter(begin, end, s);
    stop_timer(build_time);
    return filter;
}

template <typename REContainer, typename value_type>
inline bool query_bucketing(grafite::bucket<REContainer> &f, const value_type left, const value_type right)
{
    return f.query(left, right);
}

template <typename REContainer>
inline size_t size_bucketing(const grafite::bucket<REContainer> &f)
{
    return f.size();
}

int main(int argc, char const *argv[])
{
    auto parser = init_parser("bench-bucketing");
    parser.add_argument("--ds")
        .nargs(1)
        .required()
        .default_value(default_container);

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
    auto container = parser.get<std::string>("ds");

    if (keys.back() == std::numeric_limits<uint64_t>::max())
        keys.resize(keys.size() - 1);
    auto L = (std::get<1>(queries[0]) - std::get<0>(queries[0]));
    if (L == 0) ++L;

    std::cout << "[+] expected fpr: " << (double) L/std::exp2(arg - 2) << std::endl;
    std::cout << "[+] using container `" << container << "`" << std::endl;
    if (container == "sux")
        experiment(pass_fun(init_bucketing<grafite::ef_sux_vector>),pass_ref(query_bucketing),
            pass_ref(size_bucketing), arg, keys, queries);
    else if (container == "sdsl")
        experiment(pass_fun(init_bucketing<grafite::ef_sdsl_vector>),pass_ref(query_bucketing),
            pass_ref(size_bucketing), arg, keys, queries);
    else
        throw std::runtime_error("error, range emptiness data structure unknown.");
    print_test();

    return 0;
}


