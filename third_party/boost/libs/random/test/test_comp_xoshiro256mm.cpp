/*
 * Copyright Matt Borland 2025.
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * This file copies and pastes the original code for comparison under the following license
 *
 * Written in 2019 by David Blackman and Sebastiano Vigna (vigna@acm.org)
 *
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <boost/random/xoshiro.hpp>
#include <boost/random/splitmix64.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstdint>

using std::uint64_t;

/* This is xoshiro256** 1.0, one of our all-purpose, rock-solid
   generators. It has excellent (sub-ns) speed, a state (256 bits) that is
   large enough for any parallel application, and it passes all tests we
   are aware of.

   For generating just floating-point numbers, xoshiro256+ is even faster.

   The state must be seeded so that it is not everywhere zero. If you have
   a 64-bit seed, we suggest to seed a splitmix64 generator and use its
   output to fill s. */

static inline uint64_t rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}


static uint64_t s[4];

uint64_t next(void) {
    const uint64_t result = rotl(s[1] * 5, 7) * 9;

    const uint64_t t = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];

    s[2] ^= t;

    s[3] = rotl(s[3], 45);

    return result;
}


/* This is the jump function for the generator. It is equivalent
   to 2^128 calls to next(); it can be used to generate 2^128
   non-overlapping subsequences for parallel computations. */

void jump(void) {
    static const uint64_t JUMP[] = { 0x180ec6d33cfd0aba, 0xd5a61266f0c9392c, 0xa9582618e03fc9aa, 0x39abdc4529b1661c };

    uint64_t s0 = 0;
    uint64_t s1 = 0;
    uint64_t s2 = 0;
    uint64_t s3 = 0;
    for(std::size_t i = 0; i < sizeof JUMP / sizeof *JUMP; i++)
        for(int b = 0; b < 64; b++) {
            if (JUMP[i] & UINT64_C(1) << b) {
                s0 ^= s[0];
                s1 ^= s[1];
                s2 ^= s[2];
                s3 ^= s[3];
            }
            next();
        }

    s[0] = s0;
    s[1] = s1;
    s[2] = s2;
    s[3] = s3;
}



/* This is the long-jump function for the generator. It is equivalent to
   2^192 calls to next(); it can be used to generate 2^64 starting points,
   from each of which jump() will generate 2^64 non-overlapping
   subsequences for parallel distributed computations. */

void long_jump(void) {
    static const uint64_t LONG_JUMP[] = { 0x76e15d3efefdcbbf, 0xc5004e441c522fb3, 0x77710069854ee241, 0x39109bb02acbe635 };

    uint64_t s0 = 0;
    uint64_t s1 = 0;
    uint64_t s2 = 0;
    uint64_t s3 = 0;
    for(std::size_t i = 0; i < sizeof LONG_JUMP / sizeof *LONG_JUMP; i++)
        for(int b = 0; b < 64; b++) {
            if (LONG_JUMP[i] & UINT64_C(1) << b) {
                s0 ^= s[0];
                s1 ^= s[1];
                s2 ^= s[2];
                s3 ^= s[3];
            }
            next();
        }

    s[0] = s0;
    s[1] = s1;
    s[2] = s2;
    s[3] = s3;
}

void test_no_seed()
{
    // Default initialized to contain splitmix64 values
    boost::random::xoshiro256pp boost_rng;
    for (int i {}; i < 10000; ++i)
    {
        boost_rng();
    }

    boost::random::splitmix64 gen;
    for (auto& i : s)
    {
        i = gen();
    }

    for (int i {}; i < 10000; ++i)
    {
        next();
    }

    const auto final_state = boost_rng.state();

    for (std::size_t i {}; i < final_state.size(); ++i)
    {
        BOOST_TEST_EQ(final_state[i], s[i]);
    }
}

void test_basic_seed()
{
    // Default initialized to contain splitmix64 values
    boost::random::xoshiro256pp boost_rng(42ULL);
    for (int i {}; i < 10000; ++i)
    {
        boost_rng();
    }

    boost::random::splitmix64 gen(42ULL);
    for (auto& i : s)
    {
        i = gen();
    }

    for (int i {}; i < 10000; ++i)
    {
        next();
    }

    const auto final_state = boost_rng.state();

    for (std::size_t i {}; i < final_state.size(); ++i)
    {
        BOOST_TEST_EQ(final_state[i], s[i]);
    }
}

void test_jump()
{
    // Default initialized to contain splitmix64 values
    boost::random::xoshiro256pp boost_rng;
    for (int i {}; i < 10000; ++i)
    {
        boost_rng();
    }

    boost::random::splitmix64 gen;
    for (auto& i : s)
    {
        i = gen();
    }

    for (int i {}; i < 10000; ++i)
    {
        next();
    }

    boost_rng.jump();
    jump();

    const auto final_state = boost_rng.state();

    for (std::size_t i {}; i < final_state.size(); ++i)
    {
        BOOST_TEST_EQ(final_state[i], s[i]);
    }
}

void test_long_jump()
{
    // Default initialized to contain splitmix64 values
    boost::random::xoshiro256pp boost_rng;
    for (int i {}; i < 10000; ++i)
    {
        boost_rng();
    }

    boost::random::splitmix64 gen;
    for (auto& i : s)
    {
        i = gen();
    }

    for (int i {}; i < 10000; ++i)
    {
        next();
    }

    boost_rng.long_jump();
    long_jump();

    const auto final_state = boost_rng.state();

    for (std::size_t i {}; i < final_state.size(); ++i)
    {
        BOOST_TEST_EQ(final_state[i], s[i]);
    }
}

int main()
{
    test_no_seed();
    test_basic_seed();
    test_jump();
    test_long_jump();

    return boost::report_errors();
}
