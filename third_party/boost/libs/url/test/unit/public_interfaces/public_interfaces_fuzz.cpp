// test/unit/public_interfaces/public_interfaces_fuzz.cpp
//
// Deterministic fuzz-style tests for Boost.URL public interfaces.
//
// This file intentionally avoids libFuzzer integration and instead performs
// bounded randomized stress testing.
//
// Phases:
//  - Parse/resolve/encode/decode fuzzing with mutation
//  - Construction fuzzing
//  - Comparison fuzzing
//  - Container (segments/params) stress
//  - Authority component fuzzing
//  - Normalize/resolve fuzzing
//  - Copy/move fuzzing
//  - Encode buffer boundary abuse

#include <boost/url.hpp>
#include <boost/url/decode_view.hpp>
#include <boost/url/encode.hpp>

#include "test_suite.hpp"

#include <random>
#include <string>
#include <vector>
#include <limits>
#include <cstring>   // std::strlen
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <iostream>  // std::cerr
#include <cstddef>   // std::size_t
#include <utility>   // std::move

namespace boost {
namespace urls {

namespace {

// ============================================================
// Shared helpers
// ============================================================

static const char* const kSeeds[] = {
    "https://example.com/",
    "https://user:pass@example.com:443/path/to/file.txt?x=1&y=2#frag",
    "http://127.0.0.1:8080/a/b?c=d",
    "mailto:someone@example.com",
    "urn:uuid:123e4567-e89b-12d3-a456-426614174000",
    "/just/a/path?and=query#frag",
    "//example.com/authority-only",
    "https://example.com/%2Fencoded%20space?q=a+b",
    "ws://example.com/chat",
    "file:///C:/Windows/System32/drivers/etc/hosts"
};

std::string make_random_input(std::mt19937& rng, std::size_t max_len)
{
    static const char kAlphabet[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        "-._~"
        ":/?#[]@"
        "!$&'()*+,;="
        "% "
        "\"<>\\^`{|}\t\r\n";

    std::uniform_int_distribution<std::size_t> len_dist(0, max_len);
    std::uniform_int_distribution<std::size_t> ch_dist(0, sizeof(kAlphabet) - 2);
    std::uniform_int_distribution<int> coin(0, 999);

    std::size_t n = len_dist(rng);
    std::string s;
    s.reserve(n);

    for (std::size_t i = 0; i < n; ++i)
    {
        // Rarely inject embedded NUL
        if (coin(rng) == 0)
        {
            s.push_back('\0');
            continue;
        }

        char c = kAlphabet[ch_dist(rng)];
        s.push_back(c);

        // Occasionally inject a percent-escape (valid or invalid).
        if (c == '%' && i + 2 < n)
        {
            static const char kHex[] = "0123456789ABCDEFabcdefGg";
            std::uniform_int_distribution<std::size_t> hx_dist(0, sizeof(kHex) - 2);
            s.push_back(kHex[hx_dist(rng)]);
            s.push_back(kHex[hx_dist(rng)]);
            i += 2;
        }
    }

    return s;
}

// Most-compatible encode helper: buffer form
std::string encode_to_string(boost::core::string_view sv)
{
    auto const& cs = pchars;

    encoding_opts opt;

    std::size_t cap = sv.size() * 3 + 1; // worst-case expansion
    std::string out;
    out.resize(cap);

    std::size_t n = encode(&out[0], out.size(), sv, cs, opt);
    out.resize(n);
    return out;
}

template <class UrlViewLike>
void cheap_invariants(UrlViewLike const& u)
{
    auto b = u.buffer();
    BOOST_TEST(b.data() != nullptr || b.size() == 0);
    (void)u.size();
}

// Exercise iterator-heavy views (good at surfacing internal offset bugs).
void exercise_views(url const& u)
{
    try
    {
        auto p = u.params();
        std::size_t n = 0;
        for (auto it = p.begin(); it != p.end() && n < 16; ++it, ++n)
            (void)*it;
    }
    catch (...) {}

    try
    {
        auto s = u.segments();
        std::size_t n = 0;
        for (auto it = s.begin(); it != s.end() && n < 16; ++it, ++n)
            (void)*it;
    }
    catch (...) {}
}

// Aggressive mutation: 41 distinct operations covering setters, encoded
// setters, params, segments, remove, clear, swap, normalize, assignment.
void mutate_url(url& u, std::mt19937& rng)
{
    std::uniform_int_distribution<int> op_dist(0, 40);

    auto rand_text = [&]() -> std::string {
        static const char* texts[] = {
            "", "a", "x", "test", "hello world",
            "a+b", "a/b", "a%20b", "a%", "a%GG"
        };
        std::uniform_int_distribution<std::size_t> d(0, 9);
        return texts[d(rng)];
    };

    int op = op_dist(rng);

    try {
        switch (op) {
        case 0: u.set_scheme("http"); break;
        case 1: u.set_scheme("https"); break;
        case 2: u.set_user("user"); break;
        case 3: u.set_password("pass"); break;
        case 4: u.set_host("example.com"); break;
        case 5: u.set_host("127.0.0.1"); break;
        case 6: u.set_port("80"); break;
        case 7: u.set_port(""); break;
        case 8: u.set_path("/"); break;
        case 9: u.set_path(std::string("/") + rand_text() + "/" + rand_text()); break;
        case 10: u.set_query(std::string("k=") + rand_text() + "&x=" + rand_text()); break;
        case 11: u.set_fragment(rand_text()); break;
        case 12: {
            auto p = u.params();
            std::string val = rand_text();
            p.append(param_view{"k", val});
            break;
        }
        case 13: {
            auto p = u.params();
            auto it = p.find("k");
            if (it != p.end())
                p.erase(it);
            std::string val = rand_text();
            p.append(param_view{"k", val});
            break;
        }
        case 14: {
            auto s = u.segments();
            s.push_back(rand_text());
            break;
        }
        case 15: {
            auto s = u.segments();
            if (!s.empty())
                s.pop_back();
            break;
        }
        case 16: u.set_scheme_id(static_cast<scheme>(rng() % 10)); break;
        case 17: u.set_userinfo(rand_text()); break;
        case 18: u.set_encoded_user(rand_text()); break;
        case 19: u.set_encoded_password(rand_text()); break;
        case 20: u.set_encoded_host(rand_text()); break;
        case 21: u.set_port_number(rng() % 65536); break;
        case 22: u.set_encoded_path(rand_text()); break;
        case 23: u.set_encoded_query(rand_text()); break;
        case 24: u.set_encoded_fragment(rand_text()); break;
        case 25: u.remove_authority(); break;
        case 26: u.remove_userinfo(); break;
        case 27: u.remove_port(); break;
        case 28: u.remove_query(); break;
        case 29: u.remove_fragment(); break;
        case 30: {
            auto p = u.params();
            if (!p.empty()) {
                std::uniform_int_distribution<std::size_t> idx(0, p.size() - 1);
                auto it = p.begin();
                std::advance(it, idx(rng));
                p.erase(it);
            }
            break;
        }
        case 31: {
            auto p = u.params();
            p.clear();
            break;
        }
        case 32: {
            auto p = u.params();
            if (!p.empty()) {
                std::uniform_int_distribution<std::size_t> idx(0, p.size() - 1);
                auto it = p.begin();
                std::advance(it, idx(rng));
                std::string key = rand_text();
                std::string val = rand_text();
                p.replace(it, param_view{key, val});
            }
            break;
        }
        case 33: {
            auto s = u.segments();
            if (!s.empty()) {
                std::uniform_int_distribution<std::size_t> idx(0, s.size() - 1);
                auto it = s.begin();
                std::advance(it, idx(rng));
                s.erase(it);
            }
            break;
        }
        case 34: {
            auto s = u.segments();
            s.clear();
            break;
        }
        case 35: {
            auto s = u.segments();
            if (!s.empty()) {
                std::uniform_int_distribution<std::size_t> idx(0, s.size() - 1);
                auto it = s.begin();
                std::advance(it, idx(rng));
                s.insert(it, rand_text());
            }
            break;
        }
        case 36: u.clear(); break;
        case 37: u.reserve(rng() % 1000); break;
        case 38: {
            url u2("http://other.com/path");
            u = u2;
            break;
        }
        case 39: {
            url u2;
            std::swap(u, u2);
            break;
        }
        case 40: {
            try { u.normalize(); } catch(...) {}
            break;
        }
        default: break;
        }
    } catch (...) {
        // Expected under mutation fuzz
    }
}

// Abuse encode buffer sizes (bounds checking).
void encode_bounds_abuse(boost::core::string_view sv)
{
    auto const& cs = pchars;
    encoding_opts opt;

    // encoded_size: just compute the size, no output buffer needed
    (void)encode(nullptr, 0, {}, cs, opt);

    // small buffers: encode truncates to fit
    char buf1[1] = {0};
    char buf8[8] = {0};
    char buf32[32] = {0};

    (void)encode(buf1, sizeof(buf1), sv, cs, opt);
    (void)encode(buf8, sizeof(buf8), sv, cs, opt);
    (void)encode(buf32, sizeof(buf32), sv, cs, opt);

    (void)buf1[0];
}

// ============================================================
// Fuzz phases
// ============================================================

void fuzz_parse_resolve_encode(std::mt19937& rng, std::size_t iterations)
{
    // Seed corpus first
    for (auto* seed : kSeeds)
    {
        boost::core::string_view sv(seed, std::strlen(seed));

        auto r1 = parse_uri(sv);
        if (r1) cheap_invariants(*r1);

        auto r2 = parse_uri_reference(sv);
        if (r2) cheap_invariants(*r2);

        auto r3 = parse_relative_ref(sv);
        if (r3) cheap_invariants(*r3);

        auto r4 = parse_origin_form(sv);
        if (r4) cheap_invariants(*r4);

        (void)parse_query(sv);
        (void)parse_path(sv);

        encode_bounds_abuse(sv);
    }

    // Random fuzz-style loop
    for (std::size_t i = 0; i < iterations; ++i)
    {
        std::string s = make_random_input(rng, 768);
        boost::core::string_view sv(s.data(), s.size());

        auto ru = parse_uri(sv);
        if (ru) cheap_invariants(*ru);

        auto rur = parse_uri_reference(sv);
        if (rur) cheap_invariants(*rur);

        auto rrel = parse_relative_ref(sv);
        if (rrel) cheap_invariants(*rrel);

        auto ro = parse_origin_form(sv);
        if (ro) cheap_invariants(*ro);

        auto rq = parse_query(sv);
        if (rq)
        {
            std::size_t count = 0;
            for (auto it = rq->begin(); it != rq->end() && count < 32; ++it, ++count)
                (void)*it;
        }

        auto rp = parse_path(sv);
        if (rp)
        {
            std::size_t count = 0;
            for (auto it = rp->begin(); it != rp->end() && count < 32; ++it, ++count)
                (void)*it;
        }

        // resolve(base, ref, dest) + mutation fuzz on resulting url
        {
            auto base_r = parse_uri("https://example.com/base/path?x=1");
            BOOST_TEST(base_r.has_value());

            auto ref_r = parse_uri_reference(sv);
            if (ref_r)
            {
                url dest;
                auto rr = resolve(*base_r, *ref_r, dest);
                if (rr)
                {
                    auto b = dest.buffer();
                    BOOST_TEST(b.data() != nullptr || b.size() == 0);

                    exercise_views(dest);

                    std::uniform_int_distribution<int> steps_dist(5, 40);
                    int steps = steps_dist(rng);
                    for (int k = 0; k < steps; ++k)
                    {
                        mutate_url(dest, rng);
                        exercise_views(dest);

                        auto enc = encode_to_string(dest.buffer());
                        decode_view dv(enc);

                        (void)dv.size();
                        if (!dv.empty())
                        {
                            (void)dv.front();
                            (void)dv.back();
                        }

                        std::string dec;
                        dec.reserve(dv.size());
                        for (char ch : dv)
                            dec.push_back(ch);

                        BOOST_TEST(dec.size() <= enc.size());

                        encode_bounds_abuse(dest.buffer());
                    }
                }
            }
        }

        // Encode/decode stress on raw input
        {
            std::string enc = encode_to_string(sv);
            decode_view dv(enc);

            (void)dv.size();
            if (!dv.empty())
            {
                (void)dv.front();
                (void)dv.back();
            }

            std::string dec;
            dec.reserve(dv.size());
            for (char ch : dv)
                dec.push_back(ch);

            BOOST_TEST(dec.size() <= enc.size());
        }

        encode_bounds_abuse(sv);
    }
}

void fuzz_construction(std::mt19937& rng, std::size_t iterations)
{
    for (std::size_t i = 0; i < iterations; ++i) {
        std::string input = make_random_input(rng, 512);
        boost::core::string_view sv(input.data(), input.size());

        try {
            url u1(sv);
            cheap_invariants(u1);
        } catch (...) {}

        auto r = parse_uri_reference(sv);
        if (r) {
            try {
                url u2(*r);
                cheap_invariants(u2);
                BOOST_TEST(u2 == *r);
            } catch (...) {}
        }
    }
}

void fuzz_comparison(std::mt19937& rng, std::size_t iterations)
{
    for (std::size_t i = 0; i < iterations; ++i) {
        std::string s1 = make_random_input(rng, 256);
        std::string s2 = make_random_input(rng, 256);

        auto r1 = parse_uri_reference(s1);
        auto r2 = parse_uri_reference(s2);

        if (r1 && r2) {
            try {
                url u1(*r1);
                url u2(*r2);

                bool eq = (u1 == u2);
                bool ne = (u1 != u2);
                BOOST_TEST(eq != ne);

                bool lt = (u1 < u2);
                bool gt = (u2 < u1);
                if (eq) {
                    BOOST_TEST(!lt && !gt);
                }

                BOOST_TEST(u1 == u1);
                BOOST_TEST(!(u1 != u1));
                BOOST_TEST(!(u1 < u1));

            } catch (...) {}
        }
    }
}

void fuzz_containers(std::mt19937& rng, std::size_t iterations)
{
    for (std::size_t i = 0; i < iterations; ++i) {
        url u("http://example.com/");

        // Stress segments
        {
            auto segs = u.segments();
            std::uniform_int_distribution<int> op_dist(0, 5);
            std::uniform_int_distribution<int> count_dist(1, 20);

            int ops = count_dist(rng);
            for (int j = 0; j < ops; ++j) {
                try {
                    int op = op_dist(rng);
                    auto rand_seg = make_random_input(rng, 32);

                    switch (op) {
                    case 0: segs.push_back(rand_seg); break;
                    case 1: if (!segs.empty()) segs.pop_back(); break;
                    case 2:
                        if (!segs.empty()) {
                            std::uniform_int_distribution<std::size_t> idx(0, segs.size() - 1);
                            auto it = segs.begin();
                            std::advance(it, idx(rng));
                            segs.insert(it, rand_seg);
                        }
                        break;
                    case 3:
                        if (!segs.empty()) {
                            std::uniform_int_distribution<std::size_t> idx(0, segs.size() - 1);
                            auto it = segs.begin();
                            std::advance(it, idx(rng));
                            segs.erase(it);
                        }
                        break;
                    case 4:
                        if (!segs.empty()) {
                            std::uniform_int_distribution<std::size_t> idx(0, segs.size() - 1);
                            auto it = segs.begin();
                            std::advance(it, idx(rng));
                            segs.replace(it, rand_seg);
                        }
                        break;
                    case 5: segs.clear(); break;
                    }

                    cheap_invariants(u);

                } catch (...) {}
            }
        }

        // Stress params
        {
            auto params = u.params();
            std::uniform_int_distribution<int> op_dist(0, 4);
            std::uniform_int_distribution<int> count_dist(1, 20);

            int ops = count_dist(rng);
            for (int j = 0; j < ops; ++j) {
                try {
                    int op = op_dist(rng);
                    auto rand_key = make_random_input(rng, 16);
                    auto rand_val = make_random_input(rng, 32);

                    switch (op) {
                    case 0: params.append(param_view{rand_key, rand_val}); break;
                    case 1:
                        if (!params.empty()) {
                            std::uniform_int_distribution<std::size_t> idx(0, params.size() - 1);
                            auto it = params.begin();
                            std::advance(it, idx(rng));
                            params.erase(it);
                        }
                        break;
                    case 2:
                        if (!params.empty()) {
                            std::uniform_int_distribution<std::size_t> idx(0, params.size() - 1);
                            auto it = params.begin();
                            std::advance(it, idx(rng));
                            params.insert(it, param_view{rand_key, rand_val});
                        }
                        break;
                    case 3:
                        if (!params.empty()) {
                            std::uniform_int_distribution<std::size_t> idx(0, params.size() - 1);
                            auto it = params.begin();
                            std::advance(it, idx(rng));
                            params.replace(it, param_view{rand_key, rand_val});
                        }
                        break;
                    case 4: params.clear(); break;
                    }

                    cheap_invariants(u);

                } catch (...) {}
            }
        }
    }
}

void fuzz_authority_components(std::mt19937& rng, std::size_t iterations)
{
    for (std::size_t i = 0; i < iterations; ++i) {
        url u("http://example.com/");

        std::uniform_int_distribution<int> op_dist(0, 10);

        try {
            switch (op_dist(rng)) {
            case 0: {
                std::uniform_int_distribution<int> octet(0, 255);
                std::string ipv4 = std::to_string(octet(rng)) + "." +
                                   std::to_string(octet(rng)) + "." +
                                   std::to_string(octet(rng)) + "." +
                                   std::to_string(octet(rng));
                u.set_host(ipv4);
                if (u.host_type() == host_type::ipv4) {
                    cheap_invariants(u);
                }
                break;
            }
            case 1: {
                std::string ipv6 = "[";
                std::uniform_int_distribution<int> hex_dist(0, 0xFFFF);
                for (int j = 0; j < 8; ++j) {
                    if (j > 0) ipv6 += ":";
                    std::stringstream ss;
                    ss << std::hex << hex_dist(rng);
                    ipv6 += ss.str();
                }
                ipv6 += "]";
                u.set_host(ipv6);
                if (u.host_type() == host_type::ipv6) {
                    cheap_invariants(u);
                }
                break;
            }
            case 2: {
                auto domain = make_random_input(rng, 64);
                u.set_host(domain);
                break;
            }
            case 3: {
                std::uniform_int_distribution<std::uint16_t> port_dist(1, 65535);
                u.set_port_number(port_dist(rng));
                break;
            }
            case 4: {
                auto user = make_random_input(rng, 32);
                u.set_user(user);
                break;
            }
            case 5: {
                auto pass = make_random_input(rng, 32);
                u.set_password(pass);
                break;
            }
            case 6: {
                auto userinfo = make_random_input(rng, 64);
                u.set_userinfo(userinfo);
                break;
            }
            case 7: u.remove_authority(); break;
            case 8: u.remove_userinfo(); break;
            case 9: u.remove_port(); break;
            case 10: {
                if (u.has_authority()) {
                    auto auth = u.authority();
                    cheap_invariants(auth);

                    if (u.host_type() == host_type::ipv4) {
                        auto ipv4 = u.host_ipv4_address();
                        (void)ipv4.to_string();
                    } else if (u.host_type() == host_type::ipv6) {
                        auto ipv6 = u.host_ipv6_address();
                        (void)ipv6.to_string();
                    }
                }
                break;
            }
            }

            cheap_invariants(u);

        } catch (...) {}
    }
}

void fuzz_mutation(std::mt19937& rng, std::size_t iterations)
{
    for (std::size_t i = 0; i < iterations; ++i) {
        url u("https://example.com/path/to/resource?x=1&y=2#section");

        std::uniform_int_distribution<int> steps_dist(5, 30);
        int steps = steps_dist(rng);

        for (int j = 0; j < steps; ++j) {
            mutate_url(u, rng);
            cheap_invariants(u);
            exercise_views(u);
        }
    }
}

void fuzz_normalize_resolve(std::mt19937& rng, std::size_t iterations)
{
    static const char* bases[] = {
        "http://example.com/",
        "https://example.com/a/b/c",
        "ftp://ftp.example.org/dir/",
        "http://192.168.1.1:8080/api/",
    };

    for (std::size_t i = 0; i < iterations; ++i) {
        std::uniform_int_distribution<std::size_t> base_dist(0, 3);
        auto base_r = parse_uri(bases[base_dist(rng)]);

        if (!base_r) continue;

        std::string ref_str = make_random_input(rng, 256);
        auto ref_r = parse_uri_reference(ref_str);

        if (!ref_r) continue;

        url dest;
        auto res = resolve(*base_r, *ref_r, dest);

        if (res) {
            cheap_invariants(dest);
            exercise_views(dest);

            std::uniform_int_distribution<int> steps_dist(5, 20);
            int steps = steps_dist(rng);
            for (int k = 0; k < steps; ++k) {
                mutate_url(dest, rng);
                cheap_invariants(dest);
                exercise_views(dest);
            }

            try {
                dest.normalize();
                cheap_invariants(dest);
            } catch (...) {}
        }
    }
}

void fuzz_copy_move(std::mt19937& rng, std::size_t iterations)
{
    for (std::size_t i = 0; i < iterations; ++i) {
        std::string s = make_random_input(rng, 256);
        auto r = parse_uri_reference(s);

        if (!r) continue;

        try {
            url u1(*r);

            url u2(u1);
            BOOST_TEST(u1 == u2);
            cheap_invariants(u2);

            url u3;
            u3 = u1;
            BOOST_TEST(u1 == u3);
            cheap_invariants(u3);

            url u4(std::move(u1));
            cheap_invariants(u4);

            url u5;
            u5 = std::move(u2);
            cheap_invariants(u5);

            url& ref = u5;
            u5 = ref;
            cheap_invariants(u5);

        } catch (...) {}
    }
}

} // namespace

struct public_interfaces_fuzz_test
{
    void run()
    {
        std::mt19937 rng(0xC0FFEEu);

        fuzz_parse_resolve_encode(rng, 3000);
        fuzz_construction(rng, 500);
        fuzz_comparison(rng, 500);
        fuzz_containers(rng, 200);
        fuzz_authority_components(rng, 500);
        fuzz_mutation(rng, 300);
        fuzz_normalize_resolve(rng, 300);
        fuzz_copy_move(rng, 500);
    }
};

TEST_SUITE(public_interfaces_fuzz_test, "boost.url.public_interfaces_fuzz");

} // urls
} // boost
