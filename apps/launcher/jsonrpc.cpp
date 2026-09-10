//
// jsonrpc.cpp
// ~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "jsonrpc.hpp"

#include <chrono>
#include <memory>
#include <string>

namespace launcher {

bool rpc_result::ok() const
{
	return error_code_ == 0;
}

bool jsonrpc_session::valid() const
{
	return boost::variant2::visit(
		[](const auto& sp) { return sp != nullptr; }, *this);
}

void jsonrpc_session::start()
{
	boost::variant2::visit([](auto& sp) { if (sp) sp->start(); }, *this);
}

void jsonrpc_session::stop()
{
	boost::variant2::visit([](auto& sp) { if (sp) sp->stop(); }, *this);
}

bool jsonrpc_session::running() const
{
	return boost::variant2::visit(
		[](const auto& sp) { return sp != nullptr && sp->running(); }, *this);
}

void jsonrpc_session::notify(const std::string& method, const json::value& params)
{
	boost::variant2::visit([&](auto& sp) {
		if (sp) sp->notify(method, params);
	}, *this);
}

net::any_io_executor jsonrpc_session::get_executor()
{
	return boost::variant2::visit([](auto& sp) { return sp->get_executor(); }, *this);
}

net::awaitable<rpc_result> jsonrpc_session::async_call(const std::string& method,
	const json::value& params, std::chrono::milliseconds timeout)
{
	auto ex = co_await net::this_coro::executor;
	// 持有会话拷贝，保证后台 RPC 协程执行期间会话对象存活。
	auto sess = *this;

	struct race_state
	{
		bool done_ = false;
		boost::system::error_code ec_;
		json::object resp_;
	};
	auto st = std::make_shared<race_state>();
	net::steady_timer timer(ex, timeout);
	auto cancel_sig = std::make_shared<net::cancellation_signal>();

	// 后台 RPC 分支：完成后记录结果并取消定时器，唤醒等待的调用者。
	net::co_spawn(ex,
		[sess = std::move(sess), st, method, params, cancel_sig]() mutable -> net::awaitable<void>
		{
			boost::system::error_code ec;
			json::object resp;
			try {
				resp = co_await boost::variant2::visit(
					[&](auto& sp) -> net::awaitable<json::object> {
						return sp->async_call(method, params,
							net::redirect_error(net::use_awaitable, ec));
					}, sess);
			} catch (...) {
				ec = boost::asio::error::operation_aborted;
			}
			// 在 io_context 上串行记录结果（单线程，与超时判断互斥）。
			net::dispatch(co_await net::this_coro::executor,
				[st, resp = std::move(resp), ec, cancel_sig]() mutable {
					if (!st->done_) {
						st->done_ = true;
						st->ec_ = ec;
						st->resp_ = std::move(resp);
						cancel_sig->emit(net::cancellation_type::all);
					}
				});
		}, net::detached);

	// 等待定时器；RPC 完成时会取消定时器。
	auto slot = cancel_sig->slot();
	slot.assign([&timer](net::cancellation_type_t) { timer.cancel(); });
	boost::system::error_code tec;
	co_await timer.async_wait(net::redirect_error(net::use_awaitable, tec));
	slot.clear();

	if (!st->done_) {
		// 定时器先触发：超时。
		rpc_result res;
		res.error_code_ = kCodeServer;
		res.error_message_ = "rpc call timeout";
		co_return res;
	}

	rpc_result res;
	if (st->ec_) {
		res.error_code_ = kCodeServer;
		res.error_message_ = st->ec_.message();
		co_return res;
	}
	const auto& resp = st->resp_;
	if (auto e = resp.if_contains("error"); e && e->is_object()) {
		const auto& eo = e->as_object();
		res.error_code_ = json_num(eo, "code");
		res.error_message_ = json_str(eo, "message");
	} else if (auto r = resp.if_contains("result"); r) {
		res.result_ = *r;
	}
	co_return res;
}

} // namespace launcher
