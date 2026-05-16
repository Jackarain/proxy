//
// Copyright (c) 2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_TEST_RESOLVER_HPP
#define BOOST_MQTT5_TEST_RESOLVER_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/prepend.hpp>

namespace boost::mqtt5::test {

class test_resolver {
public:
    using executor_type = asio::any_io_executor;
    using results_type = asio::ip::tcp::resolver::results_type;

private:
    executor_type _ex;

public:
    static constexpr std::string_view invalid_host = "example.invalid";

    explicit test_resolver(executor_type ex) : _ex(std::move(ex)) {}

    executor_type get_executor() const noexcept { return _ex; }

    template <typename CompletionToken>
    decltype(auto) async_resolve(
        std::string_view host, std::string_view port, CompletionToken&& token
    ) {
        results_type results;
        boost::system::error_code ec;

        if (host == invalid_host)
            ec = asio::error::host_not_found;
        else
            results = results_type::create(
                asio::ip::tcp::endpoint(asio::ip::make_address_v4("127.0.0.1"), 1883),
                std::string(host), std::string(port)
            );

        return asio::post(
            get_executor(),
            asio::prepend(
                std::forward<CompletionToken>(token),
                ec, results
            )
        );
    }
};

} // namespace boost::mqtt5::test

#endif // !BOOST_MQTT5_TEST_RESOLVER_HPP
