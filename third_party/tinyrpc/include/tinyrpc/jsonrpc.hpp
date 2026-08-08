//
// jsonrpc.hpp
// ~~~~~~~~~~~
//
// Copyright (c) 2023 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2023_10_18__JSONRPC_HPP
#define INCLUDE__2023_10_18__JSONRPC_HPP

#include <boost/system/error_code.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/detached.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <boost/json/value.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>

#include <atomic>
#include <functional>
#include <memory>
#include <utility>
#include <mutex>
#include <vector>
#include <deque>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>


namespace jsonrpc
{
  // 命名空间别名，方便使用
  namespace beast = boost::beast;
  namespace net = boost::asio;
  namespace json = boost::json;

  using coroutine_type = std::function<net::awaitable<void>(json::object)>;

  namespace detail
  {
    template <typename T, typename = void>
    struct has_is_open : std::false_type {};

    template <typename T>
    struct has_is_open<T, std::void_t<decltype(std::declval<T>().is_open())>>
        : std::is_same<bool, decltype(std::declval<T>().is_open())>
    {};

    template <typename T, typename = void>
    struct has_close : std::false_type {};

    template <typename T>
    struct has_close<T, std::void_t<decltype(std::declval<T>().close())>>
        : std::is_same<void, decltype(std::declval<T>().close())>
    {
    };

    // 函数特征模板，用于获取函数的参数类型和返回类型, 支持函数指针、函数对象和 lambda 表达式
    // 参考来源: boost/leaf/detail/function_traits.hpp
    template<class... T> struct mp_list
    {
    };

    template<class L> struct mp_pop_front_impl
    {
      // An error "no type named 'type'" here means that the argument to mp_pop_front
      // is either not a list, or is an empty list
    };

    template<template<class...> class L, class T1, class... T> struct mp_pop_front_impl<L<T1, T...>>
    {
        using type = L<T...>;
    };

    template<class L> using mp_pop_front = typename mp_pop_front_impl<L>::type;
    template<class L> using mp_rest = mp_pop_front<L>;


    template <class T> struct remove_noexcept { using type = T; };
    template <class R, class... A>  struct remove_noexcept<R(*)(A...) noexcept> { using type = R(*)(A...); };
    template <class C, class R, class... A>  struct remove_noexcept<R(C::*)(A...) noexcept> { using type = R(C::*)(A...); };
    template <class C, class R, class... A>  struct remove_noexcept<R(C::*)(A...) const noexcept> { using type = R(C::*)(A...) const; };

    template<class...>
    struct gcc49_workaround //Thanks Glen Fernandes
    {
        using type = void;
    };

    template<class... T>
    using void_t = typename gcc49_workaround<T...>::type;

    template<class F,class V=void>
    struct function_traits_impl
    {
        constexpr static int arity = -1;
    };

    template<class F>
    struct function_traits_impl<F, void_t<decltype(&F::operator())>>
    {
    private:

        using tr = function_traits_impl<typename remove_noexcept<decltype(&F::operator())>::type>;

    public:

        using return_type = typename tr::return_type;
        static constexpr int arity = tr::arity - 1;

        using mp_args = mp_rest<typename tr::mp_args>;

        template <int I>
        struct arg:
            tr::template arg<I+1>
        {
        };
    };

    template<class R, class... A>
    struct function_traits_impl<R(A...)>
    {
        using return_type = R;
        static constexpr int arity = sizeof...(A);

        using mp_args = mp_list<A...>;

        template <int I>
        struct arg
        {
            static_assert(I < arity, "I out of range");
            using type = typename std::tuple_element<I,std::tuple<A...>>::type;
        };
    };

    template<class F> struct function_traits_impl<F&> : function_traits_impl<F> { };
    template<class F> struct function_traits_impl<F&&> : function_traits_impl<F> { };
    template<class R, class... A> struct function_traits_impl<R(*)(A...)> : function_traits_impl<R(A...)> { };
    template<class R, class... A> struct function_traits_impl<R(* &)(A...)> : function_traits_impl<R(A...)> { };
    template<class R, class... A> struct function_traits_impl<R(* const &)(A...)> : function_traits_impl<R(A...)> { };
    template<class C, class R, class... A> struct function_traits_impl<R(C::*)(A...)> : function_traits_impl<R(C&,A...)> { };
    template<class C, class R, class... A> struct function_traits_impl<R(C::*)(A...) const> : function_traits_impl<R(C const &,A...)> { };
    template<class C, class R> struct function_traits_impl<R(C::*)> : function_traits_impl<R(C&)> { };

