//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#include <boost/url/error.hpp>
#include <boost/url/string_view.hpp>
#include <boost/url/url.hpp>
#include <boost/url/url_view.hpp>
#include <boost/url/authority_view.hpp>
#include <boost/url/segments_view.hpp>
#include <boost/url/segments_encoded_view.hpp>
#include <boost/url/segments_ref.hpp>
#include <boost/url/segments_encoded_ref.hpp>
#include <boost/url/ipv4_address.hpp>
#include <boost/url/decode_view.hpp>
#include <boost/url/param.hpp>
#include <boost/url/params_view.hpp>
#include <boost/url/pct_string_view.hpp>
#include <boost/url/grammar/recycled.hpp>
#include <boost/assert/source_location.hpp>
#include <boost/core/ignore_unused.hpp>
#include <system_error>
#include "test_suite.hpp"

namespace boost {
namespace urls {

struct natvis_test
{
    struct yesexcept
    {
        int id;
        yesexcept()
            : id([]
                {
                    static int id_ = 0;
                    return ++id_;
                }())
        {
        }
        yesexcept(yesexcept&& u) { id = u.id; }
        yesexcept(yesexcept const& u) { id = u.id; }
        yesexcept& operator=(yesexcept&& u) { id = u.id; return *this; }
        yesexcept& operator=(yesexcept const& u) { id = u.id; return *this; }
    };

    struct my_category : boost::system::error_category
    {
        my_category() noexcept
            : boost::system::error_category(0xabadfadeadeadfad)
        {
        }

        const char* name() const noexcept override
        {
            return "boost.url.natvis";
        }

        std::string message(int) const override
        {
            return {};
        }

        boost::system::error_condition default_error_condition(
            int ev) const noexcept override
        {
            return {ev, *this};
        }
    };

    // these are here to view the results of
    // .natvis definitions in the debugger.
    void
    run()
    {
        // boost::assert::source_location
        {
            static auto loc = BOOST_CURRENT_LOCATION;
            ignore_unused(loc);
        }

        // boost::variant2::variant
        {
        }

        // boost::system::error_category
        {
            auto const& c1 = boost::system::generic_category();
            auto const& c2 = boost::system::system_category();
            system::error_code e3{std::error_code()};
            auto const& c3 = e3.category();
            auto const& c4 = my_category();
            system::error_code e5(error::not_a_base);
            auto const& c5 = e5.category();
            ignore_unused(c1, c2, c3, c4, c5);
        }

        // boost::system::error_code
        {
            static auto loc = BOOST_CURRENT_LOCATION;
            auto const e0 = system::error_code();
            auto const e1 = system::error_code(std::make_error_code(std::errc::address_in_use));
            auto const e2 = system::error_code(error::success);
            auto const e3a = system::error_code(error::not_a_base);
            auto const e3b = system::error_code(make_error_code(boost::system::errc::bad_address));
            auto const e4 = system::error_code(system::error_code(error::success), &loc);
            auto const e5 = system::error_code(system::error_code(error::not_a_base), &loc);
            ignore_unused(e0, e1, e2, e3a, e3b, e4, e5);
        }

        // boost::system::result
        {
            system::result<double> rv1;
            system::result<double> rv2 = 3.14;
            system::result<double> rv3 = error::not_a_base;
            system::result<yesexcept> rv4;
            system::result<yesexcept> rv5 = yesexcept{};
            system::result<yesexcept> rv6 = error::not_a_base;
            ignore_unused(rv1, rv2, rv3, rv4, rv5, rv6);
        }

        // boost::core::string_view
        {
            core::string_view s1;
            core::string_view s2 = "This is how we do it";
            core::string_view s3 = s2.substr(8, 3);
            ignore_unused(s1, s2, s3);
        }

        // boost::urls::url_view_base (url_view, url)
        {
            url_view uv1;
            url_view uv2("http://example.com/path/to/file.txt?key=value#frag");
            url_view uv3("https://user:pass@www.example.com:8080/");
            url u1;
            url u2("ftp://ftp.example.com/resource");
            url u3("mailto:user@example.com");
            ignore_unused(uv1, uv2, uv3, u1, u2, u3);
        }

        // boost::urls::authority_view
        {
            authority_view av1;
            authority_view av2 = parse_authority("example.com").value();
            authority_view av3 = parse_authority("user:pass@example.com:8080").value();
            authority_view av4 = parse_authority("[::1]:443").value();
            ignore_unused(av1, av2, av3, av4);
        }

        // boost::urls::ipv4_address
        {
            ipv4_address ip1;
            ipv4_address ip2("127.0.0.1");
            ipv4_address ip3("192.168.1.100");
            ipv4_address ip4 = parse_ipv4_address("10.0.0.1").value();
            ignore_unused(ip1, ip2, ip3, ip4);
        }

        // boost::urls::decode_view
        {
            decode_view dv1;
            decode_view dv2("hello%20world");
            decode_view dv3("100%25%20complete");
            ignore_unused(dv1, dv2, dv3);
        }

        // boost::urls::pct_string_view
        {
            pct_string_view psv1;
            pct_string_view psv2("plain");
            pct_string_view psv3("encoded%20text", 12);
            ignore_unused(psv1, psv2, psv3);
        }

        // boost::urls::param types
        {
            param p1{"key", "value"};
            param p2{"flag", no_value};
            param_view pv1{"key", "value"};
            param_view pv2{"flag", no_value};
            param_pct_view ppv1{"key%20name", "value%20data"};
            param_pct_view ppv2{"flag", no_value};
            ignore_unused(p1, p2, pv1, pv2, ppv1, ppv2);
        }

        // boost::urls::segments_view and segments_encoded_view
        {
            url_view u("/path/to/file.txt");
            segments_view sv1;
            segments_view sv2 = u.segments();
            segments_encoded_view sev1;
            segments_encoded_view sev2 = u.encoded_segments();

            // Test iterators
            auto sit1 = sv2.begin();
            auto sit2 = sv2.end();
            auto seit1 = sev2.begin();
            auto seit2 = sev2.end();

            ignore_unused(sv1, sv2, sev1, sev2, sit1, sit2, seit1, seit2);
        }

        // boost::urls::segments_ref and segments_encoded_ref
        {
            url u("/path/to/file.txt");
            segments_ref sr = u.segments();
            segments_encoded_ref ser = u.encoded_segments();

            // Test iterators
            auto srit1 = sr.begin();
            auto srit2 = sr.end();
            auto serit1 = ser.begin();
            auto serit2 = ser.end();

            ignore_unused(sr, ser, srit1, srit2, serit1, serit2);
        }

        // boost::urls::grammar::recycled_ptr
        {
            static grammar::recycled<url> bin;
            grammar::recycled_ptr<url> rp1;
            grammar::recycled_ptr<url> rp2(bin);
            *rp2 = url("http://example.com");
            ignore_unused(rp1, rp2);
        }
    }
};

TEST_SUITE(
    natvis_test,
    "boost.url.natvis");

} // urls
} // boost

