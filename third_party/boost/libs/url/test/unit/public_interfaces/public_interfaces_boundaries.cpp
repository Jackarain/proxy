// test/unit/public_interfaces/public_interfaces_boundaries.cpp
//
// Boundary & edge cases for Boost.URL public interfaces.
// Covers: parse function boundaries, resolve, encode/decode roundtrip,
// construction, modification setters, segments, params, comparison, authority.

#include <boost/url.hpp>
#include <boost/url/decode_view.hpp>
#include <boost/url/encode.hpp>

#include "test_suite.hpp"

#include <string>
#include <cstring>      // std::strlen
#include <cstddef>      // std::size_t
#include <iostream>     // std::cerr
#include <utility>      // std::move

namespace boost {
namespace urls {

namespace {

// ============================================================
// Helpers
// ============================================================

struct Case
{
    const char* s;
    bool uri;
    bool uri_ref;
    bool rel_ref;
    bool origin;
};

static const Case kCases[] = {
    // empty / minimal
    {"",          false, true,  true,  false},
    {"#",         false, true,  true,  false},
    {"?",         false, true,  true,  false},
    {"/",         false, true,  true,  true},

    // scheme-ish (calibrated to Boost.URL 1.90 behavior)
    {"http:",     true,  true,  false, false},
    {"http://",   true,  true,  false, false},
    {"mailto:a@b",true,  true,  false, false},

    // authority-ish (calibrated: parse_origin_form accepts these)
    {"//example.com",            false, true,  true,  true},
    {"//user:pass@example.com",  false, true,  true,  true},
    {"//[::1]/",                 false, true,  true,  false},

    // typical full URLs
    {"https://example.com/",                     true,  true,  false, false},
    {"https://example.com/a/b?x=1&y=2#frag",     true,  true,  false, false},
    {"https://user:pass@example.com:443/p?q#f",  true,  true,  false, false},

    // origin-form (HTTP request target)
    {"/path/to/resource?x=1",     false, true,  true,  true},
    {"*",                         false, true,  true,  false}, // not accepted by parse_origin_form in this version
    {"/%2f%2F",                   false, true,  true,  true},

    // percent escapes: valid + invalid (calibrated: invalid % rejected by uri_reference/relative/origin)
    {"https://example.com/%2F",   true,  true,  false, false},
    {"https://example.com/%",     false, false, false, false},
    {"https://example.com/%GG",   false, false, false, false},
    {"/path?x=%",                 false, false, false, false},

    // spaces and controls (calibrated: spaces rejected by uri_reference)
    {"https://example.com/has space", false, false,false, false},
    {"\r\n",                           false, false,false, false},

    // weird but common in real systems
    {"https://example.com/a+b?q=hello+world", true, true, false, false},
};

// Heuristic: does input *likely* contain an invalid pct-encoding sequence?
// We use this only to decide whether decode_view(sv) should throw.
// It's conservative: if we're not sure, we won't require a throw.
bool looks_like_bad_pct(const char* s)
{
    for (std::size_t i = 0; s[i] != '\0'; ++i)
    {
        if (s[i] != '%') continue;

        // '%' at end or too short -> bad
        if (s[i + 1] == '\0' || s[i + 2] == '\0')
            return true;

        auto is_hex = [](unsigned char c) {
            return (c >= '0' && c <= '9') ||
                   (c >= 'A' && c <= 'F') ||
                   (c >= 'a' && c <= 'f');
        };

        if (!is_hex(static_cast<unsigned char>(s[i + 1])) ||
            !is_hex(static_cast<unsigned char>(s[i + 2])))
            return true;

        i += 2; // skip the two hex chars
    }
    return false;
}

// Encode helper using stable overload: encode(char*, cap, sv, charset, opts).
std::string encode_to_string(boost::core::string_view sv)
{
    auto const& cs = pchars;

    encoding_opts opt; // default

    std::size_t cap = sv.size() * 3 + 1; // worst-case expansion
    std::string out;
    out.resize(cap);

    std::size_t n = encode(&out[0], out.size(), sv, cs, opt);
    out.resize(n);
    return out;
}

// Single-argument BOOST_TEST harness; provide diagnostics separately.
template <class Result>
void expect_ok(Result const& r, bool should_succeed, const char* which, const char* s)
{
    bool actual = r.has_value();
    if (actual != should_succeed)
    {
        std::cerr << which << " expectation mismatch for: [" << s
                  << "] expected=" << (should_succeed ? "success" : "failure")
                  << " actual=" << (actual ? "success" : "failure") << "\n";
    }

    BOOST_TEST(actual == should_succeed);

    if (r)
    {
        // Basic invariants that should never crash.
        auto b = r->buffer();
        BOOST_TEST(b.data() != nullptr || b.size() == 0);
    }
}

// Helper: expect operation to succeed
template <class Fn>
void expect_success(const char* desc, Fn fn)
{
    try
    {
        fn();
    }
    catch (std::exception const& e)
    {
        std::cerr << desc << " failed unexpectedly: " << e.what() << "\n";
        BOOST_TEST(false);
    }
}

// Helper: expect operation to throw
template <class Fn>
void expect_throw(const char* desc, Fn fn)
{
    bool threw = false;
    try
    {
        fn();
    }
    catch (boost::system::system_error const&)
    {
        threw = true;
    }
    catch (std::exception const& e)
    {
        std::cerr << desc << " threw unexpected exception type: " << e.what() << "\n";
    }

    if (!threw)
    {
        std::cerr << desc << " did not throw as expected\n";
    }
    BOOST_TEST(threw);
}

} // namespace

struct public_interfaces_boundaries_test
{
    void test_parse_boundaries()
    {
        for (auto const& c : kCases)
        {
            boost::core::string_view sv(c.s, std::strlen(c.s));

            expect_ok(parse_uri(sv),           c.uri,     "parse_uri",           c.s);
            expect_ok(parse_uri_reference(sv), c.uri_ref, "parse_uri_reference", c.s);
            expect_ok(parse_relative_ref(sv),  c.rel_ref, "parse_relative_ref",  c.s);
            expect_ok(parse_origin_form(sv),   c.origin,  "parse_origin_form",   c.s);

            // parse_query / parse_path: robustness checks (no crash).
            {
                auto rq = parse_query(sv);
                if (rq)
                {
                    std::size_t n = 0;
                    for (auto const& p : *rq)
                    {
                        (void)p;
                        if (++n > 64) break;
                    }
                }
            }
            {
                auto rp = parse_path(sv);
                if (rp)
                {
                    std::size_t n = 0;
                    for (auto const& seg : *rp)
                    {
                        (void)seg;
                        if (++n > 64) break;
                    }
                }
            }

            // resolve(base, ref, dest)
            {
                auto base_r = parse_uri("https://example.com/base/");
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
                    }
                }
            }

