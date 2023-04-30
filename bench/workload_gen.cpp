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

#include <cassert>
#include <vector>

#include "bench_utils.hpp"
#include <argparse/argparse.hpp>

static const std::vector<std::string> kdist_names = {"kuniform", "knormal"};
static const std::vector<std::string> kdist_default = {"kuniform"};
static const std::vector<std::string> qdist_names = {"quniform", "qnormal", "qcorrelated", "qtrue"};
static const std::vector<std::string> qdist_default = {"quniform", "qcorrelated"};

auto s = 10999;
#define seed s++
// std::random_device rd;
// #define seed rd()

bool save_binary = true;
bool allow_true_queries = false;
bool mixed_queries = false;

auto default_n_keys = 200'000'000;
auto default_n_queries = 10'000'000;
auto default_range_size = std::vector<int>{0, 5, 10}; /* {point queries, 2^{5}, 2^{10}}*/
auto default_corr_degree = 0.8;

InputKeys<uint64_t> keys_from_file = InputKeys<uint64_t>();

#define PBSTR "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||"
#define PBWIDTH 60

void save_keys(InputKeys<uint64_t> &keys, const std::string &file)
{
    if (save_binary)
        write_to_binary_file(keys, file);
    else
        save_keys_to_file(keys, file + ".txt");
}

void save_queries(Workload<uint64_t> &work, const std::string &l_keys, const std::string &r_keys = "", const std::string &res_keys = "")
{
    if (save_binary)
    {
        std::vector<uint64_t> left_q, right_q;
        std::vector<int> result_q;
        left_q.reserve(work.size());
        right_q.reserve(work.size());
        result_q.reserve(work.size());
        for (auto w : work)
        {
            auto [left, right, result] = w;
            left_q.push_back(left);
            right_q.push_back(right);
            result_q.push_back(result);
        }
        write_to_binary_file(left_q, l_keys);
        if (!r_keys.empty())
            write_to_binary_file(right_q, r_keys);

        write_to_binary_file(result_q, res_keys);
    }
    else
        save_workload_to_file(work, l_keys + ".txt", r_keys + ".txt");
}

/**
 * @brief Given a point and a range size, returns the range [point, point + range_size - 1]
 */
auto point_to_range = [](auto point, auto range_size) {
    return std::make_pair(point, (point + range_size) - 1);
};

void printProgress(double percentage) {
    static int last_percentage = 0;
    int val = (int) (percentage * 100);
    if (last_percentage == val)
        return;

    int lpad = (int) (percentage * PBWIDTH);
    int rpad = PBWIDTH - lpad;
    last_percentage = val;
    printf("\r%3d%% [%.*s%*s]", val, lpad, PBSTR, rpad, "");
    fflush(stdout);
}

bool create_dir_recursive(const std::string &dir_name) {
    std::error_code err;
    if (!std::filesystem::create_directories(dir_name, err)) {
        if (std::filesystem::exists(dir_name))
            return true; // the folder probably already existed

        std::cerr << "Failed to create [" << dir_name << "]" << std::endl;
        return false;
    }
    return true;
}

InputKeys<uint64_t> generate_keys_uniform(uint64_t n_keys, uint64_t max_key = UINT64_MAX - 1) {
    std::set<uint64_t> keys_set;

    std::mt19937 gen(seed);
    std::uniform_int_distribution<uint64_t> distr_value(0, max_key);

    auto n_iterations = 0;
    while (keys_set.size() < n_keys) {
        if (++n_iterations >= 10 * n_keys)
            throw std::runtime_error("error: timeout for the input keys generation");

        auto key = distr_value(gen);
        keys_set.insert(key);
        printProgress(((double) keys_set.size()) / n_keys);
    }

    return {keys_set.begin(), keys_set.end()};
}

InputKeys<uint64_t> generate_keys_normal(uint64_t n_keys, long double sd) {
    std::set<uint64_t> keys_set;

    std::mt19937 gen(seed);
    auto nor_dist = std::normal_distribution<long double>(1ULL << (64 - 1), sd);

    auto n_iterations = 0;
    while (keys_set.size() < n_keys) {
        if (++n_iterations >= 10 * n_keys)
            throw std::runtime_error("error: timeout for the input keys generation");

        auto key = nor_dist(gen);
        keys_set.insert(key);
        printProgress(((double) keys_set.size()) / n_keys);
    }

    return {keys_set.begin(), keys_set.end()};
}

