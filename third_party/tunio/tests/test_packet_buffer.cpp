//
// test_packet_buffer.cpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "tunio/packet_buffer.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <stdexcept>

int main()
{
    using tunio::packet_buffer;

    // 容量与预留
    packet_buffer buf(100, 20);
    assert(buf.capacity() == 100);
    assert(buf.headroom() == 20);
    assert(buf.size() == 0);
    assert(buf.writable_size() == 80);
    assert(buf.headroom_available() == 20);

    // 写入 + commit
    uint8_t *w = buf.writable_data();
    std::memcpy(w, "hello", 5);
    buf.commit(5);
    assert(buf.size() == 5);
    assert(std::memcmp(buf.data(), "hello", 5) == 0);

    // prepend / trim
    buf.prepend(2);
    assert(buf.size() == 7);
    assert(buf.headroom_available() == 18);
    buf.trim(2);
    assert(buf.size() == 5);

    // resize
    buf.resize(10);
    assert(buf.size() == 10);
    try {
        buf.resize(100); // 超出尾部容量
        assert(false && "should throw");
    } catch (const std::length_error &) {
    }

    // reset 复用
    buf.reset();
    assert(buf.size() == 0);
    assert(buf.headroom_available() == 20);
    assert(buf.writable_size() == 80);

    return 0;
}
