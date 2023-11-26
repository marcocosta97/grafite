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

#pragma once

#include <iostream>
#include <vector>
#include <algorithm>
#include <type_traits>
#include <utility>
#include <cmath>
#include <set>
#include <bitset>
#include <random>

#ifdef SUCCINCT_LIB_SDSL
#include "sdsl/sd_vector.hpp"
#include "sdsl/select_support_scan.hpp"
#endif
#ifdef SUCCINCT_LIB_SUX
#include "sux/bits/EliasFano.hpp"
#endif

#if defined(USE_LIBRARY_BOOST_PARALLEL) || defined(USE_LIBRARY_BOOST)
#include <boost/sort/sort.hpp>
#define MAX_THREADS 12
#elif defined(USE_LIBRARY_STL_PARALLEL)
#include <execution>
#endif

namespace grafite {

template<typename T> struct is_vector : public std::false_type {};

template<typename T, typename A>
struct is_vector<std::vector<T, A>> : public std::true_type {};

template <class T, class = void>
struct is_iterable : std::false_type {};

template <class T>
struct is_iterable<T, std::void_t<decltype(std::begin(std::declval<T>())),
        decltype(std::end(std::declval<T>()))>> : std::true_type {};

template <class T>
constexpr bool is_iterable_v = is_iterable<T>::value;

#ifdef SUCCINCT_LIB_SUX
/**
 * The ef_sux_vector class is a wrapper for the Elias-Fano implementation using the SUX library.
 * This implementation is used by the grafite::filter class as default container. This implementation prioritizes
 * the query time over the build time.
 *
 * Note that duplicates are removed by default, if you want to keep them, set the last parameter of the constructor to false.
 */
class ef_sux_vector
{
private:
    sux::bits::EliasFano<> ef;
public:
    ef_sux_vector() = default;

    /**
     * @brief Construct a new ef sux vector object from a sorted input range.
     *
     * Note that duplicates are removed by default, if you want to keep them, set the last parameter of the constructor to false.
     * @tparam t_itr the type of the iterator
     * @param begin the begin iterator
     * @param end the end iterator
     * @param remove_duplicates if true, duplicates are removed
     */
    template <class t_itr>
    ef_sux_vector(const t_itr begin, const t_itr end, const bool remove_duplicates = true)
            : ef(begin, end, remove_duplicates) {}

    template <class t_value>
    bool check_presence_bs(const t_value a, const t_value b) const
    {
        return *(ef.predecessor(b)) >= a;
    }

    template <class t_value>
    bool check_presence(const t_value a, const t_value b) const
    {
        return (ef.rank(a) != ef.rank(b + 1));
    }

    template <class t_value>
    bool check_presence(const t_value x) const
    {
        return check_presence(x, x);
    }

    ef_sux_vector &operator=(ef_sux_vector &&v) noexcept
    {
        if (this != &v)
        {
            ef = std::move(v.ef);
        }
        return *this;
    }

    auto operator[](const size_t i) const
    {
        return ef.predecessor(i);
    }

    friend std::ostream &operator<<(std::ostream &out, const ef_sux_vector &v)
    {
        out << v.ef;
        return out;
    }

    friend std::istream &operator>>(std::istream &in, ef_sux_vector &v)
    {
        in >> v.ef;
        return in;
    }

    [[nodiscard]] uint64_t size() const
    {
        return ef.bitCount() / 8;
    }

};
#endif

/**
 * The ef_sdsl_vector class is a wrapper for the Elias-Fano implementation using the SDSL library. This implementation
 * prioritizes the build time over the query time.
 *
 * Note that duplicates are not removed from the data structure.
 */
#ifdef SUCCINCT_LIB_SDSL
class ef_sdsl_vector
{
private:
    sdsl::sd_vector<sdsl::bit_vector, sdsl::select_support_scan<>> ef;
    decltype(ef)::rank_1_type ef_rank;

public:
    ef_sdsl_vector() = default;

    template <class t_itr>
    ef_sdsl_vector(const t_itr begin, const t_itr end)
            : ef(begin, end), ef_rank(&ef) {}

    ef_sdsl_vector(const ef_sdsl_vector &v)
            : ef(v.ef), ef_rank(v.ef_rank)
    {
        ef_rank.set_vector(&ef);
    }

