//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_TEST_PACKET_UTIL_HPP
#define BOOST_MQTT5_TEST_PACKET_UTIL_HPP

#include <boost/mqtt5/detail/control_packet.hpp>
#include <boost/mqtt5/detail/traits.hpp>

#include <boost/mqtt5/impl/codecs/message_decoders.hpp>
#include <boost/mqtt5/impl/codecs/message_encoders.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/range/algorithm/transform.hpp>

#include <bitset>
#include <cstdint>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace boost::mqtt5::test {

using control_code_e = boost::mqtt5::detail::control_code_e;

template <typename ...Strings>
std::string concat_strings(Strings&&... strings) {
    std::ostringstream stream;
    (stream << ... << std::forward<Strings>(strings));
    return stream.str();
}

namespace detail {

inline qos_e extract_qos(uint8_t flags) {
    auto byte = (flags & 0b0110) >> 1;
    return qos_e(byte);
}

inline control_code_e extract_code(uint8_t control_byte) {
    constexpr uint8_t mask = 0b11110000;
    constexpr uint8_t publish_bits = 0b0011;
    constexpr uint8_t special_mask = 0b00000010;
    constexpr control_code_e codes_with_non_zero_end[] = {
        control_code_e::pubrel, control_code_e::subscribe,
        control_code_e::unsubscribe
    };

    if ((control_byte >> 4) == publish_bits)
        return control_code_e::publish;
    if ((control_byte & mask) == control_byte)
        return control_code_e(control_byte & mask);

    for (const auto& special_code : codes_with_non_zero_end)
        if (control_byte == (uint8_t(special_code) | special_mask))
            return special_code;

    return control_code_e::no_packet;
}


inline std::string_view code_to_str(control_code_e code) {
    switch (code) {
        case control_code_e::connect: return "CONNECT";
        case control_code_e::connack: return "CONNACK";
        case control_code_e::publish: return "PUBLISH";
        case control_code_e::puback: return "PUBACK";
        case control_code_e::pubrec: return "PUBREC";
        case control_code_e::pubrel: return "PUBREL";
        case control_code_e::pubcomp: return "PUBCOMP";
        case control_code_e::subscribe: return "SUBSCRIBE";
        case control_code_e::suback: return "SUBACK";
        case control_code_e::unsubscribe: return "UNSUBSCRIBE";
        case control_code_e::unsuback: return "UNSUBACK";
        case control_code_e::auth: return "AUTH";
        case control_code_e::disconnect: return "DISCONNECT";
        case control_code_e::pingreq: return "PINGREQ";
        case control_code_e::pingresp: return "PINGRESP";
        default: return "NO PACKET";
    }
}

template <typename Props>
inline std::string to_readable_props(Props props) {
    std::ostringstream stream;
    props.visit([&stream](const auto&, const auto& v) -> bool {
        using namespace boost::mqtt5::detail;
        if constexpr (is_optional<decltype(v)>)
            if (v.has_value())
                stream << *v << " ";
        if constexpr (is_vector<decltype(v)>)
            for (size_t i = 0; i < v.size(); i++) {
                if constexpr (is_pair<decltype(v[i])>)
                    stream << "(" << v[i].first << ", " << v[i].second << ")";
                else
                    stream << v[i];
                if (i + 1 < v.size())
                    stream << ", ";
            }
        return true;
    });
    return stream.str();
}

using byte_citer = std::string::const_iterator;

template <
    control_code_e code,
    std::enable_if_t<code == control_code_e::connect, bool> = true
>
inline std::string to_string(uint32_t remain_length, byte_citer& it) {
    auto connect = decoders::decode_connect(remain_length, it);
    if (!connect.has_value())
        return "Cannot decode Connect packet!";
    auto& [cli_id, uname, pwd, keep_alive, clean_start, props, will] = *connect;

    return concat_strings(
        code_to_str(code),
        " uname: ", uname.value_or(""), " pwd: ", pwd.value_or(""),
        " keep_alive: ", keep_alive, " clean_start: ", clean_start,
        " props: ", to_readable_props(props)
    );
}

template <
    control_code_e code,
    std::enable_if_t<code == control_code_e::connack, bool> = true
>
inline std::string to_string(uint32_t remain_length, byte_citer& it) {
    auto connack = decoders::decode_connack(remain_length, it);
    if (!connack.has_value())
        return "Cannot decode Connack packet!";
    auto& [session_present, reason_code, props] = *connack;

    return concat_strings(
        code_to_str(code),
        " session_present: ", session_present, " reason_code: ", reason_code,
        " props: ", to_readable_props(props)
    );
}

template <
    control_code_e code,
    std::enable_if_t<code == control_code_e::disconnect, bool> = true
>
inline std::string to_string(uint32_t remain_length, byte_citer& it) {
    auto disconnect = decoders::decode_disconnect(remain_length, it);
    if (!disconnect.has_value())
        return "Cannot decode Disconnect packet!";
    auto& [reason_code, props] = *disconnect;

    return concat_strings(
        code_to_str(code),
        " reason_code: ", std::to_string(uint8_t(reason_code)),
        " props: ", to_readable_props(props)
    );
}

template <
    control_code_e code,
    std::enable_if_t<code == control_code_e::publish, bool> = true
>
inline std::string to_string(
    uint8_t control_byte, uint32_t remain_length, byte_citer& it
) {
    auto publish = decoders::decode_publish(control_byte, remain_length, it);
    if (!publish.has_value())
        return "Cannot decode Publish packet!";
    auto& [topic, packet_id, flags, props, payload] = *publish;

    return concat_strings(
        code_to_str(code), (packet_id ? " " + std::to_string(*packet_id) : ""),
        " flags: ", std::bitset<8>(flags),
        " topic: ", topic, " payload: ", payload,
        " props: ", to_readable_props(props)
    );
}

template <
    control_code_e code,
    std::enable_if_t<
        code == control_code_e::puback || code == control_code_e::pubrec ||
        code == control_code_e::pubrel || code == control_code_e::pubcomp,
    bool> = true
>
inline std::string to_string(uint32_t remain_length, byte_citer& it) {
    const auto packet_id = decoders::decode_packet_id(it).value_or(0);
    remain_length -= sizeof(uint16_t);
    uint8_t reason_code = remain_length == 0 ? 0 : uint8_t(*it);
    return concat_strings(
        code_to_str(code),
        " packet_id: ", packet_id, " reason_code: ", std::to_string(reason_code)
    );
}

template <
    control_code_e code,
    std::enable_if_t<code == control_code_e::auth, bool> = true
>
inline std::string to_string(uint32_t remain_length, byte_citer& it) {
    auto auth = decoders::decode_auth(remain_length, it);
    if (!auth.has_value())
        return "Cannot decode Auth packet!";
    auto& [reason_code, props] = *auth;

    return concat_strings(
        code_to_str(code),
        " reason_code: ", std::to_string(uint8_t(reason_code)),
        " props: ", to_readable_props(props)
    );
}

template <
    control_code_e code,
    std::enable_if_t<code == control_code_e::subscribe, bool> = true
>
inline std::string to_string(uint32_t remain_length, byte_citer& it) {
    const auto packet_id = decoders::decode_packet_id(it).value_or(0);
    remain_length -= sizeof(uint16_t);
    auto subscribe = decoders::decode_subscribe(remain_length, it);
    if (!subscribe.has_value())
        return "Cannot decode Subscribe packet!";
    auto& [props, topics] = *subscribe;

    std::vector<std::string> topics_str;
    topics_str.resize(topics.size());
    boost::transform(
        boost::make_iterator_range(topics.cbegin(), topics.cend()),
        topics_str.begin(),
        [](const auto& tuple) {
            auto& [topic, options] = tuple;
            return concat_strings(topic, " ", std::bitset<8>(options));
        }
    );
    return concat_strings(
        code_to_str(code),
        " packet_id: ", packet_id,
        " topics: ", boost::algorithm::join(topics_str, ","),
        " props: ", to_readable_props(props)
    );
}

template <
    control_code_e code,
    std::enable_if_t<code == control_code_e::unsubscribe, bool> = true
>
inline std::string to_string(uint32_t remain_length, byte_citer& it) {
    const auto packet_id = decoders::decode_packet_id(it).value_or(0);
    remain_length -= sizeof(uint16_t);
    auto unsubscribe = decoders::decode_unsubscribe(remain_length, it);
    if (!unsubscribe.has_value())
        return "Cannot decode Unsubscribe packet!";
    auto& [props, topics] = *unsubscribe;

    return concat_strings(
        code_to_str(code),
        " packet_id: ", packet_id,
        " topics: ", boost::algorithm::join(topics, ","),
        " props: ", to_readable_props(props)
    );
}

inline std::string reason_codes_to_string(const std::vector<uint8_t>& rcs) {
    std::vector<std::string> rcs_str;
    rcs_str.resize(rcs.size());
    boost::transform(
        boost::make_iterator_range(rcs.cbegin(), rcs.cend()),
        rcs_str.begin(),
        [](const auto& rc) { return std::to_string(rc); }
    );
    return boost::algorithm::join(rcs_str, ",");
}

template <
    control_code_e code,
    std::enable_if_t<code == control_code_e::suback, bool> = true
>
inline std::string to_string(uint32_t remain_length, byte_citer& it) {
    const auto packet_id = decoders::decode_packet_id(it).value_or(0);
    remain_length -= sizeof(uint16_t);
    auto suback = decoders::decode_suback(remain_length, it);
    if (!suback.has_value())
        return "Cannot decode Suback packet!";
    auto& [props, reason_codes] = *suback;

    return concat_strings(
        code_to_str(code),
        " packet_id: ", packet_id,
        " reason_codes: ", reason_codes_to_string(reason_codes),
        " props: ", to_readable_props(props)
    );
}

template <
    control_code_e code,
    std::enable_if_t<code == control_code_e::unsuback, bool> = true
>
inline std::string to_string(uint32_t remain_length, byte_citer& it) {
    const auto packet_id = decoders::decode_packet_id(it).value_or(0);
    remain_length -= sizeof(uint16_t);
    auto unsuback = decoders::decode_unsuback(remain_length, it);
    if (!unsuback.has_value())
        return "Cannot decode Unuback packet!";
    auto& [props, reason_codes] = *unsuback;

    return concat_strings(
        code_to_str(code),
        " packet_id: ", packet_id,
        " reason_codes: ", reason_codes_to_string(reason_codes),
        " props: ", to_readable_props(props)
    );
}

} // end namespace detail