Workload<uint64_t> generate_true_queries(InputKeys<uint64_t> &keys,uint64_t n_queries, uint64_t range_size, bool mixed = false)
{
    std::set<uint64_t> chosen_points;

    std::mt19937 gen_random_keys(seed);
    std::uniform_int_distribution<uint64_t> rand_keys_distr(0, keys.size() - 1);

    std::mt19937 gen_random_offset(seed);
    std::uniform_int_distribution<uint64_t> rand_offset_distr(0, range_size - 1);

    while (chosen_points.size() != n_queries)
        chosen_points.insert(keys[rand_keys_distr(gen_random_keys)]);

    Workload<uint64_t> w;
    w.reserve(n_queries);

    std::mt19937 gen_range(seed);
    auto left_pow = 0, right_pow = static_cast<int>(std::floor(std::log2(range_size)));
    std::uniform_int_distribution<uint64_t> range_distr(left_pow, right_pow);

    for (auto point : chosen_points)
    {
        if (mixed)
        {
            auto r = static_cast<uint64_t>(std::exp2(range_distr(gen_range)));
            std::uniform_int_distribution<uint64_t>::param_type d(0, r - 1);
            rand_offset_distr.param(d);
            auto [left, right] = point_to_range(point - rand_offset_distr(gen_random_offset), r);
            w.emplace_back(left, right, true);
        }
        else
        {
            auto [left, right] = point_to_range(point - rand_offset_distr(gen_random_offset), range_size);
            w.emplace_back(left, right, true);
        }

    }

    return w;
}


Workload<uint64_t> generate_synth_queries(const std::string& qdist, InputKeys<uint64_t> &keys,
                                          uint64_t n_queries, uint64_t min_range, uint64_t max_range,
                                          const double corr_degree, const long double stddev) {
    std::set<std::tuple<uint64_t, uint64_t, bool>> q;

    std::vector<uint64_t> middle_points;

    if (qdist == "qnormal")
        middle_points = generate_keys_normal(10 * n_queries, stddev);
    else if (qdist == "quniform")
        middle_points = generate_keys_uniform(3 * n_queries);
    else // qdist == "qcorrelated"
    {
        std::mt19937 g(seed);
        auto t = keys;
        std::shuffle(t.begin(), t.end(), g);
        auto i = 0;
        middle_points.reserve(n_queries);
        while (i < n_queries) {
            auto n = std::min<uint64_t>(keys.size(), n_queries - i);
            std::copy(t.begin() + i, t.begin() + i + n, middle_points.begin() + i);
            i += n;
        }
    }

    std::mt19937 gen_range(seed);
    auto left_pow = (min_range) ? static_cast<int>(std::floor(std::log2(min_range))) : 0, right_pow = static_cast<int>(std::floor(std::log2(max_range)));
    std::uniform_int_distribution<uint64_t> range_distr(left_pow, right_pow);

    std::mt19937 gen_corr(seed);
    std::uniform_int_distribution<uint64_t> corr_distr(1, (1UL << std::lround(30 * (1 - corr_degree))));

    std::mt19937 gen_pos_middle_points(seed);
    std::uniform_int_distribution<int> pos_distr(1, middle_points.size() - 1);
    auto n_iterations = 0;
    auto i = 0;
    while (q.size() < n_queries) {
        if (++n_iterations >= 100 * n_queries) {
            std::string in;
            std::cout << std::endl
                      << "application seems stuck, close it or save less query? (y/n/save) ";
            std::cin >> in;
            if (in == "save")
                break;
            else if (in == "y")
                throw std::runtime_error("error: timeout for the workload generation");
            n_iterations = 0;
        }

        auto range_size = (min_range == max_range) ? min_range : static_cast<uint64_t>(std::exp2(range_distr(gen_range)));

        uint64_t left, right;

        if (qdist == "quniform")
            std::tie(left, right) = point_to_range(middle_points[i++], range_size);
        else if (qdist == "qnormal")
            std::tie(left, right) = point_to_range(middle_points[pos_distr(gen_pos_middle_points)], range_size);
        else // qdist == qcorrelated
        {
            auto p = middle_points[q.size()] + corr_distr(gen_corr);
            std::tie(left, right) = point_to_range(p, range_size);
        }
        if (std::numeric_limits<uint64_t>::max() - left < range_size)
            continue;

        auto q_result = (range_size > 1) ? vector_range_query(keys, left, right) : vector_point_query(keys, left);
        if (!allow_true_queries && q_result)
            continue;

        q.emplace(left, right, q_result);
        printProgress(((double) q.size()) / n_queries);
    }

    return {q.begin(), q.end()};
}