    ef_sdsl_vector &operator=(ef_sdsl_vector &&v) noexcept
    {
        if (this != &v)
        {
            ef = std::move(v.ef);
            ef_rank = v.ef_rank;
            ef_rank.set_vector(&ef);
        }
        return *this;
    }

    template <class t_value>
    inline bool check_presence(const t_value a, const t_value b) const
    {
        return (ef_rank(a) != ef_rank(b + 1));
    }

    template <class t_value>
    inline bool check_presence(const t_value k) const
    {
        return check_presence(k, k);
    }

    [[nodiscard]] auto size() const
    {
        return sdsl::size_in_bytes(ef) + sdsl::size_in_bytes(ef_rank); //+ sdsl::size_in_bytes(ef_select);
    }

    friend std::ostream &operator<<(std::ostream &out, const ef_sdsl_vector &v)
    {
        v.ef.serialize(out);
        v.ef_rank.serialize(out);
        return out;
    }

    friend std::istream &operator>>(std::istream &in, ef_sdsl_vector &v)
    {
        v.ef.load(in);
        v.ef_rank.load(in);
        v.ef_rank.set_vector(&v.ef);
        return in;
    }

};
#endif

/**
 * The grafite::bucket class implements the heuristic range filter data structure described in the paper: [...].
 * The filter is a probabilistic data structure that can be used to answer range queries in a set of integers.
 * Since the filter is probabilistic, it can return false positives, but no false negatives.
 * Since this range filter is heuristic, the false positive rate (FPR) cannot be controlled by the user.
 *
 * @tparam EliasFanoDS the Elias-Fano data structure used to store the buckets.
 */
#if defined(SUCCINCT_LIB_SUX)
template <class EliasFanoDS = ef_sux_vector>
#elif defined(SUCCINCT_LIB_SDSL)
template <class EliasFanoDS = ef_sdsl_vector>
#else
template <class EliasFanoDS>
#endif
class bucket
{
private:
    using value_type = uint64_t;

    EliasFanoDS bv;
    value_type last;
    value_type s; /* the size of each bucket */

    /**
     * The bucket constructor builds the data structure from a sorted input sequence and the size of each bucket
     * '_s'. The input sequence must be sorted.
     *
     * @tparam t_itr the type of the input sequence
     * @param _s the size of each bucket
     * @param begin the begin iterator of the input sequence
     * @param end the end iterator of the input sequence
     */
    template <class t_itr>
    bucket(const double _s, const t_itr begin, const t_itr end) :
            bv()
    {
        if (begin == end)
            return;
        if (!std::is_sorted(begin, end))
            throw std::runtime_error("error, the input is not sorted");

        if (*(end - 1) > std::numeric_limits<value_type>::max() - 1)
            throw std::overflow_error("error, the universe overflows");

        value_type u = *(end - 1) + 1;
        s = std::ceil(_s);

        if (s <= 0) throw std::runtime_error("error, the requested bucket size is < 1");
        value_type n = ((u + s - 1)/s);
        value_type max_m = std::min(n, (value_type) std::distance(begin, end));

        std::vector<typename t_itr::value_type> pos;
        pos.reserve(max_m);
        pos.push_back((*begin)/s);
        for (auto it = begin + 1; it < end; ++it)
        {
            auto curr = (*it)/s;
            if (curr != pos.back())
                pos.push_back(curr);
        }

        last = pos.back();
        bv = EliasFanoDS(pos.begin(), pos.end());
    }
public:
    bucket() = default;

    /**
     * Constructs a bucket data structure from a sorted range of integers by specifying the (approximate) size of the
     * resulting range filter as bits per key. In fact, the (approximate) size of a bucket 's' can be calculated as
     * 's = u/(n * 2^(bpk - 2))', where 'u' is the universe size and 'bpk' is the number of desired bits per key.
     *
     * @tparam t_itr the type of the iterator
     * @param begin the beginning of the sorted range of keys
     * @param end the end of the sorted range of keys
     * @param bpk the desired number of bits per key
     */
    template <class t_itr>
    bucket(const t_itr begin, const t_itr end, const double bpk) :
        bucket(((double) (*(end - 1) + 1)/(std::distance(begin, end) * std::exp2(bpk - 2))), begin, end) {}

