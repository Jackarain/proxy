/* Copyright 2026 Joaquin M Lopez Munoz.
 * Copyright 2026 Ion Gaztanaga.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <boost/config.hpp>

/* This test exercises exception safety, so it is meaningless (and would not
 * even compile, as it relies on try/catch and throw) when exceptions are
 * disabled.
 */
#if BOOST_CXX_VERSION < 201103L || defined(BOOST_NO_EXCEPTIONS)

int main() { return 0; }

#else

#include <algorithm>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <boost/container/hub.hpp>
#include <boost/core/lightweight_test.hpp>
#include <climits>
#include <iterator>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include "hub_utility.hpp"

template<typename T, typename Allocator>
void check_valid(const boost::container::hub<T, Allocator>& h)
{
  BOOST_TEST_GE(h.capacity(), h.size());
  BOOST_TEST_EQ((std::size_t)std::distance(h.begin(), h.end()), h.size());
  for(const auto& x: h) check_valid(x);
}

template<typename Container1, typename Container2>
void check_equal(const Container1& x, const Container2& y)
{
  auto first1 = x.begin(), last1 = x.end();
  auto first2 = y.begin(), last2 = y.end();
  while(first1 != last1 && first2 != last2) {
    BOOST_TEST(*first1++ == *first2++);
  }
  BOOST_TEST(first1 == last1);
  BOOST_TEST(first2 == last2);
}

template<typename T, typename Allocator>
void fill_till_capacity(boost::container::hub<T, Allocator>& h)
{
  using value_type = typename boost::container::hub<T, Allocator>::value_type;

  while(h.size() < h.capacity()) h.insert(value_type{0});
}

template<typename ThrowingHub, typename F>
void test_basic_exception_safety(ThrowingHub& h, F f)
{
  try {
    f();
    BOOST_ERROR("Expected exception was not thrown");
    ThrowingHub::value_type::countdown_to_throw(0);
    ThrowingHub::allocator_type::countdown_to_throw(0);
  }
  catch(...) {
    check_valid(h);
  }
}

template<typename ThrowingHub, typename F, typename... Fs>
void test_basic_exception_safety(ThrowingHub& h, F f, Fs... fs)
{
  test_basic_exception_safety(h, f);
  test_basic_exception_safety(h, fs...);
}

template<typename ThrowingHub, typename F>
void test_strong_exception_safety(ThrowingHub& h, F f)
{
  auto c = h.capacity();
  std::vector<int> backup{h.begin(), h.end()};
  try { 
    f(); 
    BOOST_ERROR("Expected exception was not thrown");
    ThrowingHub::value_type::countdown_to_throw(0);
    ThrowingHub::allocator_type::countdown_to_throw(0);
  }
  catch(...) {
    check_valid(h);
    BOOST_TEST_EQ(h.capacity(), c);
    check_equal(h, backup);
  }
}

template<typename ThrowingHub, typename F, typename... Fs>
void test_strong_exception_safety(ThrowingHub& h, F f, Fs... fs)
{
  test_strong_exception_safety(h, f);
  test_strong_exception_safety(h, fs...);
}

/* Sentinel exception used for fault injection. It deliberately carries no
 * heap-allocated message: std::runtime_error stores its message in a
 * refcounted buffer allocated with operator new but, on some libc++/libc++abi
 * combinations, freed with free() in the destructor, which AddressSanitizer
 * flags as an alloc-dealloc-mismatch when the caught exception is destroyed.
 * An empty type sidesteps that entirely.
 */
struct injected_exception {};

struct throwing_int
{
  throwing_int(int n_ = 0) { maybe_throw(); n = n_; }
  throwing_int(const throwing_int& x) { maybe_throw(); n = x.n; }
  throwing_int& operator=(const throwing_int& x)
    { maybe_throw(); n = x.n; return *this; }
  ~throwing_int() { n = INT_MIN; }

  throwing_int& operator+=(int m) { n += m; return *this; }

