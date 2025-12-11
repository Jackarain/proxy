/*
* Copyright Matt Borland 2025.
 * Distributed under the Boost Software License, Version 1.0. (See
        * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org for most recent version including documentation.
 *
 * $Id$
 */

#include <boost/random.hpp>
#include <boost/core/lightweight_test.hpp>
#include <array>
#include <vector>
#include <random>
#include <cstdint>

template <class Arr>
bool all_words_equal(const Arr& a)
{
    for (std::size_t i = 1; i < a.size(); ++i)
    {
        if (a[i] != a[0]) return false;
    }

    return true;
}

template <class Engine, class SSeq>
void test_engine_with_sseq(SSeq& sseq)
{
    Engine eng(sseq);
    auto st = eng.state();
    BOOST_TEST(!all_words_equal(st));
}

int main()
{
    const std::vector<std::uint32_t> seed_words = {
        0x12345678u, 0x9abcdef0u, 0xc0ffee12u, 0xdeadbeefu
    };

    // xoshiro128mm (4 x 32-bit state)
    std::seed_seq stdseq(seed_words.begin(), seed_words.end());
    boost::random::seed_seq bseq(seed_words.begin(), seed_words.end());
    test_engine_with_sseq<boost::random::xoshiro128mm>(stdseq);
    test_engine_with_sseq<boost::random::xoshiro128mm>(bseq);

    // xoshiro256mm (4 x 64-bit state)
    std::seed_seq stdseq2(seed_words.begin(), seed_words.end());
    boost::random::seed_seq bseq2(seed_words.begin(), seed_words.end());
    test_engine_with_sseq<boost::random::xoshiro256mm>(stdseq2);
    test_engine_with_sseq<boost::random::xoshiro256mm>(bseq2);

    return boost::report_errors();
}
