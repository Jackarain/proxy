//
// completion.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include <boost/asio.hpp>

#include <tuple>
#include <utility>

namespace tunio {
namespace net = boost::asio;
namespace detail {

// 异步完成回调不得在发起函数内执行。关联执行器支持 post 时直接排队；
// 不支持 post 的执行器（如 inline_executor、any_completion_executor）
// 先从回退执行器出栈，再 dispatch 到关联执行器。
template <typename FallbackExecutor, typename Handler, typename... Args>
void post_completion_on(
    const FallbackExecutor& fallback, Handler handler, Args... args)
{
    auto ex = net::get_associated_executor(handler, fallback);
    auto deliver = [h = std::move(handler),
                       args = std::tuple(std::move(args)...)]() mutable
    {
        std::apply(std::move(h), std::move(args));
    };

    if constexpr (requires {
                      net::post(std::move(ex), std::move(deliver));
                  })
    {
        net::post(std::move(ex), std::move(deliver));
    }
    else
    {
        // 某些执行器不能直接作为 post 的排队目标：先从回退执行器出栈，
        // 再 dispatch 到关联执行器，保证回调不在发起栈内执行。
        net::post(fallback,
            [ex = std::move(ex), deliver = std::move(deliver)]() mutable
            {
                net::dispatch(std::move(ex), std::move(deliver));
            });
    }
}

// 无法提供操作执行器时使用系统执行器作为提交位置；关联执行器仍会用于
// 最终执行完成回调。
template <typename Handler, typename... Args>
void post_completion(Handler handler, Args... args)
{
    post_completion_on(net::system_executor(),
        std::move(handler),
        std::move(args)...);
}

} // namespace detail
} // namespace tunio
