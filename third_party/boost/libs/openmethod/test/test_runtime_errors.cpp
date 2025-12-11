// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod.hpp>
#include <boost/openmethod/preamble.hpp>
#include <boost/openmethod/policies/throw_error_handler.hpp>
#include <boost/openmethod/initialize.hpp>
#include <boost/openmethod/interop/std_shared_ptr.hpp>

#include "test_util.hpp"

#include <sstream>

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

using namespace boost::openmethod;
using namespace test_matrices;

struct capture_output : policies::output {
    template<class Registry>
    struct fn {
        inline static std::ostringstream os;
    };
};

template<int N>
struct errors_ : test_registry_<N, capture_output> {
    struct capture {
        using error_handler =
            typename test_registry_<N, capture_output>::error_handler;
        using output = typename test_registry_<N, capture_output>::output;

        capture() {
            prev = error_handler::set(
                [this](const typename errors_::error_handler::error_variant&
                           error) {
                    prev(error);
                    std::visit([](auto&& arg) { throw arg; }, error);
                });
        }

        ~capture() {
            error_handler::set(prev);
            output::os.clear();
        }

        auto operator()() const {
            return output::os.str();
        }

        typename errors_::error_handler::function_type prev;
    };
};

namespace TEST_NS {

struct registry : errors_<__COUNTER__> {};

BOOST_OPENMETHOD(
    transpose, (virtual_ptr<const matrix, registry>), void, registry);

BOOST_OPENMETHOD_OVERRIDE(
    transpose, (virtual_ptr<const diagonal_matrix, registry>), void) {
}

BOOST_AUTO_TEST_CASE(no_initialization) {
    if constexpr (registry::has_runtime_checks) {
        // throw during virtual_ptr construction, because of hash table lookup
        {
            registry::capture capture;
            BOOST_CHECK_THROW(
                (shared_virtual_ptr<matrix, registry>{
                    std::make_shared<diagonal_matrix>()}),
                not_initialized);
            BOOST_TEST(capture() == "not initialized\n");
        }

        // throw during method call
        {
            registry::capture capture;
            BOOST_CHECK_THROW(
                transpose(make_shared_virtual<diagonal_matrix, registry>()),
                not_initialized);
        }
    } else {
        try {
            registry::require_initialized();
        } catch (not_initialized&) {
            BOOST_TEST_FAIL("should not have thrown in release variant");
        }
    }
}

} // namespace TEST_NS

namespace TEST_NS {

using registry = errors_<__COUNTER__>;

BOOST_OPENMETHOD(
    transpose, (virtual_ptr<const matrix, registry>), void, registry);

// without any overrider initialize() would do nothing
BOOST_OPENMETHOD_OVERRIDE(
    transpose, (virtual_ptr<const matrix, registry>), void) {
}

BOOST_AUTO_TEST_CASE(initialize_unknown_class) {
    if constexpr (registry::has_runtime_checks) {
        {
            registry::capture capture;
            BOOST_CHECK_THROW(initialize<registry>(), missing_class);
            BOOST_TEST(capture().find("unknown class") != std::string::npos);
        }
    }
}

} // namespace TEST_NS

namespace TEST_NS {

using registry = errors_<__COUNTER__>;

// don't register dense_matrix
BOOST_OPENMETHOD_CLASSES(matrix, diagonal_matrix, registry);

BOOST_OPENMETHOD(transpose, (virtual_<const matrix&>), void, registry);

// without any overrider initialize() would do nothing
BOOST_OPENMETHOD_OVERRIDE(transpose, (const matrix&), void) {
}

BOOST_AUTO_TEST_CASE(call_unknown_class) {
    if constexpr (registry::has_runtime_checks) {
        {
            initialize<registry>();

            registry::capture capture;
            BOOST_CHECK_THROW(transpose(dense_matrix()), missing_class);
            BOOST_TEST(capture().find("unknown class") != std::string::npos);
        }
    }
}

} // namespace TEST_NS