    bucket (bucket &&b) noexcept
    {
        bv = std::move(b.bv);
        s = std::move(b.s);
        last = std::move(b.last);
    }

    bucket &operator=(bucket &&b) noexcept
    {
        if (this != &b)
        {
            bv = std::move(b.bv);
            s = std::move(b.s);
            last = std::move(b.last);
        }
        return *this;
    }

    /**
     * The query function checks if the range [left, right] is present in the bucket.
     *
     * @param left the left endpoint of the range
     * @param right the right endpoint of the range
     * @return true if the range is present, false otherwise
     */
    inline bool query(const value_type left, const value_type right) const
    {
        auto l = left/s, r = right/s;
        if (l > last) return false;
        if (r > last) r = last;

        if constexpr (std::is_same_v<EliasFanoDS, ef_sux_vector>)
            return bv.check_presence_bs(l, r);
        return bv.check_presence(l, r);
    }

    /**
     * The query function checks if the element [x] is present in the bucket.
     *
     * @param x the point
     * @return true if the element is present, false otherwise
     */
    inline bool query(const value_type x) const
    {
        return query(x, x);
    }

    /**
     * The size function returns the size of the bucket in bytes.
     *
     * @return the size of the bucket in bytes
     */
    inline auto size() const
    {
        return bv.size() + sizeof(bucket);
    }
};


/**
 * The grafite::filter class implements the range filter data structure described in the paper: [...].
 * The filter is a probabilistic data structure that can be used to answer range queries in a set of integers.
 * Since the filter is probabilistic, it can return false positives, but no false negatives. The probability of
 * false positives can be controlled by the user, and it is called the false positive rate (FPR). The FPR is
 * defined as the probability that the filter returns true for a range that is not in the set. The FPR is
 * guaranteed to be less than or equal to 'eps' for range queries of size at most 'L'. The FPR for range queries of
 * size 'l' can be precisely measured by the formula 'FPR_l = (l * n)/r'. Where it still holds the equation 'r = (n*L)/eps'.
 *
 * We advice to use the filter using the Elias-Fano data structure, since it is the most efficient one and is the
 * one proposed in the paper. However, any data structure that is either iterable or supports the following operations
 * can be used:
 *  - check_presence(a, b): returns true if it exists an element 'x' in the set such that 'a <= x <= b'. False otherwise.
 *  - check_presence(a): returns true if it exists an element 'x' in the set such that 'x == a'. False otherwise.
 *  - size(): returns the size of the data structure in bytes.
 *
 * If using the Elias-Fano data structure we proved the following bounds:
 *  - The space complexity is 'n * log(L/eps) + 2n + o(n)' bits (i.e. 2 bpk more than the lower bound).
 *  - The query time is 'O(log(L/eps))'. If setting a constant space budget 'b', the query time is 'O(b) = O(1)'.
 *
 * NOTE: the vector of hashed elements passed to RangeEmptinessDS may have duplicated elements (due to the hashing
 *       collision). The RangeEmptinessDS should handle this case. In fact, it must be disabled if the user cares
 *       about the approximate range count. Otherwise, it can be approximated using the formula
 *       'approx number of collisions = n*eps/(2L)'.
 *
 * If available (and enabled at compile time) this data structure makes use of the Boost library and/or multithreading
 * to speed-up the sorting algorithm. The user can enable/disable these features by defining the macros:
 *  - USE_LIBRARY_BOOST_PARALLEL: enables the use of the Boost library in multithreading.
 *  - USE_LIBRARY_BOOST: enables the use of the Boost library in single thread.
 *  - USE_LIBRARY_STL_PARALLEL: enables the use of the standard library in multithreading.
 *
 * @tparam RangeEmptinessDS the data structure used to check the emptiness of a range.
 * @tparam default_bpk_overhead the default number of bits per key overhead used by the data structure used to
 *                              check the emptiness of a range.
 */
#if defined(SUCCINCT_LIB_SUX)
template <class RangeEmptinessDS = ef_sux_vector, unsigned int default_bpk_overhead = 2>
#elif defined(SUCCINCT_LIB_SDSL)
template <class RangeEmptinessDS = ef_sdsl_vector, unsigned int default_bpk_overhead = 2>
#else
template <class RangeEmptinessDS, unsigned int default_bpk_overhead = 0>
#endif
class filter
{
private:
    using value_type = uint64_t; /* the type of the elements in the set */

