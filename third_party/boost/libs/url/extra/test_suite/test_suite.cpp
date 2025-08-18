//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#include "test_suite.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <vector>
#include <stdexcept>

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <debugapi.h>
#include <crtdbg.h>
#include <sstream>
#endif

namespace test_suite {

//------------------------------------------------
//
// suite
//
//------------------------------------------------

any_suite::
~any_suite() = default;

//------------------------------------------------

suites&
suites::
instance() noexcept
{
    class suites_impl : public suites
    {
        std::vector<any_suite const*> v_;

    public:
        virtual ~suites_impl() = default;

        void
        insert(any_suite const& t) override
        {
            v_.push_back(&t);
        }

        iterator
        begin() const noexcept override
        {
            if(! v_.empty())
                return &v_[0];
            return nullptr;
        }

        iterator
        end() const noexcept override
        {
            if(! v_.empty())
                return &v_[0] + v_.size();
            return nullptr;
        }

        void
        sort() override
        {
            std::sort(
                v_.begin(),
                v_.end(),
                []( any_suite const* p0,
                    any_suite const* p1)
                {
                    auto s0 = p0->name();
                    auto n0 = std::strlen(s0);
                    auto s1 = p1->name();
                    auto n1 = std::strlen(s1);
                    return std::lexicographical_compare(
                        s0, s0 + n0, s1, s1 + n1);
                });
        }
    };
    static suites_impl impl;
    return impl;
}

//------------------------------------------------
//
// runner
//
//------------------------------------------------

any_runner*&
any_runner::
instance_impl() noexcept
{
    static any_runner* p = nullptr;
    return p;
}

any_runner&
any_runner::
instance() noexcept
{
    return *instance_impl();
}

any_runner::
any_runner() noexcept
    : prev_(instance_impl())
{
    instance_impl() = this;
}

any_runner::
~any_runner()
{
    instance_impl() = prev_;
}

//------------------------------------------------
//
// debug_stream
//
//------------------------------------------------

#ifdef _MSC_VER
namespace detail {

void
debug_streambuf::
write(char const* s)
{
    if(dbg_)
        ::OutputDebugStringA(s);
    os_ << s;
}

debug_streambuf::
debug_streambuf(
    std::ostream& os)
    : os_(os)
    , dbg_(::IsDebuggerPresent() != 0)
{
}

debug_streambuf::
~debug_streambuf()
{
    sync();
}

int
debug_streambuf::
sync()
{
    write(this->str().c_str());
    this->str("");
    return 0;
}
}
#endif

//------------------------------------------------
//
// implementation
//
//------------------------------------------------

namespace detail {

bool
test_impl(
    bool cond,
    char const* expr,
    char const* func,
    char const* file,
    int line)
{
    return any_runner::instance().test(
        cond, expr, func, file, line);
}

void
throw_failed_impl(
    const char* expr,
    char const* excep,
    char const* func,
    char const* file,
    int line)
{
    std::stringstream ss;
    ss <<
        "expression '" << expr <<
        "' didn't throw '" << excep <<
        "' in function '" << func <<
        "'";
   any_runner::instance().test(false, ss.str().c_str(),
       func, file, line);
}

void
no_throw_failed_impl(
    const char* expr,
    char const* excep,
    char const* func,
    char const* file,
    int line)
{
    std::stringstream ss;
    ss <<
        "expression '" << expr <<
        "' threw '" << excep <<
        "' in function '" << func <<
        "'";
   any_runner::instance().test(false, ss.str().c_str(),
       func, file, line);
}

void
no_throw_failed_impl(
    const char* expr,
    char const* func,
    char const* file,
    int line)
{
    std::stringstream ss;
    ss <<
        "expression '" << expr <<
        "' threw in function '" << func << "'";
   any_runner::instance().test(false, ss.str().c_str(),
       func, file, line);
}

//------------------------------------------------
//
// simple_runner
//
//------------------------------------------------

using clock_type =
    std::chrono::steady_clock;

struct elapsed
{
    clock_type::duration d;
};

std::ostream&
operator<<(
    std::ostream& os,
    elapsed const& e)
{
    using namespace std::chrono;
    auto const ms = duration_cast<
        milliseconds>(e.d);
    if(ms < seconds{1})
    {
        os << ms.count() << "ms";
    }
    else
    {
        std::streamsize width{
            os.width()};
        std::streamsize precision{
            os.precision()};
        os << std::fixed <<
            std::setprecision(1) <<
            (ms.count() / 1000.0) << "s";
        os.precision(precision);
        os.width(width);
    }
    return os;
}

} // detail
} // test_suite

//------------------------------------------------

namespace test_suite {
namespace detail {

class simple_runner : public any_runner
{
    struct summary
    {
        char const* name;
        clock_type::time_point start;
        clock_type::duration elapsed;
        std::atomic<std::size_t> failed;
        std::atomic<std::size_t> total;