  operator int() const { return n; }
  bool operator==(int n_) const { return n == n_; }
  bool operator<(int n_) const { return n < n_; }

  static void countdown_to_throw(int n) { countdown = n; }

  struct no_dangling_objects_guard
  {
    ~no_dangling_objects_guard()
    {  
      BOOST_TEST_EQ(outstanding_objects, n);
    }

    std::size_t n;
  };

  static no_dangling_objects_guard check_no_dangling_objects_on_exit()
  {
    return {outstanding_objects};
  }

private:
  static int         countdown;
  static std::size_t outstanding_objects;

  static void maybe_throw() 
  { 
    if(countdown && !--countdown) throw injected_exception{};
  }

  friend void check_valid(const throwing_int& x) 
  {
    BOOST_TEST_NE(x.n, INT_MIN);
  }

  int n = INT_MIN;
};

int         throwing_int::countdown = 0;
std::size_t throwing_int::outstanding_objects = 0;

template<typename Class, std::size_t ExtraSpace>
struct make_bigger: Class
{
  make_bigger(const Class& x): Class{x} {}

  unsigned char extra_space[ExtraSpace] = {};
};

int         throwing_allocator_countdown = 0;
std::size_t throwing_allocator_outstanding_allocations = 0;

struct throwing_allocator_no_leaks_guard
{
  ~throwing_allocator_no_leaks_guard()
  {  
    BOOST_TEST_EQ(throwing_allocator_outstanding_allocations, n);
  }

  std::size_t n;
};

template<
  typename T,
  typename Propagate = std::false_type, typename AlwaysEqual = std::false_type
>
struct throwing_allocator
{
  using value_type = T;
  using propagate_on_container_copy_assignment = Propagate;
  using propagate_on_container_move_assignment = Propagate;
  using propagate_on_container_swap = Propagate;
  using is_always_equal = AlwaysEqual;

  template<typename U>
  struct rebind
  {
    using other = throwing_allocator<U, Propagate, AlwaysEqual>;
  };

  throwing_allocator(int state_ = 0): state{state_} {}

  template<typename U>
  throwing_allocator(const throwing_allocator<U, Propagate, AlwaysEqual>& x):
    state{x.state} {}

  T* allocate(std::size_t n)
  {
    maybe_throw();
    auto p = static_cast<T*>(::operator new(n * sizeof(T)));
    ++throwing_allocator_outstanding_allocations;
    return p;
  }

  void deallocate(T* p, std::size_t)
  { 
    --throwing_allocator_outstanding_allocations;
    ::operator delete(p);
  }

  bool operator==(const throwing_allocator& x) const
  {
    return AlwaysEqual::value || (state == x.state);
  }

  bool operator!=(const throwing_allocator& x) const { return !(*this == x); }

  static void countdown_to_throw(int n) { throwing_allocator_countdown = n; }

  static throwing_allocator_no_leaks_guard check_no_leaks_on_exit()
  {
    return {throwing_allocator_outstanding_allocations};
  }

  int state;

private:
  static void maybe_throw() 
  { 
    if(throwing_allocator_countdown && !--throwing_allocator_countdown) {
      throw injected_exception{};
    }
  }
};

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4127) /* conditional expression is constant */
#endif