    constexpr static value_type p = (1UL << 61) - 1UL; /* a huge prime */
    static std::random_device rd;
    static std::mt19937 gen;
    static std::uniform_int_distribution<value_type> distr;

    RangeEmptinessDS ds; /* the container data structure used to check the emptiness of a range */
    value_type a, b, r, n_items; /* the parameters of the data structure */
    value_type first, last; /* the first and last element of the set */

    /**
     * @brief Hashes the input value using the formula: '(((a * (x / r) + b) % p) + x) % r'.
     *
     * This function is used to map the elements of the set to the range [0, r), where 'r' is the size of the reduced
     * universe. Moreover, this hash function is 2-independent, that is, two different elements of the set are mapped
     * to the same hashed values with probability 1/r.
     * See ^[https://en.wikipedia.org/wiki/K-independent_hashing] for more details.
     *
     * @tparam T the type of the input value (must be an arithmetic type)
     * @param x the input value
     * @return the hashed value
     */
    template <class T, class = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
    inline value_type hash(const T x) const
    {
        return (((a * (x / r) + b) % p) + x) % r;
    }

#ifdef SUCCINCT_LIB_SDSL
    template <class Q = RangeEmptinessDS>
    class std::enable_if<std::is_same<Q, sdsl::int_vector<0>>::value, void>::type
    compress()
    {
        sdsl::util::bit_compress(ds);
    }
#endif

    /**
     * Main constructor for the grafite::filter class. It takes as input a range of elements and the size of the reduced
     * universe r (see the paper for more details). The constructor computes the parameters of the data structure and
     * builds the filter.
     *
     * Note that if the maximum element in the input range is smaller than 'r', the constructor will throw an exception.
     * In fact, this is means to construct an approximate range filter such that its size in bits per key is greater
     * that the size of a lossless encoding of the input keys without any approximation (which occupies log2(u/n) + 2 bpk).
     *
     * @tparam t_itr the type of the iterator used to iterate over the input range
     * @param r the size of the reduced universe
     * @param begin the iterator to the first element of the input range
     * @param end the iterator to the last element of the input range
     */
    template <class t_itr>
    filter(const value_type _r, const t_itr begin, const t_itr end)
            : ds(), a(distr(gen)), b(distr(gen)), r(_r), n_items(std::distance(begin, end)), first(), last()
    {
        if (begin == end)
            return;

        while ((a == 0) || (a >= p)) { a = distr(gen); }
        while (b >= p) { b = distr(gen);}
        a %= r, b %= r;

        std::vector<value_type> temp(n_items);
        typename t_itr::value_type max_input_key = 0;

        /*
         * The following code hashes the input elements into the temporary vector and computes the maximum element in
         * the input range (since the input is not required to be sorted).
         */
        std::transform(begin, end, temp.begin(), [&](auto x) {
            max_input_key = std::max(max_input_key, x);
            return hash(x); });

        if (max_input_key < this->r) /* equivalent to bpk > 2 + log2(u/n) */
            throw std::runtime_error("error, the requested bpk is higher than a lossless compressed encoding of the input data");

#if defined(USE_LIBRARY_BOOST_PARALLEL)
        /*
         * The following code uses the Boost library to sort the elements in parallel. The number of threads is set to
         * the minimum between the number of available threads and 8.
         */
#ifdef MAX_THREADS
        auto n_threads = std::min(std::thread::hardware_concurrency(), (unsigned int) MAX_THREADS);
#else
        auto n_threads = std::min(std::thread::hardware_concurrency(), (unsigned int) 8);
#endif
        boost::sort::block_indirect_sort(temp.begin(), temp.end(), n_threads);
#elif defined(USE_LIBRARY_BOOST)
        /*
         * The following code uses the Boost library to sort the elements in a single thread, via spreadsort function.
         * This function is faster than std::sort and exploits the fact that the size of the maximum hash is bounded
         * via hybrid radix sort.
         */
        boost::sort::spreadsort::spreadsort(temp.begin(), temp.end());
#elif defined(USE_LIBRARY_STL_PARALLEL)
        /*
         * The following code uses the STL library to sort the elements in parallel via execution policies.
         */
        std::sort(std::execution::par, temp.begin(), temp.end());
#else
        std::sort(temp.begin(), temp.end());
#endif

        first = temp.front(), last = temp.back();
        ds = RangeEmptinessDS{temp.begin(), temp.end()};

#ifdef SUCCINCT_LIB_SDSL
        if constexpr (std::is_same_v<RangeEmptinessDS, sdsl::int_vector<>>)
            compress();
#endif
    }

public:

    filter() = default;

    /**
     * @brief This constructor allows to specify the false positive rate 'eps' desired for the range queries of size 'L'.
     * The size of the reduced universe r is calculated as r = (n * L)/eps, where 'n' is the number of elements in the input
     * range. The false positive rate for a range query of size 'l' is bounded by the formula 'eps_l = (l * eps)/L'.
     *
     * @tparam t_itr the iterator type
     * @param begin the start iterator of the input keys
     * @param end the end iterator of the input keys
     * @param eps the false positive rate desired for the range queries of size 'L'
     * @param L the maximum range size for which the false positive rate is guaranteed
     */
    template <class t_itr>
    filter(const t_itr begin, const t_itr end, const double eps, const typename t_itr::value_type L)
            : filter((std::distance(begin, end) * L) / eps, begin, end) {}


    /**
     * @brief This constructor allows to specify the number of bits per key (bpk) desired.
     * The size of the reduced universe r is calculated as r = n * 2^(bpk - overhead). The overhead is a parameter that can be specified as
     * a template argument by the user and represents the overhead, in bits per key, of the underlying container data
     * structure with respect to the optimal space consumption of log2(r/n) bits per key. The overhead for the
     * Elias-Fano data structure is 2. The false positive rate for a range query of size 'l' is bounded by the formula
     * 'eps_l = l/2^(bpk - overhead)'.
     *
     * @tparam t_itr the iterator type
     * @param begin the start iterator of the input keys
     * @param end the end iterator of the input keys
     * @param bpk the desired bits per key (bpk) occupied by the filter
     */
    template <class t_itr>
    filter(const t_itr begin, const t_itr end, const double bpk)
            : filter(std::ceil(std::distance(begin, end) * std::exp2(bpk - default_bpk_overhead)), begin, end) {}


    filter (filter &&rf) noexcept
    {
        ds = std::move(rf.ds);
        first = std::move(rf.first);
        last = std::move(rf.last);
        n_items = std::move(rf.n_items);
        a = std::move(rf.a);
        b = std::move(rf.b);
        r = std::move(rf.r);
    }

    filter& operator=(filter &&rf) noexcept
    {
        if (this != &rf)
        {
            ds = std::move(rf.ds);
            first = std::move(rf.first);
            last = std::move(rf.last);
            n_items = std::move(rf.n_items);
            a = std::move(rf.a);
            b = std::move(rf.b);
            r = std::move(rf.r);
        }

        return *this;
    }

    /**
     * @brief Range query method for the Grafite range filter.
     * Both query endpoints are inclusive, i.e. [left, right]. The expected false positive rate is (right - left)*n/r.
     *
     * @param left the left endpoint, inclusive
     * @param right the right endpoint, inclusive
     * @return tt if a key possibly intersects the range, ff if a key definitely does not
     */
    template <class T>
    bool query(const T left, const T right) const
    {
        if (right < left)
            throw std::runtime_error("range parameters are not sorted");
        if (left == right)
            return query(left);

        auto hash_left = hash(left), hash_right = hash(right);

        if (hash_left > hash_right)
            return ((first <= hash_right) || (last >= hash_left));
        else if ((hash_left > last) || (hash_right < first))
            return false;
        else if (hash_right > last)
            return (hash_left <= last);
        else if (hash_left < first)
            return (first <= hash_right);

        /**
         * We verify if the container data structure is iterable. In this case we apply the std::lower_bound method
         * to verify if exists a certain key 'x' in the range [h(left), h(right)], otherwise we use the method
         * 'check_presence' provided by the user container class.
         */
        if constexpr (is_iterable_v<RangeEmptinessDS>)
        {
            auto next = std::lower_bound(ds.begin(), ds.end(), hash_left);
            return (next != ds.end()) && (*next <= hash_right);
        }

        return ds.check_presence(hash_left, hash_right);
    }

