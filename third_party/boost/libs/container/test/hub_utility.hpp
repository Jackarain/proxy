/* Copyright 2025-2026 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#ifndef BOOST_HUB_TEST_UTILITY_HPP
#define BOOST_HUB_TEST_UTILITY_HPP

#include <cstddef>
#include <memory>
#include <new>
#include <vector>
#include <type_traits>
#include <boost/container/allocator_traits.hpp>

template<typename T>
std::vector<T> make_range(std::size_t n)
{
  std::vector<T> res;
  T i = T();
  while(n--) {
    res.push_back(i);
    i += T(1);
  }
  return res;
}

struct null_callback
{
  template<typename T>
  void operator()(const T&) const {}
};

template<typename Container, typename EraseCallback = null_callback>
void puncture(Container& x, EraseCallback callback = EraseCallback())
{
  for(auto first = x.begin(); first != x.end(); ) {
    if(!(*first % 7)) {
      callback(first);
      first = x.erase(first);
    }
    else ++first;
  }
}

template<typename Hub, typename U>
struct rebind_value_type;

template<
  template<typename...> class Hub, typename T, typename Allocator,
  typename U
>
struct rebind_value_type<Hub<T, Allocator>, U>
{
  //Use the container's resolved allocator_type rather than the Allocator
  //template argument, which may be void (the default that selects new_allocator).
  using type = Hub<
    U,
    typename boost::container::allocator_traits<
      typename Hub<T, Allocator>::allocator_type>::
      template portable_rebind_alloc<U>::type>;
};

template<typename Hub, typename U>
using rebind_value_type_t = typename rebind_value_type<Hub, U>::type;

template<typename Hub, typename OtherAllocator>
struct rebind_allocator;

template<
  template<typename...> class Hub, typename T, typename Allocator,
  typename OtherAllocator
>
struct rebind_allocator<Hub<T, Allocator>, OtherAllocator>
{
  using type = Hub<
    T,
    typename boost::container::allocator_traits<OtherAllocator>::
      template portable_rebind_alloc<T>::type>;
};

template<typename Hub, typename OtherAllocator>
using rebind_allocator_t = 
  typename rebind_allocator<Hub, OtherAllocator>::type;

template<typename T> struct reference_or_void { using type = T&; };
template<> struct reference_or_void<void> { using type = void; };
template<> struct reference_or_void<const void> { using type = const void; };

template<
  typename T, 
  typename Propagate = std::false_type, typename AlwaysEqual = std::false_type
>
struct stateful_allocator
{
  using value_type = T;
  using propagate_on_container_copy_assignment = Propagate;
  using propagate_on_container_move_assignment = Propagate;
  using propagate_on_container_swap = Propagate;
  using is_always_equal = AlwaysEqual;

  /* typedefs and rebind required by not quite C++11-conformant
   * GCC < 5 stdlib.
   */
  using pointer = T*;
  using const_pointer = const T*;
  using void_pointer = void*;
  using reference = typename reference_or_void<T>::type;
  using const_reference = typename reference_or_void<const T>::type;
  using const_void_pointer = const void*;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;

  template<typename U>
  struct rebind
  {
    using other = stateful_allocator<U, Propagate, AlwaysEqual>;
  };

  stateful_allocator(int state_ = 0): state{state_} {}

  template<typename U>
  stateful_allocator(const stateful_allocator<U, Propagate, AlwaysEqual>& x):
    state{x.state}, num_allocations{x.num_allocations} {}

  T* allocate(std::size_t n)
  {
    auto p = static_cast<T*>(::operator new(n * sizeof(T)));
    ++num_allocations;
    return p;
  }

  void deallocate(T* p, std::size_t) { ::operator delete(p); }

  bool operator==(const stateful_allocator& x) const
  {
    return AlwaysEqual::value || (state == x.state);
  }

  bool operator!=(const stateful_allocator& x) const { return !(*this == x); }

  int state;
  int num_allocations = 0;
};

#endif