Workload<uint64_t> generate_synth_queries(const std::string& qdist, InputKeys<uint64_t> &keys,
                                          uint64_t n_queries, uint64_t range_size,
                                          const double corr_degree, const long double stddev) {
    return generate_synth_queries(qdist, keys, n_queries, range_size, range_size, corr_degree, stddev);
}

void generate_synth_datasets(const std::vector<std::string> &kdist, const std::vector<std::string> &qdist,
                             uint64_t n_keys, uint64_t n_queries,
                             std::vector<int> range_size_list, // uint64_t min_range, uint64_t max_range,
                             const double corr_degree = 0.8, const long double stddev = (long double) UINT64_MAX * 0.1) {
    std::vector<uint64_t> ranges(range_size_list.size());
    std::transform(range_size_list.begin(), range_size_list.end(), ranges.begin(), [](auto v) {
        return (1ULL << v);
    });

    std::cout << "[+] starting dataset generation" << std::endl;
    std::cout << "[+] n=" << n_keys << ", q=" << n_queries << std::endl;
    std::cout << "[+] kdist=";
    std::copy(kdist.begin(), kdist.end(), std::ostream_iterator<std::string>(std::cout, ","));
    std::cout << std::endl
              << "[+] qdist=";
    std::copy(qdist.begin(), qdist.end(), std::ostream_iterator<std::string>(std::cout, ","));
    std::cout << std::endl;
    std::cout << "[+] corr_degree=" << corr_degree << std::endl;


    for (const auto& k: kdist) {
        std::string root_path = "./" + k + "/";
        auto keys = (k == "kuniform") ? generate_keys_uniform(n_keys) : generate_keys_normal(n_keys, stddev);
        std::cout << std::endl
                  << "[+] generated `" << k << "` keys" << std::endl;
        for (const auto& q: qdist) {
            for (auto i = 0; i < ranges.size(); i++) {
                auto range_size = ranges[i];
                auto queries = (q == "qtrue") ? generate_true_queries(keys, n_queries, range_size) :
                        generate_synth_queries(q, keys, n_queries, range_size, range_size, corr_degree, stddev);
                std::cout << std::endl
                          << "[+] generated `" << q << "_" << range_size_list[i] << "` queries" << std::endl;
                std::string queries_path = root_path + std::to_string(range_size_list[i]) + "_" + q + "/";
                if (!create_dir_recursive(queries_path))
                    throw std::runtime_error("error, impossible to create dir");

                if (allow_true_queries && range_size == 1)
                    save_queries(queries, queries_path + "point", "",queries_path + "result");
                else if (allow_true_queries)
                    save_queries(queries, queries_path + "left", queries_path + "right", queries_path + "result");
                else if (range_size == 1)
                    save_queries(queries, queries_path + "point");
                else
                    save_queries(queries, queries_path + "left", queries_path + "right");
                std::cout << "[+] queries wrote at " << queries_path << std::endl;
            }

            if (mixed_queries)
            {
                auto range_size = ranges.back();
                auto range_size_min = 1;

                auto queries = (q == "qtrue") ? generate_true_queries(keys, n_queries, range_size, true) :
                               generate_synth_queries(q, keys, n_queries, range_size_min, range_size, corr_degree, stddev);

                auto queries_path = root_path + std::to_string(range_size_list.back()) + "M_" + q + "/"; /* mixed */
                if (!create_dir_recursive(queries_path))
                    throw std::runtime_error("error, impossible to create dir");

                if (allow_true_queries)
                    save_queries(queries, queries_path + "left", queries_path + "right", queries_path + "result");
                else
                    save_queries(queries, queries_path + "left", queries_path + "right");

                std::cout << std::endl << "[+] queries wrote at " << queries_path << std::endl;
            }
        }

        save_keys(keys, root_path + "keys");
        std::cout << "[+] keys wrote at " << root_path << std::endl;
    }
}