        summary(summary const& other) noexcept
            : name(other.name)
            , start(other.start)
            , elapsed(other.elapsed)
            , failed(other.failed.load())
            , total(other.total.load())
        {
        }

        explicit
        summary(char const* name_) noexcept
            : name(name_)
            , start(clock_type::now())
            , failed(0)
            , total(0)
        {
        }
    };

    summary all_;
    std::ostream& log_;
    std::vector<summary> v_;
    bool log_time_at_exit_ = true;

    void
    log_time()
    {
        log_ <<
            elapsed{clock_type::now() -
                     all_.start } << ", " <<
            v_.size() << " suites, " <<
            all_.failed.load() << " failures, " <<
            all_.total.load() << " total." <<
            std::endl;
    }

public:
    explicit
    simple_runner(
        std::ostream& os)
        : all_("(all)")
        , log_(os)
    {
        std::unitbuf(log_);
        v_.reserve(256);
    }

    virtual ~simple_runner()
    {
        if (!log_time_at_exit_)
            return;
        log_time();
    }

    // true if no failures
    bool
    success() const noexcept
    {
        return all_.failed.load() == 0;
    }

    void
    run(any_suite const& test) override
    {
        v_.emplace_back(test.name());
        log_ << test.name() << "\n";
        test.run();
        v_.back().elapsed =
            clock_type::now() -
            v_.back().start;
    }

    void
    note(char const* msg) override
    {
        log_ << msg << "\n";
    }

    std::ostream&
    log() noexcept override
    {
        return log_;
    }

    char const*
    filename(
        char const* file)
    {
        auto const p0 = file;
        auto p = p0 + std::strlen(file);
        while(p-- != p0)
        {
        #ifdef _MSC_VER
            if(*p == '\\')
        #else
            if(*p == '/')
        #endif
                break;
        }
        return p + 1;
    }

    bool test(bool, char const*, char const*, char const*, int) override;

