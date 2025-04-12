//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mqtt5/detail/topic_validation.hpp>
#include <boost/mqtt5/detail/utf8_mqtt.hpp>

#include <boost/test/unit_test.hpp>

#include <string>

BOOST_AUTO_TEST_SUITE(utf8_mqtt/*, *boost::unit_test::disabled()*/)

std::string to_str(int utf8ch) {
    if (utf8ch < 0x80)
        return { char(utf8ch) };
    if (utf8ch < 0x800)
        return {
            char((utf8ch >> 6) | 0xC0),
            char((utf8ch & 0x3F) | 0x80)
        };
    if (utf8ch < 0xFFFF)
        return {
            char((utf8ch >> 12) | 0xE0),
            char(((utf8ch >> 6) & 0x3F) | 0x80),
            char((utf8ch & 0x3F) | 0x80)
        };
    return {
        char((utf8ch >> 18) | 0xF0),
        char(((utf8ch >> 12) & 0x3F) | 0x80),
        char(((utf8ch >> 6) & 0x3F) | 0x80),
        char((utf8ch & 0x3F) | 0x80)
    };
}

BOOST_AUTO_TEST_CASE(utf8_string_validation) {
    using namespace boost::mqtt5::detail;

    BOOST_CHECK(validate_mqtt_utf8("stringy") == validation_result::valid);
    BOOST_CHECK(validate_mqtt_utf8("") == validation_result::valid);
    BOOST_CHECK(validate_mqtt_utf8(std::string(75000, 'a')) == validation_result::invalid);

    BOOST_CHECK(validate_mqtt_utf8(to_str(0x1)) == validation_result::invalid);
    BOOST_CHECK(validate_mqtt_utf8(to_str(0x1F)) == validation_result::invalid);
    BOOST_CHECK(validate_mqtt_utf8(to_str(0x20)) == validation_result::valid);
    BOOST_CHECK(validate_mqtt_utf8(to_str(0x7E)) == validation_result::valid);
    BOOST_CHECK(validate_mqtt_utf8(to_str(0x7F)) == validation_result::invalid);
    BOOST_CHECK(validate_mqtt_utf8(to_str(0x9F)) == validation_result::invalid);
    BOOST_CHECK(validate_mqtt_utf8(to_str(0xA0)) == validation_result::valid);
    BOOST_CHECK(validate_mqtt_utf8(to_str(0xD800)) == validation_result::invalid);
    BOOST_CHECK(validate_mqtt_utf8(to_str(0xDFFF)) == validation_result::invalid);
    BOOST_CHECK(validate_mqtt_utf8(to_str(0xFDD0)) == validation_result::invalid);
    BOOST_CHECK(validate_mqtt_utf8(to_str(0xFDEF)) == validation_result::invalid);
    BOOST_CHECK(validate_mqtt_utf8(to_str(0xFDF0)) == validation_result::valid);
    BOOST_CHECK(validate_mqtt_utf8(to_str(0x1FFFE)) == validation_result::invalid);
    BOOST_CHECK(validate_mqtt_utf8(to_str(0x1FFFF)) == validation_result::invalid);
}

BOOST_AUTO_TEST_CASE(topic_filter_validation) {
    using namespace boost::mqtt5::detail;

    BOOST_CHECK(validate_topic_filter("") == validation_result::invalid);
    BOOST_CHECK(validate_topic_filter("topic") == validation_result::valid);
    BOOST_CHECK(validate_topic_filter("topic/subtopic") == validation_result::valid);

    BOOST_CHECK(validate_topic_filter("#") == validation_result::valid);
    BOOST_CHECK(validate_topic_filter("#sport") == validation_result::invalid);
    BOOST_CHECK(validate_topic_filter("sport#") == validation_result::invalid);
    BOOST_CHECK(validate_topic_filter("sport/#/tennis") == validation_result::invalid);
    BOOST_CHECK(validate_topic_filter("#/sport") == validation_result::invalid);
    BOOST_CHECK(validate_topic_filter("spo#rt/#") == validation_result::invalid);
    BOOST_CHECK(validate_topic_filter("sport/#") == validation_result::valid);
    BOOST_CHECK(validate_topic_filter("sport/tennis/#") == validation_result::valid);
    BOOST_CHECK(validate_topic_filter("sport/tennis#") == validation_result::invalid);

    BOOST_CHECK(validate_topic_filter("+") == validation_result::valid);
    BOOST_CHECK(validate_topic_filter("+/") == validation_result::valid);
    BOOST_CHECK(validate_topic_filter("/+") == validation_result::valid);
    BOOST_CHECK(validate_topic_filter("+/+") == validation_result::valid);
    BOOST_CHECK(validate_topic_filter("+/+/+") == validation_result::valid);
    BOOST_CHECK(validate_topic_filter("+sport") == validation_result::invalid);
    BOOST_CHECK(validate_topic_filter("sport+") == validation_result::invalid);
    BOOST_CHECK(validate_topic_filter("sport+/player1") == validation_result::invalid);
    BOOST_CHECK(validate_topic_filter("sport/+player1") == validation_result::invalid);
    BOOST_CHECK(validate_topic_filter("sport/+") == validation_result::valid);
    BOOST_CHECK(validate_topic_filter("sport/+/player1") == validation_result::valid);
    BOOST_CHECK(validate_topic_filter("+/sport/+/player1/+") == validation_result::valid);

    BOOST_CHECK(validate_topic_filter("+/tennis/#") == validation_result::valid);
}

