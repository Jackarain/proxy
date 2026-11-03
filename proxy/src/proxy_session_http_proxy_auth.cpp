//
// proxy_session_http_proxy_auth.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/proxy_session.hpp"
#include "proxy/authority.hpp"
#include "proxy/http_error_body.hpp"
#include "proxy/proxy_websocket_client.hpp"

#include <array>
#include <charconv>

namespace proxy {

	net::awaitable<int> proxy_session::http_authorization(std::string_view pa) noexcept
	{
		if (!auth_required())
			co_return PROXY_AUTH_SUCCESS;

		if (pa.empty())
			co_return PROXY_AUTH_NONE;

		auto pos = pa.find(' ');
		if (pos == std::string::npos)
			co_return PROXY_AUTH_ILLEGAL;

		auto type = pa.substr(0, pos);
		auto auth = pa.substr(pos + 1);

		if (!boost::iequals(type, "Basic"))
			co_return PROXY_AUTH_ILLEGAL;

		std::string userinfo;
		if (!strutil::base64_decode_strict(auth, userinfo))
			co_return PROXY_AUTH_ILLEGAL;

		pos = userinfo.find(':');
		if (pos == std::string::npos)
			co_return PROXY_AUTH_ILLEGAL;

		std::string uname = userinfo.substr(0, pos);
		std::string passwd = userinfo.substr(pos + 1);

		bool verify_passed = co_await check_userpasswd(uname, passwd);
		if (!verify_passed)
			co_return PROXY_AUTH_FAILED;

		co_return PROXY_AUTH_SUCCESS;
	}

} // namespace proxy