    template <class F>
    struct function_traits: function_traits_impl<typename remove_noexcept<F>::type>
    {
    };

    template <class F>
    using fn_return_type = typename function_traits<F>::return_type;

    template <class F, int I>
    using fn_arg_type = typename function_traits<F>::template arg<I>::type;

    template <class F>
    using fn_mp_args = typename function_traits<F>::mp_args;

    // RPC 操作的抽象基类
    class rpc_operation
    {
    public:
      virtual ~rpc_operation() = default;
      virtual void operator()(const boost::system::error_code &) = 0;
      virtual json::object &result() = 0;
    };

    // 模板类，用于处理异步 RPC 调用操作
    template <class Handler, class ExecutorType>
    class rpc_call_op : public rpc_operation
    {
    public:
      rpc_call_op(Handler &&h, ExecutorType executor)
        : handler_(std::forward<Handler>(h))
        , executor_(executor)
      {
      }

      rpc_call_op(const rpc_call_op &other) = delete;

      rpc_call_op(rpc_call_op &&other) noexcept
        : handler_(std::forward<Handler>(other.handler_))
        , executor_(other.executor_)
        , data_(std::move(other.data_))
      {
      }

      void operator()(const boost::system::error_code &ec) override
      {
        // 使用 net::dispatch 将结果发送到指定的执行器上
        // 这样可以确保在正确的线程或上下文中调用处理程序
        net::dispatch(
          executor_,
          [handler = std::move(handler_), data = std::move(data_), ec]() mutable
          {
            handler(ec, data);
          });
      }

      // 返回存储操作结果的 JSON 对象
      json::object &result() override
      {
        return data_;
      }

    private:
      Handler handler_;
      ExecutorType executor_;
      json::object data_;
    };

    // RPC 操作的智能指针类型
    using call_op_ptr = std::unique_ptr<rpc_operation>;


    // 从 JSON-RPC 对象中提取 'id' 字段, 保持原始 JSON 类型.
    inline json::value jsonrpc_id(const json::object &obj)
    {
      if (obj.if_contains("id"))
        return obj.at("id");
      return {};
    }

  }

  using detail::jsonrpc_id;