            // Percent encoding/decoding boundary checks.
            //
            // 1) Known-valid pct string produced by encode_to_string must not throw in decode_view.
            {
                std::string enc = encode_to_string(sv);

                bool threw = false;
                try
                {
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
                catch (boost::system::system_error const& e)
                {
                    threw = true;
                    std::cerr << "decode_view threw unexpectedly on encoded input for: [" << c.s
                              << "] message=" << e.what() << "\n";
                }
                BOOST_TEST(!threw);
            }

            // 2) Direct decode_view on raw input:
            //    - If it *looks* like it has invalid percent-encoding, we expect a throw.
            //    - Otherwise, it should not throw.
            {
                bool expect_throw = looks_like_bad_pct(c.s);

                bool threw = false;
                try
                {
                    decode_view dv_bad(sv);
                    (void)dv_bad.size();
                    if (!dv_bad.empty())
                        (void)dv_bad.front();
                }
                catch (boost::system::system_error const&)
                {
                    threw = true;
                }

                BOOST_TEST(threw == expect_throw);
            }
        }
    }

    void test_url_construction()
    {
        // Default construction
        {
            url u;
            BOOST_TEST(u.empty());
            BOOST_TEST(u.buffer() == "");
            BOOST_TEST(u.size() == 0);
        }

        // String construction - valid
        {
            expect_success("construct full URL", [&] {
                url u1("https://example.com/path?q=1#frag");
                BOOST_TEST(u1.scheme() == "https");
                BOOST_TEST(u1.host() == "example.com");
            });

            expect_success("construct relative path", [&] {
                url u2("/relative/path");
                BOOST_TEST(!u2.has_scheme());
                BOOST_TEST(u2.path() == "/relative/path");
            });

            expect_success("construct query only", [&] {
                url u3("?query=only");
                BOOST_TEST(u3.has_query());
            });

            expect_success("construct fragment only", [&] {
                url u4("#fragment-only");
                BOOST_TEST(u4.has_fragment());
            });
        }

        // String construction - invalid (should throw)
        {
            expect_throw("construct with space in scheme",
                [&] { url u("ht tp://example.com"); });
            expect_throw("construct with space in host",
                [&] { url u("http://exam ple.com"); });
            expect_throw("construct with invalid pct-encoding",
                [&] { url u("http://example.com/%"); });
        }

        // Copy construction
        {
            url u1("https://example.com/path");
            url u2(u1);
            BOOST_TEST(u1 == u2);
            BOOST_TEST(u1.buffer() == u2.buffer());
            BOOST_TEST(u1.buffer().data() != u2.buffer().data()); // Different buffers
        }

        // Move construction
        {
            url u1("https://example.com/path");
            url u2(std::move(u1));
            BOOST_TEST(u2.buffer() == "https://example.com/path");
        }

        // Assignment
        {
            url u1("https://example.com/");
            url u2("http://other.org/");
            u1 = u2;
            BOOST_TEST(u1 == u2);
            BOOST_TEST(u1.buffer() == "http://other.org/");
        }

        // Self-assignment (via reference to avoid -Wself-assign-overloaded)
        {
            url u("https://example.com/");
            url& ref = u;
            u = ref;
            BOOST_TEST(u.buffer() == "https://example.com/");
        }
    }