#if BOOST_WORKAROUND(BOOST_GCC, >= 60000 && BOOST_GCC < 80000)
/* https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80947 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif

template<typename Hub, typename Propagate, typename AlwaysEqual, typename Data>
void test_allocator_ops(const Data& original_hubs)
{
  using hub = rebind_allocator_t<
    Hub, throwing_allocator<void, Propagate,AlwaysEqual>>;
  using value_type = typename hub::value_type;
  using allocator_type = typename hub::allocator_type;

  std::vector<hub> hubs;
  for(const auto& oh: original_hubs) {
    if(!oh.empty()) hubs.emplace_back(oh.begin(), oh.end());
  }

  BOOST_LIGHTWEIGHT_TEST_OSTREAM 
  << "Allocator propagate: " << Propagate::value 
  << ", always_equal: " << AlwaysEqual::value << "\n";

  BOOST_LIGHTWEIGHT_TEST_OSTREAM << "  Copy/move ctors, value_type throws\n";
  for(const auto& ch: hubs) {
    auto guard0 = allocator_type::check_no_leaks_on_exit();
    auto guard1 = value_type::check_no_dangling_objects_on_exit();

    value_type::countdown_to_throw((int)(ch.size() / 2));
    BOOST_TEST_THROWS((void)hub(ch), injected_exception);

    if(!AlwaysEqual::value) {
      auto h = ch;
      test_basic_exception_safety(h,
        [&h] {
          value_type::countdown_to_throw((int)(h.size() / 2));
          (void)hub{std::move(h), allocator_type{1}};
        });
    }
  }

  BOOST_LIGHTWEIGHT_TEST_OSTREAM <<
  "  Copy/move ctors, allocator_type throws\n";
  for(const auto& ch: hubs) {
    auto guard0 = allocator_type::check_no_leaks_on_exit();
    auto guard1 = value_type::check_no_dangling_objects_on_exit();

    allocator_type::countdown_to_throw((int)(ch.size() / 64 * 2) + 1);
    BOOST_TEST_THROWS((void)hub(ch), injected_exception);
    allocator_type::countdown_to_throw((int)(ch.size() / 64 * 2) + 2);
    BOOST_TEST_THROWS((void)hub(ch), injected_exception);

    if(!AlwaysEqual::value) {
      auto h = ch;
      test_basic_exception_safety(h,
        [&h] {
          allocator_type::countdown_to_throw((int)(h.size() / 64 * 2) + 1);
          (void)hub{std::move(h), allocator_type{1}};
        });

      h = ch;
      test_basic_exception_safety(h,
        [&h] {
          allocator_type::countdown_to_throw((int)(h.size() / 64 * 2) + 2);
          (void)hub{std::move(h), allocator_type{1}};
        });
    }
  }

  BOOST_LIGHTWEIGHT_TEST_OSTREAM << 
  "  Copy/move assignment, value_type throws\n";
  for(const auto& ch: hubs) {
    auto guard0 = allocator_type::check_no_leaks_on_exit();
    auto guard1 = value_type::check_no_dangling_objects_on_exit();

    hub dst0{allocator_type{1}};
    test_basic_exception_safety(dst0,
      [&dst0, &ch] {
        value_type::countdown_to_throw((int)(ch.size() / 2));
        dst0 = ch;
      });

    if(!AlwaysEqual::value && !Propagate::value) {
      hub src1{ch}, dst1{allocator_type{1}};
      test_basic_exception_safety(src1,
        [&src1, &dst1] {
          test_basic_exception_safety(dst1,
            [&src1, &dst1] {
              value_type::countdown_to_throw((int)(src1.size() / 2));
              dst1 = std::move(src1);
            });
          throw injected_exception{};
        });
    }
  }

  BOOST_LIGHTWEIGHT_TEST_OSTREAM << 
  "  Copy/move assignment, allocator_type throws\n";
  for(const auto& ch: hubs) {
    auto guard0 = allocator_type::check_no_leaks_on_exit();
    auto guard1 = value_type::check_no_dangling_objects_on_exit();

    hub dst0{allocator_type{1}};
    test_basic_exception_safety(dst0,
      [&dst0, &ch] {
        allocator_type::countdown_to_throw((int)(ch.size() / 64 * 2) + 1);
        dst0 = ch;
      });

    hub dst1{allocator_type{1}};
    test_basic_exception_safety(dst1,
      [&dst1, &ch] {
        allocator_type::countdown_to_throw((int)(ch.size() / 64 * 2) + 2);
        dst1 = ch;
      });

    if(!AlwaysEqual::value && !Propagate::value) {
      hub src2{ch}, dst2{allocator_type{1}};
      test_basic_exception_safety(src2,
        [&src2, &dst2] {
          test_basic_exception_safety(dst2,
            [&src2, &dst2] {
              allocator_type::countdown_to_throw(
                (int)(src2.size() / 64 * 2) + 1);
              dst2 = std::move(src2);
            });
          throw injected_exception{};
        });

      hub src3{ch}, dst3{allocator_type{1}};
      test_basic_exception_safety(src3,
        [&src3, &dst3] {
          test_basic_exception_safety(dst3,
            [&src3, &dst3] {
              allocator_type::countdown_to_throw(
                (int)(src3.size() / 64 * 2) + 2);
              dst3 = std::move(src3);
            });
          throw injected_exception{};
        });
    }
  }
}

#if BOOST_WORKAROUND(BOOST_GCC, >= 60000 && BOOST_GCC < 80000)
#pragma GCC diagnostic pop
#endif

#if defined(BOOST_MSVC)
#pragma warning(pop) /* C4127 */
#endif