  namespace detail
  {
  template <class StreamType>
  class jsonrpc_session_service
    : public std::enable_shared_from_this<jsonrpc_session_service<StreamType>>
  {
    // 禁止拷贝和移动, 本服务对象由 std::shared_ptr 统一管理生命周期.
    jsonrpc_session_service(const jsonrpc_session_service &) = delete;
    jsonrpc_session_service &operator=(const jsonrpc_session_service &) = delete;

  public:
    using stream_type = StreamType;
    using next_layer_type = std::remove_reference_t<stream_type>;
    using executor_type = typename next_layer_type::executor_type;

    using call_op_ptr = detail::call_op_ptr;

    using write_context = std::unique_ptr<std::string>;
    using write_message_queue = std::deque<write_context>;

    friend class initiate_async_call;

    //////////////////////////////////////////////////////////////////////////

    // 构造函数, 可以接受一个 WebSocket 对象(ws/wss都可以).
    explicit jsonrpc_session_service(stream_type ws)
      : stream_(std::move(ws))
    {
    }

    // 析构函数，确保会话停止
    ~jsonrpc_session_service() noexcept
    {
      // 确保在析构时停止服务.
      if (running_.load())
        stop();
    }

    // 查询服务是否处于运行状态.
    bool running() const noexcept
    {
      return running_.load();
    }

    //////////////////////////////////////////////////////////////////////////

    // 启动服务, 开始接收 WebSocket 消息, 如果服务已经在运行, 则什么都不做.
    // 注意: 调用此函数相当于从 stream 中接收 JSON 数据并调用 dispatch()
    // 函数来派发 JSONRPC 协议消息.
    // 亦可手工调用 dispatch() 来处理 JSONRPC 消息, 但请注意这种情况下，我们
    // 不可以调用 start() 来驱动服务, 否则会导致逻辑错误.
    void start()
    {
      if (running_.load())
      {
        BOOST_ASSERT(false && "already running");
        return;
      }

      running_.store(true);

      // 通过 shared_from_this 持有自身, 保证 run() 协程执行期间本服务存活.
      auto self = this->shared_from_this();
      net::co_spawn(stream_.get_executor(),
      [self]() mutable -> net::awaitable<void>
      {
        try
        {
          co_await self->run();
        }
        catch (...)
        {}
        self->running_.store(false);

        // 消息循环已结束, 通知会话已关闭.
        if (self->closed_cb_)
          self->closed_cb_();

        co_return;
      }, net::detached);
    }

    // 停止服务, 关闭 WebSocket 连接, 如果服务没有运行, 则什么都不做.
    // 注意: 调用此函数后, 不能再调用 start() 启动服务, 如果需要重新
    // 启动服务, 请创建一个新的 jsonrpc_session 实例并调用 start()
    // 方法.
    void stop()
    {
      if (!running_.load())
      {
        BOOST_ASSERT(false && "not running");
        return;
      }

      running_.store(false);

      try
      {
        if constexpr (detail::has_is_open<stream_type>::value)
        {
          if (stream_.is_open())
          {
            if constexpr (detail::has_close<stream_type>::value)
              stream_.close();
            else
              beast::get_lowest_layer(stream_).close();
          }
        }
      }
      catch (const std::exception&)
      {}
    }

    // 手工调度一个 JSONRPC 协议, 这个函数可以用于在不运行 start 的前提下
    // 手工调度协议, obj 对象必须符合 JSONRPC 协议的要求.
    // 比如: 在用户程序中接收到一个 JSONRPC 请求消息, 可以直接调用这个函数
    // 将该消息传递给会话进行处理.
    void dispatch(json::object obj)
    {
      if (!obj.if_contains("jsonrpc"))
      {
        BOOST_ASSERT(false && "jsonrpc field not found");
        return;
      }

      // 通过 shared_from_this 持有自身, 保证 dispatch 协程执行期间本服务存活.
      auto self = this->shared_from_this();
      net::co_spawn(stream_.get_executor(),
        [self, obj = std::move(obj)]() mutable -> net::awaitable<void>
        {
          co_await self->dispatch_impl(std::move(obj));
          co_return;
        }, net::detached);
    }

    // 获取底层的 stream 流对象, 该对象可以用于直接进行 stream 操作.
    StreamType& stream() noexcept
    {
      return stream_;
    }

    // 释放底层的 stream 流对象到外部, 调用此函数后, 当前会话将不再拥有该 stream
    // 的所有权. 由调用者负责管理 stream 的生命周期.
    StreamType release() noexcept
    {
      return std::move(stream_);
    }

    //////////////////////////////////////////////////////////////////////////
    // 辅助类，用于发起异步 JSON-RPC 调用
    class initiate_async_call
    {
    public:
      using executor_type = jsonrpc_session_service::executor_type;

      explicit initiate_async_call(jsonrpc_session_service* self)
        : self_(self)
      {
      }

      executor_type get_executor() const noexcept
      {
        return self_->get_executor();
      }

      template <typename CallHandler>
      void operator()(CallHandler&& handler,
        const std::string& method, const json::value& params) const
      {
        auto executor = net::get_associated_executor(handler);
        using handler_executor_type = std::decay_t<decltype(executor)>;
        using rpc_call_op_type = detail::rpc_call_op<CallHandler, handler_executor_type>;

        auto op = std::make_unique<rpc_call_op_type>(
          std::forward<CallHandler>(handler), executor);

        json::object data;

        data["jsonrpc"] = "2.0";
        data["method"] = method;
        data["params"] = params;

        {
          std::lock_guard<std::mutex> lock(self_->call_op_mutex_);
          if (self_->id_recycle_.empty())
          {
            auto session_id = static_cast<int64_t>(self_->call_ops_.size());
            data["id"] = session_id;
            self_->call_ops_.emplace_back(std::move(op));
          }
          else
          {
            auto session_id = self_->id_recycle_.back();
            self_->id_recycle_.pop_back();

            data["id"] = session_id;
            self_->call_ops_[session_id] = std::move(op);
          }
        }

        // 发送 JSON 请求数据
        auto context = std::make_unique<std::string>(json::serialize(data));
        self_->write_message(std::move(context));
      }

    private:
      jsonrpc_session_service* self_;
    };
    //////////////////////////////////////////////////////////////////////////

    // 异步发送 JSONRPC 请求, 返回一个 JSON 对象作为响应.
    // 参数 params 代表要发送的请求数据的 JSONRPC 的 params 字段,
    // 该参数可以是一个 JSON 对象或数组, 须满足 JSONRPC 规范的要求.
    // method 参数代表要调用的远程方法名.
    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(
      void(boost::system::error_code, json::object))
        CallToken = net::default_completion_token_t<executor_type>>
    auto async_call(const std::string& method, const json::value& params,
      CallToken&& token = net::default_completion_token_t<executor_type>()) ->
      decltype(
        net::async_initiate<CallToken,
        void(boost::system::error_code, json::object)>(
          std::declval<initiate_async_call>(), token, method, params))
    {
      return net::async_initiate<CallToken,
        void(boost::system::error_code, json::object)>(
          initiate_async_call(this), token, method, params);
    }

    // 回复 JSONRPC 请求, 该函数接受一个 JSON value 作为参数代表响应数据,
    // 以及一个 id 值代表请求的 ID, 该 id 的类型应与原始请求保持一致.
    // 如果 error 参数为 true, 则表示这是一个错误 error 响应, 否则表示正常 result 响应.
    void reply(json::value response, json::value id, bool error = false)
    {
      json::object data;

      data["jsonrpc"] = "2.0";
      data["id"] = std::move(id);
      if (error)
        data["error"] = std::move(response);
      else
        data["result"] = std::move(response);

      // 将响应数据序列化为 JSON 字符串并发送
      auto context = std::make_unique<std::string>(json::serialize(data));
      write_message(std::move(context));
    }

    // 发送一个无 id 的通知消息.
    void notify(std::string_view method_name, const json::value& params)
    {
      json::object data;

      data["jsonrpc"] = "2.0";
      data["method"] = std::string(method_name);
      if (!params.is_null())
        data["params"] = params;

      auto context = std::make_unique<std::string>(json::serialize(data));
      write_message(std::move(context));
    }

    // 绑定一个 JSON-RPC 方法调用的协程函数, 当接收到对应的方法调用时会调用该函数.
    // 方法名是一个字符串, 代表远程方法的名称, handler 是一个协程函数或普通函数,
    // 接受一个 json::object 作为参数, 代表接收到的请求消息.
    template<typename Handler>
    void bind_method(std::string_view method_name, Handler&& handler)
    {
      if (method_name.empty())
      {
        BOOST_ASSERT(false && "method name or coroutine is invalid");
        return;
      }

      auto coroutine_handler = [
        this, handler = std::forward<Handler>(handler)]
        (json::object obj) mutable -> net::awaitable<void>
        {
          using ReturnType = detail::fn_return_type<Handler>;

          // Suppress -Wunused-lambda-capture on Clang when ReturnType
          // is void or awaitable<void> and this is not used in the
          // active if constexpr branch.
          (void)this;

          if constexpr (std::same_as<ReturnType, net::awaitable<void>>)
          {
            co_await handler(std::move(obj));
          }
          else if constexpr (std::is_same_v<ReturnType, net::awaitable<json::object>>)
          {
            auto id = jsonrpc_id(obj);
            auto response = co_await handler(std::move(obj));
            reply(response, std::move(id));
          }
          else if constexpr (std::is_same_v<ReturnType, net::awaitable<json::value>>)
          {
            auto id = jsonrpc_id(obj);
            auto response = co_await handler(std::move(obj));
            reply(std::move(response), std::move(id));
          }
          else if constexpr (std::is_same_v<ReturnType, json::object>)
          {
            auto id = jsonrpc_id(obj);
            auto response = handler(std::move(obj));
            reply(response, std::move(id));
          }
          else if constexpr (std::is_same_v<ReturnType, json::value>)
          {
            auto id = jsonrpc_id(obj);
            auto response = handler(std::move(obj));
            reply(std::move(response), std::move(id));
          }
          else if constexpr (std::is_same_v<ReturnType, void>)
          {
            handler(std::move(obj));
          }
        };

      remote_methods_[std::string(method_name)] = std::move(coroutine_handler);
    }

    // 设置请求回调函数, 当接收到请求消息时会调用该函数.
    // 请求消息在 JSONRPC 中是指包含 id 字段的 json 对象.
    // 回调函数的参数是一个 json::object, 代表接收到的请求消息.
    // 如果传入的回调函数为空, 则清除之前设置的回调函数.
    void default_method_callback(std::function<void(json::object)> cb)
    {
      method_cb_ = cb;
    }

    // 清除请求回调函数, 之后接收到请求消息时不会调用任何函数.
    void default_method_callback()
    {
      method_cb_ = {};
    }

    // 设置通知回调函数, 当接收到通知消息时会调用该函数.
    // 通知消息在 JSONRPC 中是指没有 id 字段的 json 对象.
    // 如果传入的回调函数为空, 则清除之前设置的回调函数.
    // 回调函数的参数是一个 json::object, 代表接收到的通知消息.
    void notify_callback(std::function<void(json::object)> cb)
    {
      notify_cb_ = cb;
    }

    // 清除通知回调函数, 之后接收到通知消息时不会调用任何函数.
    void notify_callback()
    {
      notify_cb_ = {};
    }

    // 设置错误回调函数, 当接收到无法执行 JSON 解析的错误消息时会调用该函数.
    // 回调函数的参数是接收到的消息数据.
    void error_callback(std::function<void(std::string_view)> cb)
    {
      error_cb_ = cb;
    }

    // 清除错误回调函数, 之后接收到错误消息时不会调用任何函数.
    void error_callback()
    {
      error_cb_ = {};
    }

    // 设置数据回调函数
    void data_callback(std::function<std::string(std::string_view)> cb)
    {
      data_cb_ = cb;
    }

    void data_callback()
    {
      data_cb_ = {};
    }

    // 设置会话关闭回调函数, 当会话消息循环结束时 (连接关闭/停止) 调用.
    // 如果传入的回调函数为空, 则清除之前设置的回调函数.
    void closed_callback(std::function<void()> cb)
    {
      closed_cb_ = std::move(cb);
    }

    // 清除会话关闭回调函数.
    void closed_callback()
    {
      closed_cb_ = {};
    }

    // 获取当前 jsonrpc_session 的执行器, 该执行器可以用于在协程中调度任务.
    net::any_io_executor get_executor() noexcept
    {
      return stream_.get_executor();
    }

    //////////////////////////////////////////////////////////////////////////

  private:
    // 运行服务的协程, 负责接收 WebSocket 消息并解析为 JSON 对象, 然
    // 后通过创建一个新的协程来处理接收到的 JSON 对象.
    net::awaitable<void> run()
    {
      try
      {
        boost::system::error_code ec;
        beast::flat_buffer buf;
        auto executor = co_await net::this_coro::executor;

        while (running_.load())
        {
          auto bytes = co_await stream_.async_read(buf, net::use_awaitable);

          auto bufdata = buf.data();
          std::string_view sv;
          std::string data;

          if (data_cb_)
          {
            data = data_cb_(std::string_view((const char*)bufdata.data(), bufdata.size()));
            sv = data;
          }
          else {
            sv = std::string_view((const char*)bufdata.data(), bufdata.size());
          }

          json::value jv = json::parse(
            sv,
            ec,
            json::storage_ptr{},
            {64, json::number_precision::imprecise, true, true, true});
          if (ec)
          {
            // 解析失败, 可能是因为接收到的消息不是有效的 JSON, 忽略该消息
            // 并继续等待下一个消息.
            if (error_cb_)
              error_cb_("parse json failed");
            else
              BOOST_ASSERT(false && "parse json failed");

            buf.consume(bytes);

            continue;
          }

          buf.consume(bytes);
          if (!jv.if_object())
          {
            // 解析结果不是一个 JSON 对象, 忽略该消息并继续等待下一个消息.
            if (error_cb_)
              error_cb_("parsed json is not an object");
            else
              BOOST_ASSERT(false && "parsed json is not an object");
            continue;
          }
          auto obj = jv.as_object();
          if (!obj.if_contains("jsonrpc"))
          {
            // 解析结果不是一个 JSONRPC 协议.
            if (error_cb_)
              error_cb_("jsonrpc field not found");
            else
              BOOST_ASSERT(false && "jsonrpc field not found");
            continue;
          }

          if (!running_.load())
            co_return; // 如果服务已经停止, 则退出协程

          net::co_spawn(executor, [self = this->shared_from_this(), obj = std::move(obj)]() mutable -> net::awaitable<void>
          {
            co_await self->dispatch_impl(std::move(obj));
            co_return;
          }, net::detached);
        }
      }
      catch (const std::exception&)
      {
        // 若服务已被显式停止 (running_ 为 false), 则视为正常关闭,
        // 不再上报错误, 以便 session 析构时能干净退出.
        if (running_.load())
        {
          // 捕获异常并调用错误回调函数
          if (error_cb_)
            error_cb_("exception occurred while running jsonrpc session");
          else
            BOOST_ASSERT(false && "exception occurred while running jsonrpc session");
        }
      }
    }

    net::awaitable<void> dispatch_impl(json::object obj)
    {
      auto try_id = obj.try_at("id");
      if (!try_id.has_value())
      {
        // 这是一个通知消息，回调通知处理函数
        if (notify_cb_)
          notify_cb_(std::move(obj));
        co_return;
      }

      // 这是一个请求或响应消息，检查 id 字段
      auto id = *try_id;
      if (!id.is_string() && !id.is_number())
      {
        // id 字段不是字符串或数字，忽略该消息
        BOOST_ASSERT(false && "id must be string or number");
        co_return;
      }

      if (obj.if_contains("result") || obj.if_contains("error"))
      {
        // 包含 result 或 error 的 json 对象说明当前是作为调用者身份
        // 向远端发起 RPC 请求的回应.
        if (call_ops_.empty())
        {
          // 如果没有正在进行的调用操作，忽略该消息
          BOOST_ASSERT(false && "no call operation in progress");
          co_return;
        }

        int64_t session_id = -1;
        try
        {
          session_id = (id.is_number() ? id.as_int64() : std::stoi(std::string(id.as_string())));
        }
        catch(const std::exception&)
        {
          // 转换失败，忽略该消息
          if (error_cb_)
            error_cb_("invalid id format");
          else
            BOOST_ASSERT(false && "invalid id format");
          co_return;
        }

        co_await handle_call(std::move(obj), session_id);

        co_return;
      }
      else if (obj.if_contains("method"))
      {
        if (!obj["method"].is_string())
        {
          // method 字段不是字符串，忽略该消息
          if (error_cb_)
            error_cb_("method must be string");
          else
            BOOST_ASSERT(false && "method must be string");
          co_return;
        }

        // 包含 method 字段的 json 对象说明当前是作为服务端身份
        std::string method (obj["method"].as_string());
        co_await handle_method(std::move(obj), method);

        // 处理方法调用消息
        co_return;
      }
      else
      {
        // 既不是请求也不是响应，忽略该消息
        BOOST_ASSERT(false && "not a request or response");
      }

      co_return;
    }

    net::awaitable<void> handle_call(json::object obj, int64_t session_id)
    {
      // 查找是否有对应的调用操作
      call_op_ptr handler;

      {
        std::lock_guard<std::mutex> lock(call_op_mutex_);
        if (session_id < 0 || session_id >= static_cast<int>(call_ops_.size()))
        {
          // id 不在有效范围内，忽略该消息
          if (error_cb_)
            error_cb_("invalid session id");
          else
            BOOST_ASSERT(false && "invalid session id");
          co_return;
        }
        // 获取对应的调用操作
        handler = std::move(call_ops_[session_id]);

        // 回收 RPC 调用操作的 id
        id_recycle_.push_back(session_id);

        BOOST_ASSERT(handler && "call op is nullptr!");
      }

      if (handler)
      {
        // 调用操作存在，执行它
        handler->result() = std::move(obj);
        (*handler)(boost::system::error_code{});
      }
      else
      {
        // 没有找到对应的调用操作，忽略该消息
        if (error_cb_)
          error_cb_("no call operation found for id");
        else
          BOOST_ASSERT(false && "no call operation found for id");
      }

      co_return;
    }

    net::awaitable<void> handle_method(json::object obj, std::string_view method_name)
    {
      // 检查远程方法是否已注册
      auto it = remote_methods_.find(std::string(method_name));
      if (it != remote_methods_.end())
      {
        co_await it->second(std::move(obj));
      }
      else if (method_cb_)
      {
        method_cb_(std::move(obj));
      }
      else
      {
        // 方法未找到且没有设置默认回调，返回 JSON-RPC 标准 -32601 错误响应
        auto id = jsonrpc_id(obj);
        if (!id.is_null())
        {
          json::object error_obj = {
            {"code", -32601},
            {"message", "Method not found"}
          };
          reply(std::move(error_obj), std::move(id), true);
        }
        else
        {
          BOOST_ASSERT(false && "no method callback set");
        }
      }

      co_return;
    }

    // 异步写入消息到 WebSocket, 该函数接受一个 write_context, 该上下文包含要写入的消息数据.
    // 如果当前没有正在进行的写入操作, 则直接发送消息, 否则将消息添加到写入队列中.
    // 注意: 该函数是线程安全的, 可以在任何线程中调用.
    void write_message(write_context context)
    {
      if (!context || context->empty())
      {
        BOOST_ASSERT(false && "context is empty");
        return;
      }

      // 通过 shared_from_this 持有自身, 保证写消息协程执行期间本服务存活.
      auto self = this->shared_from_this();
      net::dispatch(stream_.get_executor(),
        [self, context = std::move(context)]() mutable
        {
          bool write_in_progress = !self->write_msgs_.empty();
          self->write_msgs_.emplace_back(std::move(context));

          if (write_in_progress)
            return;

          // 直接调用协程来处理写入消息
          net::co_spawn(self->stream_.get_executor(),
            [self]() mutable -> net::awaitable<void>
            {
              co_await self->write_messages();
              co_return;
            }, net::detached);
        });
    }

    // 处理 WebSocket 写入消息的协程
    // 注意: 发送消息不依赖 running_ 状态, 因此手动 dispatch 模式 (模式 B)
    // 下即使未调用 start() 也能正常发送响应, 服务生命周期由 shared_from_this
    // 持有的 self 保证, 无需再依赖 running_ 来防止悬垂访问.
    net::awaitable<void> write_messages()
    {
      try
      {
        while (!write_msgs_.empty())
        {
          // 发送消息
          auto msg = std::move(write_msgs_.front());
          co_await stream_.async_write(net::buffer(*msg), net::use_awaitable);
          write_msgs_.pop_front();
        }
      }
      catch(const std::exception& e)
      {
        if (error_cb_)
        {
          write_msgs_.clear();
          error_cb_(std::string_view(e.what()));
        }
      }
    }

  private:
    // Stream 对象, 用于与远程服务进行通信.
    stream_type stream_;

    // 会话运行状态标志.
    std::atomic_bool running_{false};

    // 回调函数, 用于处理请求、通知消息和错误消息.
    std::function<void(json::object)> method_cb_;

    // 处理通知消息的回调.
    std::function<void(json::object)> notify_cb_;

    // 处理错误相关的回调.
    std::function<void(std::string_view)> error_cb_;

    // 数据处理相关回调, 如果设置了这个函数, 则解析该回调函数
    // 返回的数据.
    std::function<std::string(std::string_view)> data_cb_;

    // 会话关闭回调, 当消息循环结束时调用.
    std::function<void()> closed_cb_;

    // 注册的 RPC 调用方法.
    std::unordered_map<std::string, coroutine_type> remote_methods_;

    // 保护调用操作的互斥锁.
    std::mutex call_op_mutex_;
    std::vector<int64_t> id_recycle_;
    std::vector<call_op_ptr> call_ops_;

    // 消息发送队列.
    write_message_queue write_msgs_;
  };
  } // namespace detail

