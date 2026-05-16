// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_INT128_DETAIL_UTILITIES_HPP
#define BOOST_DECIMAL_DETAIL_INT128_DETAIL_UTILITIES_HPP

#include <cstddef>

namespace boost {
namespace int128 {
namespace detail {

template <typename T>
constexpr std::size_t strlen(const T* str) noexcept
{
    std::size_t i {};
    while (*str != '\0')
    {
        ++str;
        ++i;
    }

    return i;
}

} // namespace detail
} // namespace int128
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_INT128_DETAIL_UTILITIES_HPP