std::pair<InputKeys<uint64_t>, std::vector<Workload<uint64_t>>>
generate_real_queries(std::vector<uint64_t> &data, uint64_t n_queries, std::vector<uint64_t> &range_list, bool remove_duplicates = true) {
    std::vector<std::pair<uint64_t, int>> candidates;

    std::vector<Workload<uint64_t>> queries_list((mixed_queries) ? range_list.size() + 1 : range_list.size());

    auto max_range_size = *std::max_element(range_list.begin(), range_list.end());

    for (size_t i = 0; i < data.size() - 1; i++) {
        if (data[i + 1] < data[i])
            throw std::runtime_error("error, sequence is not strictly increasing");
        if (remove_duplicates && data[i] == data[i + 1])
            data.erase(data.begin() + i);
        else if (max_range_size < data[i + 1] - data[i])
            candidates.emplace_back(data[i], i);

        // dst += (data[i + 1] - data[i]);
        printProgress((double) i / (data.size() - 2));
    }
    std::cout << std::endl;
    // long double avg = (long double) dst / (data.size() - 1);
    // std::cout << "dst: " << dst << ", avg: " << avg << std::endl;

    if (std::numeric_limits<uint64_t>::max() - data.back() > max_range_size)
        candidates.emplace_back(data.back(), data.size() - 1);

    if (candidates.size() < n_queries)
        throw std::runtime_error(
                "error, can build at most " + std::to_string(candidates.size()) + " over " + std::to_string(n_queries) +
                " queries");

    std::cout << "[+] found " << candidates.size() << " candidates.";

    std::mt19937 gen_shuffle(seed);
    std::shuffle(candidates.begin(), candidates.end(), gen_shuffle);

    auto indexes = std::vector<int>(n_queries);
    std::transform(candidates.begin(), candidates.begin() + n_queries, indexes.begin(),
                   [](const std::pair<uint64_t, int> &p) { return p.second; });
    std::sort(indexes.begin(), indexes.end());

    std::vector<uint64_t> keys;
    keys.reserve(data.size() - n_queries);
    auto it = indexes.begin();
    for (auto i = 0; i < data.size(); i++) {
        if (i != *it)
            keys.push_back(data[i]);
        else if (it != indexes.end())
            ++it;
    }

    if (!std::is_sorted(keys.begin(), keys.end()))
        throw std::runtime_error("unexpected error, keys are not sorted.");

    std::mt19937 gen_range(seed);
    auto right_pow = static_cast<int>(std::floor(std::log2(range_list.back())));
    std::uniform_int_distribution<uint64_t> range_distr(0, right_pow);

    for (auto it = candidates.begin(); it < candidates.begin() + n_queries; ++it) {
        for (auto i = 0; i < range_list.size(); i++)
        {
            auto range_q = point_to_range(it->first, range_list[i]);
            if (range_q.first > range_q.second)
                throw std::runtime_error("unexpected error, queries are not sorted");
            queries_list[i].emplace_back(range_q.first, range_q.second, false);
        }

        if (mixed_queries)
        {
            auto range_size = static_cast<uint64_t>(std::exp2(range_distr(gen_range)));
            auto range_q = point_to_range(it->first, range_size);
            queries_list[range_list.size()].emplace_back(range_q.first, range_q.second, false);
        }
    }

    return std::make_pair(keys, queries_list);
}

template <typename value_type = uint64_t>
void generate_real_dataset(const std::string& file, uint64_t n_queries, std::vector<int> range_size_list) {
    std::vector<uint64_t> ranges(range_size_list.size());
    std::transform(range_size_list.begin(), range_size_list.end(), ranges.begin(), [](auto v) {
        return (1ULL << v);
    });

    std::string base_filename = file.substr(file.find_last_of("/\\") + 1);
    std::string::size_type pos = base_filename.find('_');
    std::string dir_name = (pos != std::string::npos) ? base_filename.substr(0, pos) : base_filename;
    std::string root_path = "./" + dir_name + "/";
    auto temp_data = read_data_binary<value_type>(file);
    auto all_data = std::vector<uint64_t>(temp_data.begin(), temp_data.end());
    assert(all_data.size() > n_queries);

    std::cout << "[+] starting `" << dir_name << "` dataset generation" << std::endl;
    auto [keys, queries_list] = generate_real_queries(all_data, n_queries, ranges);
    std::cout << std::endl << "[+] full dataset generated" << std::endl;
    std::cout << "[+] nkeys=" << keys.size() << ", nqueries=" << queries_list[0].size() << std::endl;

    for (auto i = 0; i < ranges.size(); i++) {
        auto range_size = ranges[i];

        std::string queries_path = root_path + std::to_string(range_size_list[i]) + "/";
        if (!create_dir_recursive(queries_path))
            throw std::runtime_error("error, impossible to create dir");
        if (range_size == 1)
            save_queries(queries_list[i], queries_path + "point");
        else
            save_queries(queries_list[i], queries_path + "left", queries_path + "right");
        std::cout << "[+] queries wrote at " << queries_path << std::endl;
    }

    if (mixed_queries)
    {
        std::string queries_path = root_path + std::to_string(range_size_list.back()) + "M/";
        if (!create_dir_recursive(queries_path))
            throw std::runtime_error("error, impossible to create dir");

        save_queries(queries_list.back(), queries_path + "left", queries_path + "right");
        std::cout << "[+] queries wrote at " << queries_path << std::endl;
    }

    save_keys(keys, root_path + "keys");
    std::cout << "[+] keys wrote at " << root_path << std::endl;

}