  using detail::jsonrpc_id;

  //////////////////////////////////////////////////////////////////////////
  // 对外门面类 jsonrpc_session
  // 内部持有 detail::jsonrpc_session_service 的 shared_ptr, 全部接口
  // 通过 impl_ 转发实现.
  template <class StreamType>
  class jsonrpc_session
  {
    // c++11 noncopyable.
    jsonrpc_session(const jsonrpc_session &) = delete;
    jsonrpc_session &operator=(const jsonrpc_session &) = delete;

  public:
    using stream_type = StreamType;
    using next_layer_type = std::remove_reference_t<stream_type>;
    using executor_type = typename next_layer_type::executor_type;

    using call_op_ptr = detail::call_op_ptr;

    using write_context = std::unique_ptr<std::string>;
    using write_message_queue = std::deque<write_context>;

    // 构造函数, 可以接受一个 WebSocket 对象(ws/wss都可以).
    explicit jsonrpc_session(stream_type ws)
      : impl_(std::make_shared<detail::jsonrpc_session_service<StreamType>>(std::move(ws)))
    {
    }

    // 移动构造函数, 转移 impl_ 的所有权.
    jsonrpc_session(jsonrpc_session &&) noexcept = default;
    jsonrpc_session &operator=(jsonrpc_session &&) noexcept = default;