inline std::string to_readable_packet(std::string packet) {
    auto control_byte = uint8_t(*packet.data());
    auto code = detail::extract_code(control_byte);

    if (
        code == control_code_e::pingreq ||
        code == control_code_e::pingresp
    ) {
        return concat_strings(detail::code_to_str(code));
    }

    auto begin = ++packet.cbegin();
    auto varlen = decoders::type_parse(
        begin, packet.cend(), decoders::basic::varint_
    );

    switch (code) {
        case control_code_e::connect:
            return detail::to_string<control_code_e::connect>(*varlen, begin);
        case control_code_e::connack:
            return detail::to_string<control_code_e::connack>(*varlen, begin);
        case control_code_e::disconnect:
            return detail::to_string<control_code_e::disconnect>(*varlen, begin);
        case control_code_e::publish:
            return detail::to_string<control_code_e::publish>(
                control_byte, *varlen, begin
            );
        case control_code_e::puback:
            return detail::to_string<control_code_e::puback>(*varlen, begin);
        case control_code_e::pubrec:
            return detail::to_string<control_code_e::pubrec>(*varlen, begin);
        case control_code_e::pubrel:
            return detail::to_string<control_code_e::pubrel>(*varlen, begin);
        case control_code_e::pubcomp:
            return detail::to_string<control_code_e::pubcomp>(*varlen, begin);
        case control_code_e::auth:
            return detail::to_string<control_code_e::auth>(*varlen, begin);
        case control_code_e::subscribe:
            return detail::to_string<control_code_e::subscribe>(*varlen, begin);
        case control_code_e::suback:
            return detail::to_string<control_code_e::suback>(*varlen, begin);
        case control_code_e::unsubscribe:
            return detail::to_string<control_code_e::unsubscribe>(*varlen, begin);
        case control_code_e::unsuback:
            return detail::to_string<control_code_e::unsuback>(*varlen, begin);
        default:
            return "";
    }
}

template <typename ConstBufferSequence>
std::vector<std::string> to_readable_packets(const ConstBufferSequence& buffers) {
    namespace asio = boost::asio;

    std::vector<std::string> content;
    
    for (
        auto it = asio::buffer_sequence_begin(buffers);
        it != asio::buffer_sequence_end(buffers);
        it++
    )
        content.push_back(
            to_readable_packet(std::string { (const char*)it->data(), it->size() })
        );

    return content;
}

inline disconnect_props dprops_with_reason_string(const std::string& reason_string) {
    disconnect_props dprops;
    dprops[prop::reason_string] = reason_string;
    return dprops;
}

} // end namespace boost::mqtt5::test

#endif // BOOST_MQTT5_TEST_PACKET_UTIL_HPP
