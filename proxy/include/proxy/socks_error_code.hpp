//
// socks_error_code.hpp
// ~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2023_10_18__SOCKS_ERROR_CODE_HPP
#define INCLUDE__2023_10_18__SOCKS_ERROR_CODE_HPP


#include <boost/system/error_code.hpp>

namespace proxy {

	class error_category_impl;

	template<class error_category>
	const boost::system::error_category& error_category_single()
	{
		static error_category error_category_instance;

		return reinterpret_cast<const boost::system::error_category&>(
				error_category_instance);
	}

	inline const boost::system::error_category& error_category()
	{
		return error_category_single<proxy::error_category_impl>();
	}

	namespace errc {
		enum errc_t
		{
			/// SOCKS unsupported version.
			socks_unsupported_version = 1000,

			/// SOCKS username required.
			socks_username_required,

			/// SOCKS unsupported authentication version.
			socks_unsupported_authentication_version,

			/// SOCKS authentication error.
			socks_authentication_error,

			/// SOCKS general failure.
			socks_general_failure,

			/// connection not allowed by ruleset.
			socks_connection_not_allowed_by_ruleset,

			/// Network unreachable.
			socks_network_unreachable,

			/// Host unreachable.
			socks_host_unreachable,

			/// Connection refused.
			socks_connection_refused,

			/// TTL expired.
			socks_ttl_expired,

			/// SOCKS command not supported.
			socks_command_not_supported,

			/// Address type not supported.
			socks_address_type_not_supported,

			/// unassigned.
			socks_unassigned,

			/// unknown error.
			socks_unknown_error,

			/// SOCKS no identd running.
			socks_no_identd,

			/// SOCKS no identd running.
			socks_identd_error,

			/// request rejected or failed.
			socks_request_rejected_or_failed,

			/// request rejected becasue SOCKS server
			// cannot connect to identd on the client.
			socks_request_rejected_cannot_connect,

			/// request rejected because the client
			// program and identd report different user - ids
			socks_request_rejected_incorrect_userid,
		};

		inline boost::system::error_code make_error_code(errc_t e)
		{
			return boost::system::error_code(
				static_cast<int>(e), proxy::error_category());
		}
	}

	class error_category_impl
		: public boost::system::error_category
	{
		virtual const char* name() const noexcept override
		{
			return "SOCKS";
		}

		virtual std::string message(int e) const override
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
				return "SOCKS request rejected becasue SOCKS"
					" server cannot connect to identd on the client";
			case errc::socks_request_rejected_incorrect_userid:
				return "SOCKS request rejected because the client"
					" program and identd report different user";
			default:
				return "SOCKS Unknown PROXY error";
			}
		}
	};

}

namespace boost::system {
	template <>
	struct is_error_code_enum<proxy::errc::errc_t>
	{
		static const inline bool value = true;
	};
} // namespace boost

#endif // INCLUDE__2023_10_18__SOCKS_ERROR_CODE_HPP
