//
// proxy_session_auth.cpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/proxy_session.hpp"

#ifdef USE_PAM_AUTH
# include <security/pam_appl.h>
# include <security/pam_misc.h>
#endif

namespace proxy {

	//////////////////////////////////////////////////////////////////////////
	// PAM 认证

#ifdef USE_PAM_AUTH
	int proxy_session::pam_conv_func(int num_msg, const struct pam_message **msg,
		struct pam_response **resp, void *appdata_ptr)
	{
		if (num_msg <= 0 || num_msg > PAM_MAX_NUM_MSG)
			return PAM_CONV_ERR;

		*resp = (struct pam_response *)std::calloc(num_msg, sizeof(struct pam_response));
		if (*resp == nullptr)
			return PAM_BUF_ERR;

		const char *password = (const char *)appdata_ptr;

		for (int i = 0; i < num_msg; i++)
		{
			if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF)
			{
				(*resp)[i].resp = strdup(password);
				(*resp)[i].resp_retcode = 0;
			}
			else
			{
				(*resp)[i].resp = nullptr;
			}
		}

		return PAM_SUCCESS;
	}

	bool proxy_session::pam_authenticate_user(const char *service, const char *username, const char *password) noexcept
	{
		pam_handle_t *pamh = nullptr;
		struct pam_conv conv = {
			.conv = pam_conv_func,
			.appdata_ptr = (void *)password  // 传入密码
		};

		int retval = pam_start(service, username, &conv, &pamh);
		if (retval != PAM_SUCCESS)
		{
			log_conn_warning() << ", pam_start failed: " << pam_strerror(pamh, retval);
			return false;
		}

		retval = pam_authenticate(pamh, 0);  // 核心认证
		if (retval != PAM_SUCCESS) {
			log_conn_warning() << ", authentication failed: " << pam_strerror(pamh, retval);
			pam_end(pamh, retval);
			return false;
		}

		retval = pam_acct_mgmt(pamh, 0);	// 检查账户（如锁定、过期）
		if (retval != PAM_SUCCESS) {
			log_conn_warning() << ", account management failed: " << pam_strerror(pamh, retval);
			pam_end(pamh, retval);
			return false;
		}

		pam_end(pamh, PAM_SUCCESS);

		log_conn_debug() << ", pam_authenticate_user success";

		return true;
	}

	template <typename CompletionToken>
	auto
	proxy_session::async_pam_auth(const std::string& username, const std::string& passwd,
		const std::string& service_name, CompletionToken&& token) noexcept
	{
		return net::async_initiate<CompletionToken,
			void(boost::system::error_code, bool)>
			([this, username, passwd, service_name](auto&& handler) mutable
			{
				// 这里不需要保持对 proxy_session 的 shared_ptr 引用, 因为 PAM 认证完成后无论结果如
				// 何都不需要访问 proxy_session 的成员变量或方法了, 另外 async_pam_auth 总是运行在
				// 协程中，协程在 proxy_session 启动时就已经保存了 shared_ptr 自身.
				std::thread([this, username = std::move(username), passwd = std::move(passwd),
					service_name = std::move(service_name), handler = std::move(handler)]() mutable
					{
						bool result = pam_authenticate_user(
							service_name.c_str(),
							username.c_str(),
							passwd.c_str());

						auto executor = net::get_associated_executor(handler);
						net::post(executor,
							[result, handler = std::move(handler)]() mutable
							{
								boost::system::error_code ec;
								handler(ec, result);
							});
					}
				).detach();
			}, token);
	}

#endif // USE_PAM_AUTH

	//////////////////////////////////////////////////////////////////////////
	// 认证与授权

	bool proxy_session::auth_required() const noexcept
	{
		if (!m_option.auth_users_.empty())
			return true;

		if (!m_option.pam_auth_.empty())
			return true;

		return false;
	}

	net::awaitable<bool> proxy_session::check_userpasswd(
		const std::string& username,
		const std::string& passwd, bool skip_passwd) noexcept
	{
		// 若不需要认证, 直接返回 true.
		if (!auth_required())
			co_return true;

		// 检查用户名和密码是否匹配.
		for (const auto& [user, pwd, addr, proxy_pass] : m_option.auth_users_)
		{
			if (username == user)
			{
				if (!skip_passwd && passwd != pwd)
					continue;

				// 用户连接数限制: 已达该用户连接数上限则视为认证失败.
				if (!user_conn_acquire(user))
				{
					log_conn_warning()
						<< ", user connection limit exceeded, user: "
						<< user;
					co_return false;
				}

				// 记录认证用户（供 launcher 状态按用户统计）.
				m_auth_user = user;

				user_rate_limit_config(user);
				update_bind_interface(addr);

				if (proxy_pass)
					m_proxy_pass = proxy_pass;

				co_return true;
			}
		}

		// 如果启用了 PAM 认证, 则尝试使用 PAM 认证.
		if (!skip_passwd && !m_option.pam_auth_.empty())
		{
#ifdef USE_PAM_AUTH
			boost::system::error_code ec;
			auto result = co_await async_pam_auth(username, passwd,
				m_option.pam_auth_, net_awaitable[ec]);
			if (result)
			{
				// 用户连接数限制: 已达该用户连接数上限则视为认证失败.
				if (!user_conn_acquire(username))
				{
					log_conn_warning()
						<< ", user connection limit exceeded, user: "
						<< username;
					co_return false;
				}

				// 记录认证用户（供 launcher 状态按用户统计）.
				m_auth_user = username;

				user_rate_limit_config(username);
				co_return true;
			}
#endif
		}

		co_return false;
	}

} // namespace proxy