    // 析构函数, 若底层服务正在运行则停止服务并关闭连接.
    // 注意: 协程通过 shared_from_this 持有服务, 这里显式 stop()
    // 使运行中的协程结束, 避免门面销毁后服务仍挂起在 I/O 上.
    ~jsonrpc_session() noexcept
    {
      if (impl_ && impl_->running())
        impl_->stop();
    }

    // 启动服务, 开始接收 WebSocket 消息.
    void start()
    {
      impl_->start();
    }

    // 停止服务, 关闭 WebSocket 连接.
    void stop()
    {
      impl_->stop();
    }

    // 手工调度一个 JSONRPC 协议, 可以用于在不运行 start 的前提下
    // 手工调度协议.
    void dispatch(json::object obj)
    {
      impl_->dispatch(std::move(obj));
    }

    // 获取底层的 stream 流对象.
    StreamType& stream() noexcept
    {
      return impl_->stream();
    }

    // 释放底层的 stream 流对象到外部.
    StreamType release() noexcept
    {
      return impl_->release();
    }

    // 异步发送 JSONRPC 请求, 返回一个 JSON 对象作为响应.
    template<typename CallToken = net::default_completion_token_t<executor_type>>
    auto async_call(const std::string& method, const json::value& params,
      CallToken&& token = net::default_completion_token_t<executor_type>()) ->
      decltype(std::declval<detail::jsonrpc_session_service<StreamType>&>().async_call(
        method, params, std::forward<CallToken>(token)))
    {
      return impl_->async_call(method, params, std::forward<CallToken>(token));
    }

