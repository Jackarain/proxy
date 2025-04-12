//
// Copyright (c) 2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mqtt5/mqtt_client.hpp>

#include <boost/asio/system_executor.hpp>
#include <boost/asio/ip/tcp.hpp>

int main() {
    boost::asio::system_executor sys;
    boost::mqtt5::mqtt_client<boost::asio::ip::tcp::socket> client(sys);
    return 0;
}
