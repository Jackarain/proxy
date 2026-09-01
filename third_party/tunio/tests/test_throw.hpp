//
// test_throw.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

// 测试宏（基于 Boost.Test）：与标准 assert 不同，不受 NDEBUG 影响，
// Release 构建下同样生效，避免测试因断言被编译掉而形同虚设。
// 使用前需先包含 boost/test/included/unit_test.hpp（每个测试文件
// 定义 BOOST_TEST_MODULE 后包含）。

// 断言失败：中止当前测试用例并报告失败（等价 BOOST_REQUIRE）.
#define TEST_ASSERT(...) BOOST_REQUIRE(__VA_ARGS__)

// 直接失败：中止当前测试用例并报告信息（等价 BOOST_FAIL）.
#define TEST_THROW(msg) BOOST_FAIL(msg)