namespace TEST_NS {

using namespace test_matrices;
using registry = errors_<__COUNTER__>;

BOOST_OPENMETHOD_CLASSES(matrix, dense_matrix, diagonal_matrix, registry);

BOOST_OPENMETHOD(
    times,
    (virtual_ptr<const matrix, registry>, virtual_ptr<const matrix, registry>),
    void, registry);

BOOST_OPENMETHOD_OVERRIDE(
    times,
    (virtual_ptr<const matrix, registry>,
     virtual_ptr<const diagonal_matrix, registry>),
    void) {
}

BOOST_OPENMETHOD_OVERRIDE(
    times,
    (virtual_ptr<const diagonal_matrix, registry>,
     virtual_ptr<const matrix, registry>),
    void) {
}

BOOST_AUTO_TEST_CASE(bad_call) {
    auto report = initialize<registry>().report;
    BOOST_TEST(report.not_implemented == 1u);
    BOOST_TEST(report.ambiguous == 1u);

    {
        registry::capture capture;
        matrix a, b;
        BOOST_CHECK_THROW(times(a, b), no_overrider);
        BOOST_TEST(capture().find("not implemented") != std::string::npos);
    }

    {
        registry::capture capture;
        diagonal_matrix a, b;
        BOOST_CHECK_THROW(times(a, b), ambiguous_call);
        BOOST_TEST(capture().find("ambiguous") != std::string::npos);
    }
}

BOOST_AUTO_TEST_CASE(bad_call_type_ids) {
    initialize<registry>();
    registry::capture capture;

    try {
        diagonal_matrix a, b;
        times(a, b);
        BOOST_FAIL("should have thrown");
    } catch (const ambiguous_call& error) {
        BOOST_TEST(error.arity == 2u);
        BOOST_TEST(error.types[0] == &typeid(diagonal_matrix));
        BOOST_TEST(error.types[1] == &typeid(diagonal_matrix));
    } catch (...) {
        BOOST_FAIL("wrong exception");
    }
}

} // namespace TEST_NS

namespace TEST_NS {

using namespace test_matrices;
using registry = errors_<__COUNTER__>;

BOOST_OPENMETHOD_CLASSES(matrix, dense_matrix, diagonal_matrix, registry);

BOOST_OPENMETHOD(
    times,
    (shared_virtual_ptr<const matrix, registry>,
     shared_virtual_ptr<const matrix, registry>),
    void, registry);

BOOST_AUTO_TEST_CASE(bad_call_type_ids_smart_ptr) {
    initialize<registry>();
    registry::capture capture;

    try {
        auto a = make_shared_virtual<const diagonal_matrix, registry>();
        auto b = make_shared_virtual<const diagonal_matrix, registry>();
        times(a, b);
        BOOST_FAIL("should have thrown");
    } catch (const no_overrider& error) {
        BOOST_TEST(error.arity == 2u);
        BOOST_TEST(error.types[0] == &typeid(diagonal_matrix));
        BOOST_TEST(error.types[1] == &typeid(diagonal_matrix));
    } catch (...) {
        BOOST_FAIL("wrong exception");
    }
}

} // namespace TEST_NS

namespace TEST_NS {

using namespace test_matrices;
struct registry
    : test_registry_<__COUNTER__>::with<policies::throw_error_handler> {};

BOOST_OPENMETHOD_CLASSES(matrix, dense_matrix, diagonal_matrix, registry);

BOOST_OPENMETHOD(
    times, (virtual_<const matrix&>, virtual_<const matrix&>), void, registry);

BOOST_AUTO_TEST_CASE(throw_error) {
    initialize<registry>();

    try {
        times(matrix(), matrix());
        BOOST_FAIL("should have thrown");
    } catch (const std::runtime_error& error) {
        BOOST_TEST(
            std::string(error.what()).find("not implemented") !=
            std::string::npos);
    } catch (...) {
        BOOST_FAIL("wrong exception");
    }
}

} // namespace TEST_NS
