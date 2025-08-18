/*
 *             Copyright Andrey Semashev 2025.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes/constant.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/utility/formatting_ostream.hpp>
#include <boost/log/utility/value_ref.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

namespace logging = boost::log;
namespace attrs = boost::log::attributes;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;

enum severity_level
{
    normal,
    notification,
    warning,
    error,
    critical
};

std::ostream& operator<< (std::ostream& strm, severity_level level)
{
    static const char* strings[] =
    {
        "normal",
        "notification",
        "warning",
        "error",
        "critical"
    };

    if (static_cast< std::size_t >(level) < sizeof(strings) / sizeof(*strings))
        strm << strings[level];
    else
        strm << static_cast< int >(level);

    return strm;
}

//[ example_expressions_wrap_formatter
// Define a custom attribute value type
struct host_address
{
    std::string hostname;
    std::uint_least16_t port = 0;
};

// Define the attribute keywords
BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", severity_level)
BOOST_LOG_ATTRIBUTE_KEYWORD(host, "Host", host_address)

// User-defined formatter for the Host attribute values
void format_host(logging::record_view const& rec, logging::formatting_ostream& stream)
{
    // Formatter implementation outputs directly into the formatting stream
    logging::value_ref< host_address, tag::host > host_ref = rec[host];
    if (host_ref)
    {
        stream << host_ref->hostname;

        if (host_ref->port != 0)
            stream << ":" << host_ref->port;
    }
    else
    {
        stream << "-";
    }
}

void init()
{
    logging::add_console_log
    (
        std::clog,
        keywords::format =
            expr::stream << "<" << severity << "> ["
                << expr::wrap_formatter< char >(&format_host)
                << "] " << expr::smessage
    );
}
//]

int main(int, char*[])
{
    init();
    logging::add_common_attributes();

    src::severity_logger< severity_level > lg1;

    host_address h;
    h.hostname = "hostname";
    h.port = 1234;
    lg1.add_attribute(host.get_name(), attrs::make_constant(h));

    BOOST_LOG_SEV(lg1, normal) << "Connection established";
    BOOST_LOG_SEV(lg1, normal) << "Connection closed";

    src::severity_logger< severity_level > lg2;
    BOOST_LOG_SEV(lg2, normal) << "Message not associated with a host";

    return 0;
}
