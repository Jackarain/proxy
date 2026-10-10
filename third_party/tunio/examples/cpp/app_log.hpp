//
// app_log.hpp
// ~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

// 示例应用的分级日志：debug/info/warn/error/silent，统一输出到 stderr。
// 多线程 io 下通过互斥量串行化输出，避免不同线程的日志交错。

#include <atomic>
#include <iostream>
#include <mutex>
#include <string_view>
#include <utility>

namespace tun2socks_example {

enum class log_level {
    debug = 0,
    info = 1,
    warn = 2,
    error = 3,
    silent = 4,
};

inline const char* log_level_name(log_level lvl) noexcept
{
    switch (lvl) {
    case log_level::debug:
        return "debug";
    case log_level::info:
        return "info";
    case log_level::warn:
        return "warn";
    case log_level::error:
        return "error";
    default:
        return "silent";
    }
}

inline bool parse_log_level(std::string_view text, log_level& out) noexcept
{
    if (text == "debug") {
        out = log_level::debug;
    } else if (text == "info") {
        out = log_level::info;
    } else if (text == "warn" || text == "warning") {
        out = log_level::warn;
    } else if (text == "error") {
        out = log_level::error;
    } else if (text == "silent" || text == "none" || text == "off") {
        out = log_level::silent;
    } else {
        return false;
    }
    return true;
}

class logger
{
public:
    static logger& instance()
    {
        static logger inst;
        return inst;
    }

    void set_level(log_level lvl) noexcept
    {
        level_.store(lvl, std::memory_order_relaxed);
    }

    log_level level() const noexcept
    {
        return level_.load(std::memory_order_relaxed);
    }

    bool enabled(log_level lvl) const noexcept
    {
        return lvl != log_level::silent && lvl >= level();
    }

    template <typename... Args>
    void write(log_level lvl, Args&&... args)
    {
        if (!enabled(lvl)) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        std::cerr << "[" << log_level_name(lvl) << "] ";
        (std::cerr << ... << std::forward<Args>(args));
        std::cerr << std::endl;
    }

private:
    logger() = default;

    std::atomic<log_level> level_{log_level::info};
    std::mutex mutex_;
};

template <typename... Args>
void log_debug(Args&&... args)
{
    logger::instance().write(log_level::debug, std::forward<Args>(args)...);
}

template <typename... Args>
void log_info(Args&&... args)
{
    logger::instance().write(log_level::info, std::forward<Args>(args)...);
}

template <typename... Args>
void log_warn(Args&&... args)
{
    logger::instance().write(log_level::warn, std::forward<Args>(args)...);
}

template <typename... Args>
void log_error(Args&&... args)
{
    logger::instance().write(log_level::error, std::forward<Args>(args)...);
}

} // namespace tun2socks_example