    void test_url_modification_setters()
    {
        // set_scheme - valid cases
        {
            url u("http://example.com/path");
            u.set_scheme("https");
            BOOST_TEST(u.buffer() == "https://example.com/path");

            u.set_scheme("ws");
            BOOST_TEST(u.scheme() == "ws");

            u.set_scheme("ftp");
            BOOST_TEST(u.scheme() == "ftp");
        }

        // set_scheme - invalid cases (should throw)
        {
            url u("http://example.com/");
            expect_throw("set_scheme empty", [&] { u.set_scheme(""); });
            expect_throw("set_scheme with space", [&] { u.set_scheme("ht tp"); });
            expect_throw("set_scheme starting with digit", [&] { u.set_scheme("123abc"); });
            expect_throw("set_scheme with colon", [&] { u.set_scheme("http://"); });
        }

        // set_scheme_id
        {
            url u("http://example.com/");
            u.set_scheme_id(scheme::https);
            BOOST_TEST(u.scheme_id() == scheme::https);
            BOOST_TEST(u.scheme() == "https");
        }

        // set_user - valid cases
        {
            url u("http://example.com/");
            u.set_user("alice");
            BOOST_TEST(u.user() == "alice");
            BOOST_TEST(u.encoded_user() == "alice");

            u.set_user("bob smith"); // Should encode space
            BOOST_TEST(u.user() == "bob smith");
            BOOST_TEST(u.encoded_user() == "bob%20smith");

            u.set_user(""); // Empty user
            BOOST_TEST(u.user() == "");
        }

        // set_user - special characters
        {
            url u("http://example.com/");
            u.set_user("user@domain");
            BOOST_TEST(u.user() == "user@domain");
            BOOST_TEST(u.encoded_user().find("%40") != core::string_view::npos);
        }

        // set_password - valid cases
        {
            url u("http://user@example.com/");
            u.set_password("secret");
            BOOST_TEST(u.password() == "secret");

            u.set_password("pa$$word"); // Special chars
            BOOST_TEST(u.password() == "pa$$word");

            u.set_password(""); // Empty password
            BOOST_TEST(u.password() == "");
        }

        // set_userinfo - combined
        {
            url u("http://example.com/");
            u.set_userinfo("alice:secret");
            BOOST_TEST(u.user() == "alice");
            BOOST_TEST(u.password() == "secret");

            u.set_userinfo("bob"); // No password
            BOOST_TEST(u.user() == "bob");
            BOOST_TEST(!u.has_password());
        }

        // set_host - valid cases
        {
            url u("http://example.com/");

            // Domain name
            u.set_host("example.org");
            BOOST_TEST(u.host() == "example.org");

            // IPv4 address
            u.set_host("127.0.0.1");
            BOOST_TEST(u.host() == "127.0.0.1");
            BOOST_TEST(u.host_type() == host_type::ipv4);

            // IPv6 address
            u.set_host("[::1]");
            BOOST_TEST(u.host() == "[::1]");
            BOOST_TEST(u.host_type() == host_type::ipv6);
        }

        // set_port - valid cases
        {
            url u("http://example.com/");

            u.set_port("8080");
            BOOST_TEST(u.port() == "8080");
            BOOST_TEST(u.port_number() == 8080);

            u.set_port("80");
            BOOST_TEST(u.port() == "80");
        }

        // set_port - invalid cases
        {
            url u("http://example.com/");
            // "99999" is valid per RFC 3986 (port = *DIGIT), no range check
            expect_throw("set_port non-numeric", [&] { u.set_port("abc"); });
            expect_throw("set_port negative", [&] { u.set_port("-1"); });
        }

        // set_port_number
        {
            url u("http://example.com/");
            u.set_port_number(8080);
            BOOST_TEST(u.port_number() == 8080);

            u.set_port_number(443);
            BOOST_TEST(u.port_number() == 443);
        }

        // set_path - valid cases
        {
            url u("http://example.com/");

            u.set_path("/new/path");
            BOOST_TEST(u.path() == "/new/path");

            u.set_path("/path with spaces");
            BOOST_TEST(u.path() == "/path with spaces");
            BOOST_TEST(u.encoded_path().find("%20") != core::string_view::npos);

            u.set_path(""); // Empty path
            BOOST_TEST(u.path() == "");
        }

        // set_query - valid cases
        {
            url u("http://example.com/path");

            u.set_query("a=1&b=2");
            BOOST_TEST(u.query() == "a=1&b=2");

            u.set_query("x=hello world");
            BOOST_TEST(u.query() == "x=hello world");

            u.set_query(""); // Empty query
            BOOST_TEST(u.query() == "");
        }

        // set_fragment - valid cases
        {
            url u("http://example.com/path");

            u.set_fragment("section1");
            BOOST_TEST(u.fragment() == "section1");

            u.set_fragment("section with spaces");
            BOOST_TEST(u.fragment() == "section with spaces");

            u.set_fragment(""); // Empty fragment
            BOOST_TEST(u.fragment() == "");
        }

        // Chained modifications
        {
            url u("http://example.com/");
            u.set_scheme("https")
             .set_host("secure.example.org")
             .set_port("8443")
             .set_path("/api/v1/users")
             .set_query("limit=10")
             .set_fragment("results");

            BOOST_TEST(u.scheme() == "https");
            BOOST_TEST(u.host() == "secure.example.org");
            BOOST_TEST(u.port() == "8443");
            BOOST_TEST(u.path() == "/api/v1/users");
            BOOST_TEST(u.has_query());
            BOOST_TEST(u.has_fragment());
        }

        // Removal operations
        {
            url u("https://user:pass@example.com:8080/path?q=1#frag");

            u.remove_authority();
            BOOST_TEST(!u.has_authority());

            u = url("https://user:pass@example.com:8080/path?q=1#frag");
            u.remove_userinfo();
            BOOST_TEST(!u.has_userinfo());
            BOOST_TEST(u.has_authority()); // Still has host

            u.remove_port();
            BOOST_TEST(!u.has_port());

            u.remove_query();
            BOOST_TEST(!u.has_query());

            u.remove_fragment();
            BOOST_TEST(!u.has_fragment());
        }
    }

