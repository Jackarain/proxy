//
// json_rpc.hpp
// ~~~~~~~~~~~~
//
// JSON-RPC 2.0 over WebSocket 的对称连接端点封装。
// 基于 tinyrpc（third_party/tinyrpc/include/tinyrpc/jsonrpc.hpp）的
// jsonrpc_session 实现，提供与 golang 版本 internal/rpc 完全兼容的接口：
//   set_handler / notify / call / close / run
// 支持双向 Call / Notify / Handle 与请求-响应按 id 关联。
//
// 并发模型：tinyrpc 会话的读写全部 post 到其所属 io_context（连接线程），
// 本封装可从任意线程安全调用（notify/call/close 内部转发到会话执行器）。
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef LAUNCHER_JSON_RPC_HPP
#define LAUNCHER_JSON_RPC_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <boost/asio/io_context.hpp>
#include <boost/json.hpp>

#include <tinyrpc/jsonrpc.hpp>

namespace launcher::rpc {

// 标准 JSON-RPC 2.0 错误码。
inline constexpr int kCodeParse = -32700;    // 解析错误
inline constexpr int kCodeInvalid = -32600;  // 无效请求
inline constexpr int kCodeMethod = -32601;   // 方法不存在
inline constexpr int kCodeParams = -32602;   // 参数错误
inline constexpr int kCodeInternal = -32603; // 内部错误
inline constexpr int kCodeServer = -32000;   // 应用错误

// 调用结果。
struct call_result {
	boost::json::value result;
	int error_code = 0;
	std::string error_message;

	bool ok() const { return error_code == 0; }
};

// RPC 错误（处理器内抛出，由端点转换为 JSON-RPC error 响应）。
class error : public std::exception {
public:
	error(int code, std::string message)
		: code_(code)
		, message_(std::move(message)) {}

	int code() const { return code_; }
	const std::string& message() const { return message_; }

	const char* what() const noexcept override { return message_.c_str(); }

private:
	int code_;
	std::string message_;
};

namespace detail {

// 取 json 对象中的整数字段。
inline std::int64_t json_num(const boost::json::object& obj, const char* key) {
	auto it = obj.find(key);
	if (it == obj.end())
		return 0;
	const auto& v = it->value();
	if (v.is_int64())
		return v.as_int64();
	if (v.is_uint64())
		return static_cast<std::int64_t>(v.as_uint64());
	if (v.is_double())
		return static_cast<std::int64_t>(v.as_double());
	return 0;
}

// 取 json 对象中的字符串字段。
inline std::string json_str(const boost::json::object& obj, const char* key) {
	auto it = obj.find(key);
	if (it == obj.end() || !it->value().is_string())
		return {};
	return std::string(it->value().as_string());
}

} // namespace detail

// 连接端点（服务端/客户端通用）。WsStream 为 Beast websocket 底层流：
// tcp::socket 或 ssl::stream<tcp::socket>。
class endpoint_base : public std::enable_shared_from_this<endpoint_base> {
public:
	// 处理方法调用（请求或通知）。params 为参数对象。
	// 返回 result；抛出 rpc::error 表示错误（仅请求时回错误响应）。
	using handler_fn = std::function<boost::json::value(const boost::json::value& params)>;

	virtual ~endpoint_base() = default;

	// 注册一个方法处理器（请求与通知共用）。
	virtual void set_handler(const std::string& method, handler_fn h) = 0;

	// 发送一个无 id 的通知。
	virtual void notify(const std::string& method, const boost::json::value& params) = 0;

	// 发送请求并等待响应（阻塞，最多 timeout）。
	virtual call_result call(const std::string& method, const boost::json::value& params,
		std::chrono::milliseconds timeout) = 0;

	// 关闭连接（任意线程可调用，线程安全）。
	virtual void close() = 0;

	// 连接是否已关闭。
	virtual bool closed() const = 0;

	// 启动会话并运行 io_context，直到连接关闭（阻塞）。
	// ioc 为承载本会话的 io_context（连接线程运行）。
	virtual void run(boost::asio::io_context& ioc) = 0;
};

template <class WsStream>
class endpoint : public endpoint_base {
public:
	explicit endpoint(WsStream ws) {
		ws.read_message_max(16 * 1024 * 1024);
		ws.binary(true);
		sess_ = std::make_shared<jsonrpc::jsonrpc_session<WsStream>>(std::move(ws));
	}

