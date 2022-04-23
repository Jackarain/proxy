//
// socks_error_code.cpp
// ~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "socks/socks_error_code.hpp"

namespace socks {

	namespace errc {
		boost::system::error_code make_error_code(errc_t e)
		{
			return boost::system::error_code(static_cast<int>(e), socks::error_category());
		}
	}

	const char* error_category_impl::name() const BOOST_SYSTEM_NOEXCEPT
	{
		return "SOCKS";
	}

	std::string error_category_impl::message(int e) const
	{
		switch (static_cast<errc::errc_t>(e))
		{
		case errc::socks_unsupported_version:
			return "SOCKS unsupported version";
		case errc::socks_username_required:
			return "SOCKS username required";
		case errc::socks_unsupported_authentication_version:
			return "SOCKS unsupported authentication version";
		case errc::socks_authentication_error:
			return "SOCKS authentication error";
		case errc::socks_general_failure:
			return "SOCKS general failure";
		case errc::socks_connection_not_allowed_by_ruleset:
			return "SOCKS connection not allowed by ruleset";
		case errc::socks_network_unreachable:
			return "SOCKS network unreachable";
		case errc::socks_host_unreachable:
			return "SOCKS host unreachable";
		case errc::socks_connection_refused:
			return "SOCKS connection refused";
		case errc::socks_ttl_expired:
			return "SOCKS TTL expired";
		case errc::socks_command_not_supported:
			return "SOCKS command not supported";
		case errc::socks_address_type_not_supported:
			return "SOCKS Address type not supported";
		case errc::socks_unassigned:
			return "SOCKS unassigned";
		case errc::socks_unknown_error:
			return "SOCKS unknown error";
		case errc::socks_no_identd:
			return "SOCKS no identd running";
		case errc::socks_identd_error:
			return "SOCKS no identd running";
		case errc::socks_request_rejected_or_failed:
			return "SOCKS request rejected or failed";
		case errc::socks_request_rejected_cannot_connect:
			return "SOCKS request rejected becasue SOCKS server cannot connect to identd on the client";
		case errc::socks_request_rejected_incorrect_userid:
			return "SOCKS request rejected because the client program and identd report different user";
		default:
			return "SOCKS Unknown PROXY error";
		}
	}

	const boost::system::error_category& error_category()
	{
		return error_category_single<socks::error_category_impl>();
	}

}
