//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TCP_HPP
#define BOOST_MYSQL_TCP_HPP

#include <boost/mysql/connection.hpp>

#include <boost/asio/ip/tcp.hpp>

namespace boost {
namespace mysql {

/**
 * \brief (Legacy) A connection to MySQL over a TCP socket.
 *
 * \par Legacy
 * New code should use \ref any_connection instead of
 * \ref connection and its helper typedefs, as it's simpler to use
 * and provides the same level of performance.
 */
using tcp_connection = connection<boost::asio::ip::tcp::socket>;

}  // namespace mysql
}  // namespace boost

#endif