    void test_segments_operations()
    {
        // Basic push_back/pop_back
        {
            url u("http://example.com/");
            auto segs = u.segments();

            segs.push_back("api");
            BOOST_TEST(u.path() == "/api");

            segs.push_back("v1");
            BOOST_TEST(u.path() == "/api/v1");

            segs.push_back("users");
            BOOST_TEST(u.path() == "/api/v1/users");

            segs.pop_back();
            BOOST_TEST(u.path() == "/api/v1");
        }

        // Segments with encoding
        {
            url u("http://example.com/");
            auto segs = u.segments();

            segs.push_back("path with spaces");
            BOOST_TEST(u.encoded_path().find("%20") != core::string_view::npos);

            segs.push_back("a/b"); // Slash in segment
            BOOST_TEST(u.encoded_path().find("%2F") != core::string_view::npos ||
                      u.encoded_path().find("%2f") != core::string_view::npos);
        }

        // Insert operations
        {
            url u("http://example.com/a/c");
            auto segs = u.segments();

            auto it = segs.begin();
            ++it; // Move to position after 'a'
            segs.insert(it, "b");
            BOOST_TEST(u.path() == "/a/b/c");

            segs.insert(segs.begin(), "start");
            BOOST_TEST(u.path() == "/start/a/b/c");

            segs.insert(segs.end(), "end");
            BOOST_TEST(u.path() == "/start/a/b/c/end");
        }

        // Erase operations
        {
            url u("http://example.com/a/b/c/d");
            auto segs = u.segments();

            auto it = segs.begin();
            ++it; // Point to 'b'
            segs.erase(it); // Remove 'b'
            BOOST_TEST(u.path() == "/a/c/d");

            // Erase range
            auto it2 = segs.begin();
            auto it3 = segs.begin();
            ++it3;
            ++it3;
            segs.erase(it2, it3); // Remove first 2 elements
            BOOST_TEST(u.path() == "/d");
        }

        // Replace operations
        {
            url u("http://example.com/old/path/here");
            auto segs = u.segments();

            segs.replace(segs.begin(), "new");
            BOOST_TEST(u.path() == "/new/path/here");

            auto it = segs.begin();
            ++it; // Point to second segment
            segs.replace(it, "route");
            BOOST_TEST(u.path() == "/new/route/here");
        }

        // Clear
        {
            url u("http://example.com/a/b/c");
            auto segs = u.segments();

            segs.clear();
            BOOST_TEST(u.path() == "");
            BOOST_TEST(segs.empty());
        }

        // Empty segments
        {
            url u("http://example.com/");
            auto segs = u.segments();

            segs.push_back("");
            segs.push_back("a");
            segs.push_back("");
            BOOST_TEST(segs.size() == 3);
        }

        // Large number of segments (stress test)
        {
            url u("http://example.com/");
            auto segs = u.segments();

            for (int i = 0; i < 100; ++i) {
                segs.push_back(std::to_string(i));
            }

            BOOST_TEST(segs.size() == 100);
            BOOST_TEST(segs.front() == "0");
            BOOST_TEST(segs.back() == "99");
        }
    }