template<typename Hub>
void test()
{
  using value_type = typename Hub::value_type;
  using allocator_type = typename Hub::allocator_type;

  std::vector<Hub> hubs;
  hubs.emplace_back();
  hubs.emplace_back(Hub{0, 2, 1});
  hubs.emplace_back(Hub{64, 5}); /* capacity() - size() == 0 */
  hubs.emplace_back([] {
    Hub h;
    for(int i = 0; i < 1000; ++i) h.insert(-i);
    puncture(h);
    return h;
  }());

  BOOST_LIGHTWEIGHT_TEST_OSTREAM << 
  "Non copy/move ctors and assignment, value_type throws\n";
  {
    auto guard0 = allocator_type::check_no_leaks_on_exit();
    auto guard1 = value_type::check_no_dangling_objects_on_exit();
    
    value_type::countdown_to_throw(100);
    BOOST_TEST_THROWS((void)Hub(200), injected_exception);

    value_type::countdown_to_throw(100);
    BOOST_TEST_THROWS((void)Hub(200, value_type{42}), injected_exception);

    auto rng = make_range<value_type>(200);
    value_type::countdown_to_throw(100);
    BOOST_TEST_THROWS((void)Hub(rng.begin(), rng.end()), injected_exception);

#if !defined(BOOST_CONTAINER_HUB_NO_RANGES)
    value_type::countdown_to_throw(100);
    BOOST_TEST_THROWS(
      (void)Hub(boost::container::from_range, rng), injected_exception);
#endif

    std::initializer_list<value_type> il = {0, 1, 2, 3};
    value_type::countdown_to_throw(2);
    BOOST_TEST_THROWS((void)Hub(il), injected_exception);

    Hub h0;
    test_basic_exception_safety(h0, 
      [&h0]{
        std::initializer_list<value_type> il0 = {0, 1, 2, 3};
        value_type::countdown_to_throw(2);
        h0 = il0;
      });
  }

  BOOST_LIGHTWEIGHT_TEST_OSTREAM <<
  "Non copy/move ctors and assignment, allocator_type throws\n";
  {
    auto guard0 = allocator_type::check_no_leaks_on_exit();
    auto guard1 = value_type::check_no_dangling_objects_on_exit();
    
    allocator_type::countdown_to_throw(3);
    BOOST_TEST_THROWS((void)Hub(200), injected_exception);
    allocator_type::countdown_to_throw(4);
    BOOST_TEST_THROWS((void)Hub(200), injected_exception);

    allocator_type::countdown_to_throw(3);
    BOOST_TEST_THROWS((void)Hub(200, value_type{42}), injected_exception);
    allocator_type::countdown_to_throw(4);
    BOOST_TEST_THROWS((void)Hub(200, value_type{42}), injected_exception);

    auto rng = make_range<value_type>(200);
    allocator_type::countdown_to_throw(3);
    BOOST_TEST_THROWS((void)Hub(rng.begin(), rng.end()), injected_exception);
    allocator_type::countdown_to_throw(4);
    BOOST_TEST_THROWS((void)Hub(rng.begin(), rng.end()), injected_exception);

#if !defined(BOOST_CONTAINER_HUB_NO_RANGES)
    allocator_type::countdown_to_throw(3);
    BOOST_TEST_THROWS(
      (void)Hub(boost::container::from_range, rng), injected_exception);
    allocator_type::countdown_to_throw(4);
    BOOST_TEST_THROWS(
      (void)Hub(boost::container::from_range, rng), injected_exception);
#endif

    std::initializer_list<value_type> il = {0, 1, 2, 3};
    allocator_type::countdown_to_throw(1);
    BOOST_TEST_THROWS((void)Hub(il), injected_exception);
    allocator_type::countdown_to_throw(2);
    BOOST_TEST_THROWS((void)Hub(il), injected_exception);

    Hub h0;
    test_basic_exception_safety(h0, 
      [&h0]{
        std::initializer_list<value_type> il0 = {0, 1, 2, 3};
        allocator_type::countdown_to_throw(1);
        h0 = il0;
      });

    Hub h1;
    test_basic_exception_safety(h1, 
      [&h1]{
        std::initializer_list<value_type> il1 = {0, 1, 2, 3};
        allocator_type::countdown_to_throw(2);
        h1 = il1;
      });
  }

  test_allocator_ops<Hub, std::false_type, std::false_type>(hubs);
  test_allocator_ops<Hub, std::false_type, std::true_type >(hubs);
  test_allocator_ops<Hub, std::true_type,  std::false_type>(hubs);
  test_allocator_ops<Hub, std::true_type,  std::true_type >(hubs);

  BOOST_LIGHTWEIGHT_TEST_OSTREAM << "assign[_range], value_type throws\n";
  for(const auto& ch: hubs) {
    auto guard0 = allocator_type::check_no_leaks_on_exit();
    auto guard1 = value_type::check_no_dangling_objects_on_exit();
    auto h = ch;

    test_basic_exception_safety(h,
      [&h] {
        auto rng = make_range<value_type>(200);
        value_type::countdown_to_throw(100);
        h.assign(rng.begin(), rng.end());
      },
#if !defined(BOOST_CONTAINER_HUB_NO_RANGES)
      [&h] {
        auto rng = make_range<value_type>(200);
        value_type::countdown_to_throw(100);
        h.assign_range(rng);
      },
#endif
      [&h] {
        std::initializer_list<value_type> il = {0, 1, 2, 3};
        value_type::countdown_to_throw(2);
        h.assign(il);
      });
  }

  BOOST_LIGHTWEIGHT_TEST_OSTREAM << "assign[_range], allocator_type throws\n";
  for(const auto& ch: hubs) {
    auto guard0 = allocator_type::check_no_leaks_on_exit();
    auto guard1 = value_type::check_no_dangling_objects_on_exit();
    auto h = ch;

    test_basic_exception_safety(h,
      [&h] {
        auto rng = make_range<value_type>(h.capacity() + 200);
        allocator_type::countdown_to_throw(3);
        h.assign(rng.begin(), rng.end());
      },
      [&h] {
        auto rng = make_range<value_type>(h.capacity() + 200);
        allocator_type::countdown_to_throw(4);
        h.assign(rng.begin(), rng.end());
      },
#if !defined(BOOST_CONTAINER_HUB_NO_RANGES)
      [&h] {
        auto rng = make_range<value_type>(h.capacity() + 200);
        allocator_type::countdown_to_throw(3);
        h.assign_range(rng);
      },
      [&h] {
        auto rng = make_range<value_type>(h.capacity() + 200);
        allocator_type::countdown_to_throw(4);
        h.assign_range(rng);
      },
#endif
      [&h] {
        h.clear();
        h.shrink_to_fit();
        std::initializer_list<value_type> il = {0, 1, 2, 3};
        allocator_type::countdown_to_throw(1);
        h.assign(il);
      },
      [&h] {
        h.clear();
        h.shrink_to_fit();
        std::initializer_list<value_type> il = {0, 1, 2, 3};
        allocator_type::countdown_to_throw(2);
        h.assign(il);
      });
  }

  BOOST_LIGHTWEIGHT_TEST_OSTREAM << "reserve, allocator_type throws\n";
  for(const auto& ch: hubs) {
    auto guard0 = allocator_type::check_no_leaks_on_exit();
    auto guard1 = value_type::check_no_dangling_objects_on_exit();
    auto h = ch;

    test_basic_exception_safety(h,
      [&h] {
        allocator_type::countdown_to_throw(3);
        h.reserve(h.capacity() + 200);
      },
      [&h] {
        allocator_type::countdown_to_throw(4);
        h.reserve(h.capacity() + 200);
      });
  }

  BOOST_LIGHTWEIGHT_TEST_OSTREAM << "shrink_to_fit, value_type throws\n";
  for(const auto& ch: hubs) {
    auto guard0 = allocator_type::check_no_leaks_on_exit();
    auto guard1 = value_type::check_no_dangling_objects_on_exit();
    auto h = ch;

    test_basic_exception_safety(h, [&h] {
      value_type::countdown_to_throw(10);
      h.shrink_to_fit(); /* may not throw depending on h */
      value_type::countdown_to_throw(0);
      throw injected_exception{};
    });
  }

  BOOST_LIGHTWEIGHT_TEST_OSTREAM << "emplace/insert, value_type throws\n";
  for(const auto& ch: hubs) {
    auto guard0 = allocator_type::check_no_leaks_on_exit();
    auto guard1 = value_type::check_no_dangling_objects_on_exit();
    auto h = ch;

    test_strong_exception_safety(h,
      [&h] {
        value_type::countdown_to_throw(1);
        h.emplace(3);
      },
      [&h] {
        value_type::countdown_to_throw(1);
        h.emplace_hint(h.end(), 3);
      },
      [&h] {
        value_type::countdown_to_throw(2);
        h.insert(3);
      },
      [&h] {
        value_type::countdown_to_throw(2);
        auto x = value_type{3};
        h.insert(std::move(x));
      },
      [&h] {
        value_type::countdown_to_throw(2);
        h.insert(h.begin(), 3);
      });

    test_basic_exception_safety(h,
      [&h] {
        value_type::countdown_to_throw(2);
        auto x = value_type{3};
        h.insert(h.begin(), std::move(x));
      },
      [&h] {
        std::initializer_list<value_type> il = {0, 1, 2};
        value_type::countdown_to_throw(2);
        h.insert(il);
      },
#if !defined(BOOST_CONTAINER_HUB_NO_RANGES)
      [&h] {
        std::vector<value_type> v = {0, 1, 2};
        value_type::countdown_to_throw(2);
        h.insert_range(v);
      },
#endif
      [&h] {
        std::vector<value_type> v = {0, 1, 2};
        value_type::countdown_to_throw(2);
        h.insert(v.begin(), v.end());
      },
      [&h] {
        value_type::countdown_to_throw(50);
        h.insert(100, value_type{42});
      });
  }

  BOOST_LIGHTWEIGHT_TEST_OSTREAM << "emplace/insert, allocator_type throws\n";
  for(const auto& ch: hubs) {
    auto guard0 = allocator_type::check_no_leaks_on_exit();
    auto guard1 = value_type::check_no_dangling_objects_on_exit();
    auto h = ch;
    fill_till_capacity(h);

    test_strong_exception_safety(h,
      [&h] {
        allocator_type::countdown_to_throw(1);
        h.emplace(3);
      },
      [&h] {
        allocator_type::countdown_to_throw(2);
        h.emplace(3);
      },
      [&h] {
        allocator_type::countdown_to_throw(1);
        h.emplace_hint(h.end(), 3);
      },
      [&h] {
        allocator_type::countdown_to_throw(2);
        h.emplace_hint(h.end(), 3);
      },
      [&h] {
        allocator_type::countdown_to_throw(1);
        h.insert(3);
      },
      [&h] {
        allocator_type::countdown_to_throw(2);
        h.insert(3);
      },
      [&h] {
        allocator_type::countdown_to_throw(1);
        auto x = value_type{3};
        h.insert(std::move(x));
      },
      [&h] {
        allocator_type::countdown_to_throw(2);
        auto x = value_type{3};
        h.insert(std::move(x));
      },
      [&h] {
        allocator_type::countdown_to_throw(1);
        h.insert(h.begin(), 3);
      },
      [&h] {
        allocator_type::countdown_to_throw(2);
        h.insert(h.begin(), 3);
      },
      [&h] {
        allocator_type::countdown_to_throw(1);
        auto x = value_type{3};
        h.insert(h.begin(), std::move(x));
      },
      [&h] {
        allocator_type::countdown_to_throw(2);
        auto x = value_type{3};
        h.insert(h.begin(), std::move(x));
      });

    test_basic_exception_safety(h,
      [&h] {
        std::initializer_list<value_type> il = {0, 1, 2};
        allocator_type::countdown_to_throw(1);
        h.insert(il);
      },
      [&h] {
        std::initializer_list<value_type> il = {0, 1, 2};
        allocator_type::countdown_to_throw(2);
        h.insert(il);
      },
#if !defined(BOOST_CONTAINER_HUB_NO_RANGES)
      [&h] {
        std::vector<value_type> v = {0, 1, 2};
        allocator_type::countdown_to_throw(1);
        h.insert_range(v);
      },
      [&h] {
        std::vector<value_type> v = {0, 1, 2};
        allocator_type::countdown_to_throw(2);
        h.insert_range(v);
      },
#endif
      [&h] {
        std::vector<value_type> v = {0, 1, 2};
        allocator_type::countdown_to_throw(1);
        h.insert(v.begin(), v.end());
      },
      [&h] {
        std::vector<value_type> v = {0, 1, 2};
        allocator_type::countdown_to_throw(2);
        h.insert(v.begin(), v.end());
      },
      [&h] {
        allocator_type::countdown_to_throw(3);
        h.insert(100, value_type{42});
      },
      [&h] {
        allocator_type::countdown_to_throw(4);
        h.insert(100, value_type{42});
      });
  }

  BOOST_LIGHTWEIGHT_TEST_OSTREAM << "sort, value_type throws\n";
  for(const auto& ch: hubs) {
    if(std::is_sorted(ch.begin(), ch.end())) continue;
    auto guard0 = allocator_type::check_no_leaks_on_exit();
    auto guard1 = value_type::check_no_dangling_objects_on_exit();

    /* transfer_sort */
    auto h0 = ch;
    test_basic_exception_safety(h0,
      [&h0] {
        value_type::countdown_to_throw((int)(h0.size()/2));
        h0.sort();
      });

    /* proxy_sort */
    using bigger_element_hub = 
      rebind_value_type_t<Hub, make_bigger<value_type, 128>>;
     
    bigger_element_hub h1{ch.begin(), ch.end()};
    test_basic_exception_safety(h1,
      [&h1] {
        value_type::countdown_to_throw(1);
        h1.sort();
      });

    /* compact_sort */
    bigger_element_hub h2;
    while(h2.size() < 2 * 1024 * 1024 / sizeof(void*)) {
      h2.insert(ch.begin(), ch.end());
    }
    test_basic_exception_safety(h2,
      [&h2] {
        value_type::countdown_to_throw((int)(h2.size() / 2));
        h2.sort();
      });
  }
}

int main()
{
  test<
    boost::container::hub<throwing_int, throwing_allocator<throwing_int>>>();

  return boost::report_errors();
}

#endif