BOOST_AUTO_TEST_CASE(topic_name_validation) {
    using namespace boost::mqtt5::detail;

    BOOST_CHECK(validate_topic_name("") == validation_result::invalid);
    BOOST_CHECK(validate_topic_name("topic") == validation_result::valid);
    BOOST_CHECK(validate_topic_name("topic/subtopic") == validation_result::valid);

    BOOST_CHECK(validate_topic_name("#") == validation_result::has_wildcard_character);
    BOOST_CHECK(validate_topic_name("sport#") == validation_result::has_wildcard_character);
    BOOST_CHECK(validate_topic_name("sport/#") == validation_result::has_wildcard_character);

    BOOST_CHECK(validate_topic_name("+") == validation_result::has_wildcard_character);
    BOOST_CHECK(validate_topic_name("+sport") == validation_result::has_wildcard_character);
    BOOST_CHECK(validate_topic_name("sport+") == validation_result::has_wildcard_character);
    BOOST_CHECK(validate_topic_name("sport/+/player1") == validation_result::has_wildcard_character);

    BOOST_CHECK(validate_topic_name("+/tennis/#") == validation_result::has_wildcard_character);
}

BOOST_AUTO_TEST_CASE(topic_alias_name_validation) {
    using namespace boost::mqtt5::detail;

    BOOST_CHECK(validate_topic_alias_name("") == validation_result::valid);
    BOOST_CHECK(validate_topic_alias_name("topic") == validation_result::valid);
    BOOST_CHECK(validate_topic_alias_name("topic/subtopic") == validation_result::valid);

    BOOST_CHECK(validate_topic_alias_name("#") == validation_result::has_wildcard_character);
    BOOST_CHECK(validate_topic_alias_name("sport#") == validation_result::has_wildcard_character);
    BOOST_CHECK(validate_topic_alias_name("sport/#") == validation_result::has_wildcard_character);

    BOOST_CHECK(validate_topic_alias_name("+") == validation_result::has_wildcard_character);
    BOOST_CHECK(validate_topic_alias_name("+sport") == validation_result::has_wildcard_character);
    BOOST_CHECK(validate_topic_alias_name("sport+") == validation_result::has_wildcard_character);
    BOOST_CHECK(validate_topic_alias_name("sport/+/player1") == validation_result::has_wildcard_character);

    BOOST_CHECK(validate_topic_alias_name("+/tennis/#") == validation_result::has_wildcard_character);
}

BOOST_AUTO_TEST_CASE(shared_topic_filter_validation) {
    using namespace boost::mqtt5::detail;

    BOOST_CHECK(validate_shared_topic_filter("") == validation_result::invalid);
    BOOST_CHECK(validate_shared_topic_filter("$shared/grp/topic") == validation_result::invalid);
    BOOST_CHECK(validate_shared_topic_filter("$share//grp/topic") == validation_result::invalid);
    BOOST_CHECK(validate_shared_topic_filter("$share/grp+/topic") == validation_result::invalid);
    BOOST_CHECK(validate_shared_topic_filter("$share/#grp/topic") == validation_result::invalid);

    BOOST_CHECK(validate_shared_topic_filter("$share/grp/topic") == validation_result::valid);
    BOOST_CHECK(validate_shared_topic_filter("$share/grp/topic/#") == validation_result::valid);
    BOOST_CHECK(validate_shared_topic_filter("$share/grp/+/topic/#") == validation_result::valid);
    BOOST_CHECK(validate_shared_topic_filter("$share/grp/topic/+") == validation_result::valid);

    BOOST_CHECK(validate_shared_topic_filter("$share/grp/topic/#", false) == validation_result::has_wildcard_character);
    BOOST_CHECK(validate_shared_topic_filter("$share/grp/+/topic/#", false) == validation_result::has_wildcard_character);
    BOOST_CHECK(validate_shared_topic_filter("$share/grp/topic/+", false) == validation_result::has_wildcard_character);
}

BOOST_AUTO_TEST_SUITE_END();
