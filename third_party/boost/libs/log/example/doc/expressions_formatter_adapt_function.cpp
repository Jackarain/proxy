/*
 *             Copyright Andrey Semashev 2025.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */

#include <cstddef>
#include <iostream>
#include <boost/phoenix/function/adapt_function.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/utility/value_ref.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

namespace logging = boost::log;
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

//[ example_expressions_formatter_adapt_function
// Define the attribute keywords
BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", severity_level)
BOOST_LOG_ATTRIBUTE_KEYWORD(channel, "Channel", std::string)

// User-defined formatting function
std::string extract_channel(logging::record_view const& rec)
{
    logging::value_ref< std::string, tag::channel > chan = rec[channel];
    if (chan)
        return chan.get();
    else
        return "-";
}

namespace lazy {
BOOST_PHOENIX_ADAPT_FUNCTION(std::string, extract_channel, ::extract_channel, 1)
}

void init()
{
    logging::add_console_log
    (
        std::clog,
        keywords::format =
            expr::stream << "<" << severity << "> ["
                << lazy::extract_channel(boost::phoenix::placeholders::_1)
                << "] " << expr::smessage
    );
}
//]

int main(int, char*[])
{
    init();
    logging::add_common_attributes();

    src::severity_channel_logger< severity_level, std::string > lg1;
    BOOST_LOG_CHANNEL_SEV(lg1, "general", normal) << "A normal severity level message";
    BOOST_LOG_CHANNEL_SEV(lg1, "general", notification) << "A notification severity level message";
    BOOST_LOG_CHANNEL_SEV(lg1, "general", warning) << "A warning severity level message";
    BOOST_LOG_CHANNEL_SEV(lg1, "general", error) << "An error severity level message";
    BOOST_LOG_CHANNEL_SEV(lg1, "general", critical) << "A critical severity level message";

    src::severity_logger< severity_level > lg2;
    BOOST_LOG_SEV(lg2, normal) << "A normal severity level message";
    BOOST_LOG_SEV(lg2, notification) << "A notification severity level message";
    BOOST_LOG_SEV(lg2, warning) << "A warning severity level message";
    BOOST_LOG_SEV(lg2, error) << "An error severity level message";
    BOOST_LOG_SEV(lg2, critical) << "A critical severity level message";

    return 0;
}
