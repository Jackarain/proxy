/* Copyright 2025-2026 Joaquin M Lopez Munoz.
 * Copyright 2026 Ion Gaztanaga.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <boost/config.hpp>

#if BOOST_CXX_VERSION < 201103L

int main() { return 0; }

#else

#include <algorithm>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <boost/container/hub.hpp>
#include <boost/core/lightweight_test.hpp>
#include <functional>
#include <iterator>
#include <memory>
#include <type_traits>
#include <vector>
#include "hub_utility.hpp"

struct tidy_int
{
  tidy_int(int n_ = 0): n{n_} {}
  ~tidy_int() { n = 0x0BADBEEF; }

  operator int() const { return n; }

  tidy_int& operator+=(const tidy_int& x)
  {
    n += x.n;
    return *this;
  }

  int n;
};

template<typename Hub>
using erase_callback = std::function<void(typename Hub::iterator)>;

template<typename Hub>
struct track_info
{
  typename Hub::iterator   it;
  typename Hub::pointer    pointer;
  typename Hub::value_type value;

  bool valid() const
  {
    return std::addressof(*it) == pointer && *it == value;
  }
};

template<typename Hub>
using track_info_vector = std::vector<track_info<Hub>>;

template<typename F, typename Arg>
void call_optionally_with_impl(F& f, Arg, ...) { f(); }

template<
  typename F, typename Arg,
  typename = typename std::enable_if<
    sizeof(std::declval<F&>()(std::declval<Arg>()), 0) != 0
  >::type
>
void call_optionally_with_impl(F& f, Arg arg, int) { f(arg); }

template<typename F, typename Arg>
void call_optionally_with(F& f, Arg arg) 
{
  call_optionally_with_impl(f, arg, 0); 
}

template<typename Hub>
void save_track_info(Hub& x, track_info_vector<Hub>& track)
{
  for(auto it = x.begin(); it != x.end(); ++it) {
    track.push_back(track_info<Hub>{it, std::addressof(*it), *it});
  }
}

#if BOOST_WORKAROUND(BOOST_GCC, < 70000)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif

template<typename Hub, typename F>
bool check_stability(F f, track_info_vector<Hub>&& track = {})
{
  using iterator = typename Hub::iterator;

  call_optionally_with(
    f,
    erase_callback<Hub>{[&] (iterator it) {
      track.erase(std::find_if(
        track.begin(), track.end(),
        [&] (const track_info<Hub>& info) { return info.it == it; }));
    }});
  for(const auto& info: track) if(!info.valid()) return false;
  return true;
}

#if BOOST_WORKAROUND(BOOST_GCC, < 70000)
#pragma GCC diagnostic pop
#endif

template<typename Hub, typename F>
bool check_stability(Hub& x, F f, track_info_vector<Hub>&& track = {})
{
  auto last = x.end();
  save_track_info(x, track);
  if(!check_stability<Hub>(f, std::move(track))) return false;
  return (x.end() == last);
}

template<typename Hub, typename F>
bool check_stability(Hub& x, Hub& y, F f, track_info_vector<Hub>&& track = {})
{
  auto last = x.end();
  save_track_info(x, track);
  if(!check_stability<Hub>(y, f, std::move(track))) return false;
  return (x.end() == last);
}

template<typename Hub>
void test()
{
  using value_type = typename Hub::value_type;
  using difference_type = typename Hub::difference_type;
  using erase_callback = ::erase_callback<Hub>;

  auto rng = make_range<value_type>(200);

  {
    Hub x(rng.begin(), rng.end());
    BOOST_TEST(check_stability(x, [&] (erase_callback callback) {
      puncture(x, callback);
      x.insert(rng.begin(), rng.end());
    }));
  }
  {
    Hub                  x(rng.begin(), rng.end());
    std::shared_ptr<Hub> py;
    BOOST_TEST(check_stability(x, [&] { 
      py = std::make_shared<Hub>(std::move(x)); 
    }));
  }
  {
    Hub x(rng.begin(), rng.end()), y;
    BOOST_TEST(check_stability(x, y, [&] { 
      y = std::move(x); 
    }));
  }
  {
    Hub x(rng.begin(), rng.end());
    BOOST_TEST(check_stability(x, [&] { 
      x.reserve(x.capacity() + 100); 
    }));
  }
  {
    Hub x(rng.begin(), rng.end());
    puncture(x);
    x.shrink_to_fit(); 
    BOOST_TEST(check_stability(x, [&] { 
      x.shrink_to_fit(); 
    }));
  }
  {
    Hub x(rng.begin(), rng.end());
    puncture(x);
    auto c = x.capacity();
    x.reserve(c + 1000);
    BOOST_TEST(check_stability(x, [&] { 
      x.trim_capacity(c+500); 
    }));
    BOOST_TEST(check_stability(x, [&] { 
      x.trim_capacity(); 
    }));
  }
  {
    Hub x(rng.begin(), rng.end());
    BOOST_TEST(check_stability(x, [&] (erase_callback callback) {
      callback(x.begin());
      x.erase(x.begin());
    }));
  }
  {
    Hub x(rng.begin(), rng.end());
    BOOST_TEST(check_stability(x, [&] (erase_callback callback) { 
      auto first = std::next(x.begin(), (difference_type)(x.size() / 3)),
           last = std::next(x.begin(), (difference_type)(x.size() * 2 / 3));
      for(auto it = first; it != last; ++it) callback(it);
      x.erase(first,last);
    }));
  }
  {
    Hub x(rng.begin(), rng.begin() + (difference_type)(rng.size() / 2)),
        y(rng.begin() + (difference_type)(rng.size() / 2), rng.end());
    BOOST_TEST(check_stability(x, y, [&] {
      x.swap(y); 
    }));
  }
  {
    Hub x(rng.begin(), rng.begin() + (difference_type)(rng.size() / 2)),
        y(rng.begin() + (difference_type)(rng.size() / 2), rng.end());
    BOOST_TEST(check_stability(x, y, [&] {
      x.splice(y); 
    }));
    BOOST_TEST(check_stability(x, y, [&] {
      y.splice(std::move(x)); 
    }));
  }
  {
    Hub x;
    x.insert(rng.begin(), rng.end());
    x.insert(rng.begin(), rng.end());
    x.sort();
    BOOST_TEST(check_stability(x, [&] (erase_callback callback) {
      for(auto it = x.begin(); it != x.end(); ) {
        auto next = std::next(it);
        while(next != x.end() && *next == *it) callback(next++);
        it = next;
      }
      x.unique(); 
    }));
  }
  {
    Hub x(rng.begin(), rng.end());
    auto is_even = [] (const value_type& v) { return v % 2 == 0; };
    BOOST_TEST(check_stability(x, [&] (erase_callback callback) {
      for(auto it = x.begin(); it != x.end(); ++it) {
        if(is_even(*it)) callback(it);
      }
      erase_if(x, is_even);
    }));
  }
}

int main()
{
  test<boost::container::hub<int>>();
  test<boost::container::hub<tidy_int>>();

  return boost::report_errors();
}

#endif
