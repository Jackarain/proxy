// Copyright Antony Polukhin, 2015-2025
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org


#include "../example/b2_workarounds.hpp"

#include <boost/dll.hpp>

#include <cctype>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include <boost/filesystem/path.hpp>

#include <boost/core/lightweight_test.hpp>

namespace {

typedef std::vector<boost::dll::fs::path> paths_t;

class simple_barrier {
    std::mutex mutex_;
    std::condition_variable cv_;
    std::size_t generation_{0};
    const std::size_t initial_count_;
    std::size_t count_{initial_count_};

public:
    explicit simple_barrier(std::size_t count) : initial_count_(count) {}
    simple_barrier(simple_barrier&&) = delete;
    simple_barrier(const simple_barrier&) = delete;

    void wait() {
        std::unique_lock<std::mutex> lock{mutex_};
        const auto gen_snapshot = generation_;

        if (--count_ == 0) {
            ++generation_;
            count_ = initial_count_;
            cv_.notify_all();
        } else {
            cv_.wait(lock, [this, gen_snapshot] {
                return gen_snapshot != generation_;
            });
        }
    }
};

// Disgusting workarounds for b2 on Windows platform
inline paths_t generate_paths(int argc, char* argv[]) {
    paths_t ret;
    ret.reserve(argc - 1);

    for (int i = 1; i < argc; ++i) {
        boost::dll::fs::path p = argv[i];
        if (b2_workarounds::is_shared_library(p)) {
            ret.push_back(p);
        }
    }

    return ret;
}

inline void load_unload(const paths_t& paths, std::size_t count, simple_barrier& b) {
    for (std::size_t j = 0; j < count; j += 2) {
        for (std::size_t i = 0; i < paths.size(); ++i) {
            boost::dll::shared_library lib(paths[i]);
            BOOST_TEST(lib);
        }
        for (std::size_t i = 0; i < paths.size(); ++i) {
            boost::dll::shared_library lib(paths[i]);
            BOOST_TEST(lib.location() != "");
        }

        // Waiting for all threads to unload shared libraries
        b.wait();
    }
}

}  // namespace


int main(int argc, char* argv[]) {
    BOOST_TEST(argc >= 3);
    paths_t paths = generate_paths(argc, argv);
    BOOST_TEST(!paths.empty());

    std::cout << "Libraries:\n\t";
    std::copy(paths.begin(), paths.end(), std::ostream_iterator<boost::dll::fs::path>(std::cout, ", "));
    std::cout << std::endl;

    constexpr std::size_t threads_count =
#ifdef BOOST_TRAVISCI_BUILD
        1
#else
        4
#endif
    ;

    simple_barrier barrier{threads_count};
    std::thread threads[threads_count];
    for (auto& t: threads) {
        t = std::thread(load_unload, paths, 1000, std::ref(barrier));
    }

    for (auto& t: threads) {
        t.join();
    }
    return boost::report_errors();
}