    bool
    log_time_at_exit(bool log)
    {
        bool const prev = log_time_at_exit_;
        log_time_at_exit_ = log;
        return prev;
    }
};

//------------------------------------------------

bool
simple_runner::
test(
    bool cond,
    char const* expr,
    char const* func,
    char const* file,
    int line)
{
    auto const id = ++all_.total;
    ++v_.back().total;
    if(cond)
        return true;
    ++all_.failed;
    ++v_.back().failed;
    (void)func;
    log_ <<
        "#" << id << " " <<
        filename(file) << "(" << line << ") "
        "failed: " << expr <<
        //" in " << func <<
        "\n";
    log_.flush();
    return false;
}

} // detail

namespace {
    int g_argc = 0;
    char const* const* g_argv = nullptr;
}

void
set_command_line(
    int argc,
    char const* const* argv)
{
    g_argc = argc;
    g_argv = argv;
}

int
argc()
{
    if (g_argc == 0 || g_argv == nullptr)
        throw std::runtime_error("Command-line arguments not set. Call set_command_line() first.");
    return g_argc;
}

char const* const*
argv()
{
    if (g_argc == 0 || g_argv == nullptr)
        throw std::runtime_error("Command-line arguments not set. Call set_command_line() first.");
    return g_argv;
}

char const*
get_command_line_option(
    char const* short_name,
    char const* name)
{
    if (g_argc == 0 || g_argv == nullptr)
        throw std::runtime_error("Command-line arguments not set. Call set_command_line() first.");

    std::size_t const short_len = std::strlen(short_name);
    std::size_t const name_len = std::strlen(name);

    for (int i = 1; i < g_argc; ++i)
    {
        const char* arg = g_argv[i];
        std::size_t arg_len = std::strlen(arg);

        if (arg_len < 2 ||
            arg[0] != '-')
        {
            // Not an option
            continue;
        }

        // Check for the long option: --name or --name=value
        if (name && std::strncmp(arg, "--", 2) == 0)
        {
            arg += 2;
            if (std::strncmp(arg, name, name_len) == 0)
            {
                if (arg[name_len] == '=')
                    return arg + name_len + 1;
                if (arg[name_len] == '\0' && i + 1 < g_argc)
                    return g_argv[i + 1];
            }
            arg -= 2; // restore for next check
        }

        // Check for the short option: -x or -x value
        if (short_name && std::strncmp(arg, "-", 1) == 0 && std::strncmp(arg, "--", 2) != 0)
        {
            arg += 1;
            if (std::strncmp(arg, short_name, short_len) == 0)
            {
                if (arg[short_len] == '=')
                    return arg + short_len + 1;
                if (arg[short_len] == '\0' && i + 1 < g_argc)
                    return g_argv[i + 1];
            }
        }
    }
    return nullptr;
}

/** Return a command-line flag

    This function returns the value of a command-line flag
    if it exists, otherwise it returns `false`.

    A command-line flag can be set with the `--name` format
    or the `-n` format.

    @param name The name of the command-line flag to search for.
    @return The value of the command-line flag or an empty string if not found.
 */
bool
get_command_line_flag(
    char const* short_name,
    char const* name)
{
    if (g_argc == 0 || g_argv == nullptr)
        throw std::runtime_error("Command-line arguments not set. Call set_command_line() first.");

    std::size_t const short_len = std::strlen(short_name);
    std::size_t const name_len = std::strlen(name);

    for (int i = 1; i < g_argc; ++i)
    {
        char const* arg = g_argv[i];
        std::size_t arg_len = std::strlen(arg);

        if (arg_len < 2 ||
            arg[0] != '-')
        {
            // Not a flag
            continue;
        }

        // Check for the long flag: --name
        if (name && std::strncmp(arg, "--", 2) == 0) {
            arg += 2;
            if (std::strncmp(arg, name, name_len) == 0
                && (arg[name_len] == '\0' || arg[name_len] == ' '))
            {
                return true;
            }
            arg -= 2; // restore for next check
        }

        // Check for the short flag: -n
        if (short_name && std::strncmp(arg, "-", 1) == 0
            && std::strncmp(arg, "--", 2) != 0)
        {
            arg += 1;
            if (std::strncmp(arg, short_name, short_len) == 0
                && (arg[short_len] == '\0' || arg[short_len] == ' '))
            {
                return true;
            }
        }
    }
    return false;
}

int
run(std::ostream& out)
{
    int argc = ::test_suite::argc();
    char const* const* argv = ::test_suite::argv();
    detail::simple_runner any_runner(out);
    suites::instance().sort();

    // If one of the arguments is -h or --help, show usage regardless
    // of other arguments
    if (get_command_line_flag("h", "help"))
    {
        // executable name
        char const* exe_name = argv[0];
        char const* last_slash = std::strrchr(exe_name, '/');
        if (!last_slash)
            last_slash = std::strrchr(exe_name, '\\');
        if (last_slash)
            exe_name = last_slash + 1;
        out <<
            "Run all test suites or only those matching the provided names.\n"
            "Usage:\n"
            "  " << exe_name << " <suite-specs> ... [ options ]\n"
                           "where\n"
                           "  - <suite-specs> are the names of test suite patterns to run\n"
                           "  - options are command-line flags or options\n"
                           "Available options and flags:\n"
                           "  -h, --help          Show this help message\n"
                           "  -l, --list-tests    List all available test suites\n"
            << std::endl;
        return EXIT_SUCCESS;
    }

    // If one of the arguments is --list-tests, list all available test suites
    // regardless of other arguments
    if (get_command_line_flag("l", "list-tests"))
    {
        for (any_suite const* e : suites::instance())
        {
            out << e->name() << "\n";
        }
        any_runner.log_time_at_exit(false);
        return EXIT_SUCCESS;
    }

    // Identify range of suite patterns
    char const* const* suite_pattern_begin = argv + 1;
    char const* const* suite_pattern_end = suite_pattern_begin;
    for (; suite_pattern_end < argv + argc; ++suite_pattern_end)
    {
        if (suite_pattern_end[0][0] == '-' && suite_pattern_end[0][1] != '\0')
           break; // stop at the first option
    }

    // Run the specified test suites
    bool const run_all = suite_pattern_begin == suite_pattern_end;
    auto should_run =
        [suite_pattern_begin, suite_pattern_end, run_all](
            char const* suite_name) {
        if (run_all)
        {
            return true;
        }
        std::size_t const suite_name_len = std::strlen(suite_name);
        for (char const* const* pattern_it = suite_pattern_begin;
             pattern_it != suite_pattern_end;
             ++pattern_it)
        {
            char const* pattern = *pattern_it;
            std::size_t const pattern_len = std::strlen(pattern);
            if (pattern_len > suite_name_len)
            {
                continue;
            }
            // 1. An exact match (such as "boost.url.grammar.parse")
            if (std::strcmp(pattern, suite_name) == 0)
            {
                return true;
            }
            // 2. Path match (such as "boost.url.grammar")
            if (pattern_len < suite_name_len &&
                suite_name[pattern_len] == '.' &&
                std::strncmp(suite_name, pattern, pattern_len) == 0)
            {
                return true;
            }
        }
        return false;
    };
    for (any_suite const* e : suites::instance())
    {
        if (should_run(e->name()))
        {
            any_runner.run(*e);
        }
    }
    return any_runner.success() ?
       EXIT_SUCCESS : EXIT_FAILURE;
}

int
run()
{
    ::test_suite::debug_stream out(std::cerr);
    return run(out);
}

int
run(std::ostream& out,
    int argc,
    char const* const* argv)
{
    ::test_suite::set_command_line(argc, argv);
    return run(out);
}

int
run(int argc, char const* const* argv)
{
    ::test_suite::set_command_line(argc, argv);
    return run();
}

} // test_suite