    // 回复 JSONRPC 请求.
    void reply(json::value response, json::value id, bool error = false)
    {
      impl_->reply(std::move(response), std::move(id), error);
    }

    // 发送一个无 id 的通知消息.
    void notify(std::string_view method_name, const json::value& params)
    {
      impl_->notify(method_name, params);
    }

    // 绑定一个 JSON-RPC 方法调用的协程函数.
    template<typename Handler>
    void bind_method(std::string_view method_name, Handler&& handler)
    {
      impl_->bind_method(method_name, std::forward<Handler>(handler));
    }

    // 设置请求回调函数.
    void default_method_callback(std::function<void(json::object)> cb)
    {
      impl_->default_method_callback(std::move(cb));
    }

    void default_method_callback()
    {
      impl_->default_method_callback();
    }

    // 设置通知回调函数.
    void notify_callback(std::function<void(json::object)> cb)
    {
      impl_->notify_callback(std::move(cb));
    }

    void notify_callback()
    {
      impl_->notify_callback();
    }

    // 设置错误回调函数.
    void error_callback(std::function<void(std::string_view)> cb)
    {
      impl_->error_callback(std::move(cb));
    }

    void error_callback()
    {
      impl_->error_callback();
    }

    // 设置数据回调函数.
    void data_callback(std::function<std::string(std::string_view)> cb)
    {
      impl_->data_callback(std::move(cb));
    }

    void data_callback()
    {
      impl_->data_callback();
    }

    // 设置会话关闭回调函数.
    void closed_callback(std::function<void()> cb)
    {
      impl_->closed_callback(std::move(cb));
    }

    void closed_callback()
    {
      impl_->closed_callback();
    }

    // 获取当前 jsonrpc_session 的执行器.
    net::any_io_executor get_executor() noexcept
    {
      return impl_->get_executor();
    }

  private:
    std::shared_ptr<detail::jsonrpc_session_service<StreamType>> impl_;
  };

  using ws_jsonrpc_session = jsonrpc_session<beast::websocket::stream<beast::tcp_stream>>;
}

#endif // INCLUDE__2023_10_18__JSONRPC_HPP