    void test_params_operations()
    {
        // Basic append
        {
            url u("http://example.com/path");
            auto params = u.params();

            params.append(param_view{"a", "1"});
            BOOST_TEST(u.query() == "a=1");

            params.append(param_view{"b", "2"});
            BOOST_TEST(u.query() == "a=1&b=2");

            params.append(param_view{"c", "3"});
            BOOST_TEST(u.query() == "a=1&b=2&c=3");
        }

        // Params with encoding
        {
            url u("http://example.com/");
            auto params = u.params();

            params.append(param_view{"key", "value with spaces"});
            // Spaces in query params are encoded as '+' (application/x-www-form-urlencoded)
            BOOST_TEST(
                u.encoded_query().find("%20") != core::string_view::npos ||
                u.encoded_query().find("+") != core::string_view::npos);

            params.append(param_view{"key&name", "val=ue"});
        }

        // Insert operations
        {
            url u("http://example.com/?a=1&c=3");
            auto params = u.params();

            auto it = params.find("c");
            params.insert(it, param_view{"b", "2"});
            BOOST_TEST(u.query() == "a=1&b=2&c=3");
        }

        // Erase operations
        {
            url u("http://example.com/?a=1&b=2&c=3");
            auto params = u.params();

            auto it = params.find("b");
            params.erase(it);
            BOOST_TEST(u.query() == "a=1&c=3");

            params.erase(params.begin());
            BOOST_TEST(u.query() == "c=3");
        }

        // Replace operations
        {
            url u("http://example.com/?a=1&b=2");
            auto params = u.params();

            auto it = params.find("a");
            params.replace(it, param_view{"a", "10"});

            auto it2 = params.find("a");
            BOOST_TEST(it2 != params.end());
            BOOST_TEST((*it2).value == "10");
        }

        // Clear
        {
            url u("http://example.com/?a=1&b=2&c=3");
            auto params = u.params();

            params.clear();
            BOOST_TEST(!u.has_query());
            BOOST_TEST(params.empty());
        }

        // Duplicate keys
        {
            url u("http://example.com/");
            auto params = u.params();

            params.append(param_view{"key", "value1"});
            params.append(param_view{"key", "value2"});
            params.append(param_view{"key", "value3"});

            int count = 0;
            for (auto const& p : params) {
                if (p.key == "key") ++count;
            }
            BOOST_TEST(count == 3);

            auto it = params.find("key");
            BOOST_TEST(it != params.end());
            BOOST_TEST((*it).value == "value1");
        }

        // Count operation
        {
            url u("http://example.com/?k=1&k=2&k=3&x=4");
            auto params = u.params();

            BOOST_TEST(params.count("k") == 3);
            BOOST_TEST(params.count("x") == 1);
            BOOST_TEST(params.count("missing") == 0);
        }

        // Empty keys and values
        {
            url u("http://example.com/");
            auto params = u.params();

            params.append(param_view{"", "empty-key"});
            params.append(param_view{"empty-value", ""});
            params.append(param_view{"", ""});

            BOOST_TEST(params.size() == 3);
        }

        // Large number of params (stress test)
        {
            url u("http://example.com/");
            auto params = u.params();

            for (int i = 0; i < 100; ++i) {
                std::string key = std::to_string(i);
                std::string val = std::to_string(i * 10);
                params.append(param_view{key, val});
            }

            BOOST_TEST(params.size() == 100);
        }
    }

