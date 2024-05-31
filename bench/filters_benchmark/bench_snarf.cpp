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
#include "../include/SNARF/include/snarf.cpp"
/**
 * This file contains the benchmark for the SNARF filter.
 */

template <typename t_itr>
inline snarf_updatable_gcs<typename t_itr::value_type> init_snarf(const t_itr begin, const t_itr end, const double bpk)
{
    std::vector<typename t_itr::value_type> keys(begin, end);
    start_timer(build_time);
    snarf_updatable_gcs<typename t_itr::value_type> f;
    f.snarf_init(keys, bpk, 100);
    stop_timer(build_time);
    return f;
}

template <typename value_type>
inline bool query_snarf(snarf_updatable_gcs<value_type> &f, const value_type left, const value_type right)
{
    return f.range_query(left, right);
}

template <typename value_type>
inline size_t size_snarf(snarf_updatable_gcs<value_type> &f)
{
    return f.return_size();
}

int main(int argc, char const *argv[])
{
    argparse::ArgumentParser parser("bench-snarf");
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
    experiment(pass_fun(init_snarf),pass_ref(query_snarf),
               pass_ref(size_snarf), arg, keys, queries);
    print_test();

    return 0;
}