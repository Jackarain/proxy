/*
 *              Copyright Andrey Semashev 2025.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   xorshift.hpp
 * \author Andrey Semashev
 * \date   14.06.2025
 *
 * \brief  This header is the Boost.Log library implementation, see the library documentation
 *         at http://www.boost.org/doc/libs/release/libs/log/doc/html/index.html.
 *
 * This file implements xorshift64 random number generator. See https://en.wikipedia.org/wiki/Xorshift.
 */

#ifndef BOOST_LOG_XORSHIFT_HPP_INCLUDED_
#define BOOST_LOG_XORSHIFT_HPP_INCLUDED_

#include <boost/log/detail/config.hpp>
#include <boost/cstdint.hpp>
#include <boost/log/detail/header.hpp>

namespace boost {

BOOST_LOG_OPEN_NAMESPACE

namespace aux {

//! Xorshift64 random number generator (https://en.wikipedia.org/wiki/Xorshift)
class xorshift64
{
public:
    using result_type = uint64_t;

private:
    uint64_t m_state;

public:
    explicit constexpr xorshift64(uint64_t seed) noexcept :
        m_state(seed)
    {
    }

    result_type operator()() noexcept
    {
        uint64_t state = m_state;
        state ^= state << 13u;
        state ^= state >> 7u;
        state ^= state << 17u;
        m_state = state;
        return state;
    }
};

} // namespace aux

BOOST_LOG_CLOSE_NAMESPACE // namespace log

} // namespace boost

#include <boost/log/detail/footer.hpp>

#endif // BOOST_LOG_XORSHIFT_HPP_INCLUDED_