    void test_url_comparison()
    {
        // Exact equality
        {
            url u1("https://example.com/path?q=1#frag");
            url u2("https://example.com/path?q=1#frag");
            BOOST_TEST(u1 == u2);
            BOOST_TEST(!(u1 != u2));
        }

        // Different encoding but same bytes
        {
            url u1("https://example.com/path%20here");
            url u2("https://example.com/path%20here");
            BOOST_TEST(u1 == u2);
        }

        // Path case sensitivity
        {
            url u3("http://example.com/Path");
            url u4("http://example.com/path");
            BOOST_TEST(u3 != u4);
        }

        // Ordering (operator<)
        {
            url u1("http://a.com/");
            url u2("http://b.com/");
            BOOST_TEST((u1 < u2) || (u2 < u1));

            url u3("http://example.com/a");
            url u4("http://example.com/b");
            BOOST_TEST(u3 < u4);
        }

        // Self-comparison
        {
            url u("https://example.com/");
            BOOST_TEST(u == u);
            BOOST_TEST(!(u != u));
            BOOST_TEST(!(u < u));
        }

        // Empty URLs
        {
            url u1;
            url u2;
            BOOST_TEST(u1 == u2);
        }

        // Different components
        {
            url u1("https://example.com/path");
            url u2("http://example.com/path");
            BOOST_TEST(u1 != u2);

            url u3("https://example.com/path");
            url u4("https://example.org/path");
            BOOST_TEST(u3 != u4);

            url u5("https://example.com/path1");
            url u6("https://example.com/path2");
            BOOST_TEST(u5 != u6);
        }
    }