    /**
     * @brief Point query method for the Grafite range filter.
     * The expected false positive rate is n/r.
     *
     * @param k the key to query
     * @return tt if the key possibly intersects in the set, ff if definitely does not
     */
    template <class T>
    bool query(const T k) const
    {
        auto hash_k = hash(k);

        if ((hash_k > last) || (hash_k < first))
            return false;

        /**
         * We verify if the container data structure is iterable. In this case we apply the std::lower_bound method
         * to verify if hash(k) is in the set, otherwise we use the method 'check_presence' provided by the user
         * container class.
         */
        if constexpr (is_iterable_v<RangeEmptinessDS>)
        {
            auto v = std::lower_bound(ds.begin(), ds.end(), hash_k);
            return (v != ds.end()) && (*v == hash_k);
        }

        return ds.check_presence(hash_k);
    }

    /**
     * @brief Returns the size in bytes of the Grafite range filter.
     * The expected size of the grafite range filter is n * bpk bits. Where bpk can be calculated as
     * bpk = log2(r/n) + overhead, where r is the size of the reduced universe and overhead is the overhead of the
     * underlying container data structure (2 with Elias-Fano).
     *
     * @return the size in bytes of the Grafite range filter
     */
    auto size() const
    {
        if constexpr (is_vector<RangeEmptinessDS>())
            return sizeof(filter) + (ds.size() * sizeof(value_type));
#ifdef SUCCINCT_LIB_SDSL
        else if constexpr (std::is_same_v<RangeEmptinessDS, sdsl::int_vector<>>)
            return sizeof(filter) + sdsl::size_in_bytes(ds);
#endif
        return sizeof(filter) + ds.size();
    }

    friend std::ostream &operator<<(std::ostream &out, const filter &rf)
    {
        out.write(reinterpret_cast<const char *>(&rf.first), sizeof(rf.first));
        out.write(reinterpret_cast<const char *>(&rf.last), sizeof(rf.last));
        out.write(reinterpret_cast<const char *>(&rf.n_items), sizeof(rf.n_items));
        out.write(reinterpret_cast<const char *>(&rf.a), sizeof(rf.a));
        out.write(reinterpret_cast<const char *>(&rf.b), sizeof(rf.b));
        out.write(reinterpret_cast<const char *>(&rf.r), sizeof(rf.r));
        out << rf.ds;
        return out;
    }

    friend std::istream &operator>>(std::istream &in, filter &rf)
    {
        in.read(reinterpret_cast<char *>(&rf.first), sizeof(rf.first));
        in.read(reinterpret_cast<char *>(&rf.last), sizeof(rf.last));
        in.read(reinterpret_cast<char *>(&rf.n_items), sizeof(rf.n_items));
        in.read(reinterpret_cast<char *>(&rf.a), sizeof(rf.a));
        in.read(reinterpret_cast<char *>(&rf.b), sizeof(rf.b));
        in.read(reinterpret_cast<char *>(&rf.r), sizeof(rf.r));
        in >> rf.ds;
        return in;
    }

};

template <class RangeEmptinessContainer, unsigned int default_bpk_overhead>
std::random_device filter<RangeEmptinessContainer, default_bpk_overhead>::rd;

template <class RangeEmptinessContainer, unsigned int default_bpk_overhead>
std::mt19937 filter<RangeEmptinessContainer, default_bpk_overhead>::gen(filter<RangeEmptinessContainer, default_bpk_overhead>::rd());

template <class RangeEmptinessContainer, unsigned int default_bpk_overhead>
std::uniform_int_distribution<typename filter<RangeEmptinessContainer, default_bpk_overhead>::value_type>
        filter<RangeEmptinessContainer, default_bpk_overhead>::distr(1, std::numeric_limits<typename filter<RangeEmptinessContainer, default_bpk_overhead>::value_type>::max());

} // namespace grafite