int main(int argc, char const *argv[]) {
    argparse::ArgumentParser parser("workload_gen");

    parser.add_argument("--kdist")
            .nargs(argparse::nargs_pattern::at_least_one)
            .required()
            .default_value(kdist_default);

    parser.add_argument("--qdist")
            .nargs(argparse::nargs_pattern::at_least_one)
            .required()
            .default_value(qdist_default);

    parser.add_argument("--range-size")
            .help("List of the ranges to compute (as power of 2 and/or 0 for point queries)")
            .nargs(argparse::nargs_pattern::at_least_one)
            .required()
            .default_value(default_range_size)
            .scan<'d', int>();

    parser.add_argument("-n", "--n-keys")
            .help("the number of input keys")
            .required()
            .default_value(uint64_t(default_n_keys))
            .scan<'u', uint64_t>()
            .nargs(1);

    parser.add_argument("-q", "--n-queries")
            .help("the number of queries")
            .nargs(1)
            .required()
            .default_value(uint64_t(default_n_queries))
            .scan<'u', uint64_t>();

    parser.add_argument("--binary-keys")
            .help("generates the queries from binary file")
            .nargs(argparse::nargs_pattern::at_least_one);

    parser.add_argument("--allow-true")
            .help("allows the generation of true queries in the workload (note, this will produce 3 files)")
            .implicit_value(true)
            .default_value(false);

    parser.add_argument("--mixed")
            .help("generates mixed range queries in the range [0, 2^last_range_size]")
            .implicit_value(true)
            .default_value(false);

    parser.add_argument("--corr-degree")
            .help("Correlation degree for correlated workloads")
            .required()
            .default_value(default_corr_degree)
            .scan<'g', double>();

    try {
        parser.parse_args(argc, argv);
    }
    catch (const std::runtime_error &err) {
        std::cerr << err.what() << std::endl;
        std::cerr << parser;
        std::exit(1);
    }

    auto kdist = parser.get<std::vector<std::string>>("--kdist");
    auto qdist = parser.get<std::vector<std::string>>("--qdist");

    if (!std::all_of(kdist.cbegin(), kdist.cend(), [](const std::string& s) {
        return (std::find(kdist_names.begin(), kdist_names.end(), s) != kdist_names.end());
    }))
        throw std::runtime_error("error, kdist unknown");

    if (!std::all_of(qdist.cbegin(), qdist.cend(), [](const std::string& s) {
        return (std::find(qdist_names.begin(), qdist_names.end(), s) != qdist_names.end());
    }))
        throw std::runtime_error("error, qdist unknown");

    auto n_keys = parser.get<uint64_t>("-n");
    auto n_queries = parser.get<uint64_t>("-q");
    auto ranges_int = parser.get<std::vector<int>>("--range-size");
    auto corr_degree = parser.get<double>("--corr-degree");

    allow_true_queries = parser.get<bool>("--allow-true");
    mixed_queries = parser.get<bool>("--mixed");

    assert((n_keys > 0) && (n_queries > 0));
    assert(!kdist.empty());
    assert(!qdist.empty());

    if (auto file_list = parser.present<std::vector<std::string>>("--binary-keys"))
    {
        for (const auto &file : *file_list)
        {
            if (file.find("uint32") != std::string::npos)
                generate_real_dataset<uint32_t>(file, n_queries, ranges_int);
            else
                generate_real_dataset(file, n_queries, ranges_int);
        }

    }
    else
        generate_synth_datasets(kdist, qdist, n_keys, n_queries, ranges_int, corr_degree);

    return 0;
}