    void test_authority_components()
    {
        // IPv4 addresses
        {
            url u1("http://127.0.0.1/");
            BOOST_TEST(u1.host_type() == host_type::ipv4);
            BOOST_TEST(u1.host() == "127.0.0.1");

            auto ipv4 = u1.host_ipv4_address();
            BOOST_TEST(ipv4.to_string() == "127.0.0.1");

            url u2("http://192.168.1.1:8080/");
            BOOST_TEST(u2.host_ipv4_address().to_string() == "192.168.1.1");
        }

        // IPv4 edge cases
        {
            url u1("http://0.0.0.0/");
            BOOST_TEST(u1.host_type() == host_type::ipv4);

            url u2("http://255.255.255.255/");
            BOOST_TEST(u2.host_type() == host_type::ipv4);
        }

        // IPv6 addresses
        {
            url u1("http://[::1]/");
            BOOST_TEST(u1.host_type() == host_type::ipv6);
            BOOST_TEST(u1.host() == "[::1]");

            auto ipv6 = u1.host_ipv6_address();
            BOOST_TEST(ipv6.is_loopback());

            url u2("http://[2001:db8::1]:8080/");
            BOOST_TEST(u2.host_type() == host_type::ipv6);
        }

        // IPv6 edge cases
        {
            url u1("http://[::]/");
            BOOST_TEST(u1.host_type() == host_type::ipv6);
        }

        // Domain names
        {
            url u1("http://example.com/");
            BOOST_TEST(u1.host_type() == host_type::name);
            BOOST_TEST(u1.host() == "example.com");

            url u2("http://sub.example.co.uk/");
            BOOST_TEST(u2.host_type() == host_type::name);
        }

        // Port extraction
        {
            url u1("http://example.com:8080/");
            BOOST_TEST(u1.has_port());
            BOOST_TEST(u1.port() == "8080");
            BOOST_TEST(u1.port_number() == 8080);

            url u2("http://example.com/");
            BOOST_TEST(!u2.has_port());
        }

        // Port edge cases
        {
            url u1("http://example.com:1/");
            BOOST_TEST(u1.port_number() == 1);

            url u2("http://example.com:65535/");
            BOOST_TEST(u2.port_number() == 65535);
        }

        // Userinfo extraction
        {
            url u1("http://user@example.com/");
            BOOST_TEST(u1.has_userinfo());
            BOOST_TEST(u1.user() == "user");
            BOOST_TEST(!u1.has_password());

            url u2("http://user:pass@example.com/");
            BOOST_TEST(u2.has_userinfo());
            BOOST_TEST(u2.user() == "user");
            BOOST_TEST(u2.has_password());
            BOOST_TEST(u2.password() == "pass");
        }

        // Authority view
        {
            url u("http://user:pass@example.com:8080/");
            auto auth = u.authority();

            BOOST_TEST(auth.buffer() == "user:pass@example.com:8080");
            BOOST_TEST(auth.user() == "user");
            BOOST_TEST(auth.password() == "pass");
            BOOST_TEST(auth.host() == "example.com");
            BOOST_TEST(auth.port() == "8080");
        }
    }

    void run()
    {
        test_parse_boundaries();
        test_url_construction();
        test_url_modification_setters();
        test_segments_operations();
        test_params_operations();
        test_url_comparison();
        test_authority_components();
    }
};

TEST_SUITE(public_interfaces_boundaries_test, "boost.url.public_interfaces_boundaries");

} // urls
} // boost