	// ---- endpoint_base ----

	void set_handler(const std::string& method, handler_fn h) override {
		std::lock_guard<std::mutex> lock(handlers_mu_);
		handlers_[method] = std::move(h);
	}

	void notify(const std::string& method, const boost::json::value& params) override {
		auto sess = sess_;
		jsonrpc::net::post(sess->get_executor(),
			[sess, method, params]() mutable {
				sess->notify(method, params);
			});
	}

	call_result call(const std::string& method, const boost::json::value& params,
		std::chrono::milliseconds timeout) override {
		auto pr = std::make_shared<std::promise<call_result>>();
		auto fut = pr->get_future();
		auto sess = sess_;
		jsonrpc::net::post(sess->get_executor(),
			[sess, method, params, pr]() mutable {
				sess->async_call(method, params,
					[pr](boost::system::error_code ec, boost::json::object resp) {
						call_result res;
						if (ec) {
							res.error_code = kCodeServer;
							res.error_message = ec.message();
						} else if (auto e = resp.if_contains("error"); e && e->is_object()) {
							const auto& eo = e->as_object();
							res.error_code = static_cast<int>(detail::json_num(eo, "code"));
							res.error_message = detail::json_str(eo, "message");
						} else if (auto r = resp.if_contains("result"); r) {
							res.result = *r;
						}
						pr->set_value(std::move(res));
					});
			});
		if (fut.wait_for(timeout) == std::future_status::timeout) {
			call_result res;
			res.error_code = kCodeServer;
			res.error_message = "rpc call timeout";
			return res;
		}
		return fut.get();
	}

	void close() override {
		auto sess = sess_;
		jsonrpc::net::post(sess->get_executor(), [sess]() { sess->stop(); });
	}

	bool closed() const override {
		return closed_.load();
	}

	// 启动会话并运行 io_context，直到连接关闭。
	void run(boost::asio::io_context& ioc) override {
		// shared_from_this 返回基类指针，需转回具体端点以访问成员。
		auto self = std::static_pointer_cast<endpoint<WsStream>>(shared_from_this());
		sess_->notify_callback([self](boost::json::object obj) {
			self->handle_message(std::move(obj), false);
		});
		sess_->default_method_callback([self](boost::json::object obj) {
			self->handle_message(std::move(obj), true);
		});
		sess_->closed_callback([self]() {
			self->closed_.store(true);
		});
		sess_->start();
		// 运行 io_context 直到读循环退出（连接关闭）。
		ioc.run();
		closed_.store(true);
	}

private:
	// 分发一条请求（is_request=true，含 id，需回复）或通知（is_request=false）。
	void handle_message(boost::json::object obj, bool is_request) {
		std::string method = detail::json_str(obj, "method");
		boost::json::value params;
		if (auto p = obj.if_contains("params"); p)
			params = *p;

		handler_fn h;
		{
			std::lock_guard<std::mutex> lock(handlers_mu_);
			auto it = handlers_.find(method);
			if (it != handlers_.end())
				h = it->second;
		}
		if (!h) {
			// 未知方法：请求回 -32601，通知静默忽略。
			if (is_request) {
				boost::json::object err;
				err["code"] = kCodeMethod;
				err["message"] = "method not found: " + method;
				auto id = obj.if_contains("id") ? obj.at("id") : boost::json::value();
				sess_->reply(std::move(err), id, true);
			}
			return;
		}
		if (!is_request) {
			// 通知：执行并忽略结果与异常。
			try {
				h(params);
			} catch (...) {}
			return;
		}
		auto id = obj.if_contains("id") ? obj.at("id") : boost::json::value();
		try {
			auto result = h(params);
			sess_->reply(result, id, false);
		} catch (const error& e) {
			boost::json::object err;
			err["code"] = e.code();
			err["message"] = e.message();
			sess_->reply(std::move(err), id, true);
		} catch (const std::exception& e) {
			boost::json::object err;
			err["code"] = kCodeServer;
			err["message"] = e.what();
			sess_->reply(std::move(err), id, true);
		}
	}

	std::shared_ptr<jsonrpc::jsonrpc_session<WsStream>> sess_;
	std::mutex handlers_mu_;
	std::map<std::string, handler_fn> handlers_;
	std::atomic<bool> closed_{ false };
};

} // namespace launcher::rpc

#endif // LAUNCHER_JSON_RPC_HPP
