//
// test_packet_buffer.cpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#define BOOST_TEST_MODULE packet_buffer
#include <boost/test/included/unit_test.hpp>
#include "tunio/packet_buffer.hpp"
#include "test_throw.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <stdexcept>

BOOST_AUTO_TEST_CASE(packet_buffer)
{
    using tunio::packet_buffer;

    // 容量与预留
    packet_buffer buf(100, 20);
    TEST_ASSERT(buf.capacity() == 100);
    TEST_ASSERT(buf.headroom() == 20);
    TEST_ASSERT(buf.size() == 0);
    TEST_ASSERT(buf.writable_size() == 80);

    // 写入 + commit
    uint8_t *w = buf.writable_data();
    std::memcpy(w, "hello", 5);
    buf.commit(5);
    TEST_ASSERT(buf.size() == 5);
    TEST_ASSERT(std::memcmp(buf.data(), "hello", 5) == 0);

    // resize
    buf.resize(10);
    TEST_ASSERT(buf.size() == 10);
    try {
        buf.resize(100); // 超出尾部容量
        TEST_ASSERT(false && "should throw");
    } catch (const std::length_error &) {
    }

    // reset 复用
    buf.reset();
    TEST_ASSERT(buf.size() == 0);
    TEST_ASSERT(buf.writable_size() == 80);
}
