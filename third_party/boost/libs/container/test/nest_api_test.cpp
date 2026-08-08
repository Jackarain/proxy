//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//
// C++03-compatible port of the hub test_api.cpp adapted to exercise
// boost::container::nest (API-compatible with boost::container::hub).
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/nest.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/pointer_traits.hpp>
#include <boost/move/core.hpp>
#include <boost/move/utility_core.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace boost::container;

//////////////////////////////////////////////////////////////////////////////
//
//                               Utilities
//
//////////////////////////////////////////////////////////////////////////////

template<class T>
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

//! Erases every element whose underlying integer value is a multiple of 7,
//! so the container ends up with a non-trivial punctured bitmask pattern.
template<class Container>
void puncture(Container& x)
{
   typedef typename Container::iterator iterator;
   for(iterator first = x.begin(); first != x.end(); ) {
      if(!((int)(*first) % 7)) {
         first = x.erase(first);
      }
      else {
         ++first;
      }
   }
}

template<class Container1, class Container2>
void test_equal(const Container1& x, const Container2& y)
{
   BOOST_TEST_EQ(x.size(), (typename Container1::size_type)y.size());
   BOOST_TEST(std::equal(x.begin(), x.end(), y.begin()));
}

template<class Iterator, class Mirror>
void test_traversal(Iterator first, Iterator last, const Mirror& data)
{
   std::size_t n = 0;
   for(Iterator it = first; it != last; ++it, ++n) {
      BOOST_TEST(*it == data[n]);
      BOOST_TEST((first == it) == (0 == n));
      BOOST_TEST((first != it) == (0 != n));

      Iterator it1 = it;
      Iterator it2 = ++it1;
      Iterator it3 = --it2;
      Iterator it4 = it3++;
      Iterator it5 = it3--;
      Iterator it_next = it; ++it_next;
      BOOST_TEST(it1 == it_next);
      BOOST_TEST(it2 == it);
      BOOST_TEST(it3 == it);
      BOOST_TEST(it4 == it);
      BOOST_TEST(it5 == it_next);
   }
}

template<class FwdIt>
bool is_sorted_cxx03(FwdIt first, FwdIt last)
{
   if(first == last) return true;
   FwdIt next = first;
   ++next;
   for(; next != last; ++first, ++next) {
      if(*next < *first) return false;
   }
   return true;
}

template<class FwdIt, class Compare>
bool is_sorted_cxx03(FwdIt first, FwdIt last, Compare comp)
{
   if(first == last) return true;
   FwdIt next = first;
   ++next;
   for(; next != last; ++first, ++next) {
      if(comp(*next, *first)) return false;
   }
   return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//   Stateful allocator: equal-comparison depends on state. POC* traits are
//   left at default (no propagation), so it can trigger unequal-allocator
//   code paths during move/swap.
//
//////////////////////////////////////////////////////////////////////////////

template<class T>
class stateful_allocator
{
   public:
   typedef T                       value_type;
   typedef T*                      pointer;
   typedef const T*                const_pointer;
   typedef T&                      reference;
   typedef const T&                const_reference;
   typedef std::size_t             size_type;
   typedef std::ptrdiff_t          difference_type;

   template<class U>
   struct rebind { typedef stateful_allocator<U> other; };

   int state;

   stateful_allocator() : state(0) {}
   explicit stateful_allocator(int s) : state(s) {}

   template<class U>
   stateful_allocator(const stateful_allocator<U>& o) : state(o.state) {}

   T* allocate(std::size_t n)
   {  return static_cast<T*>(::operator new(n * sizeof(T)));  }

   void deallocate(T* p, std::size_t) { ::operator delete(p); }

   size_type max_size() const
   {  return static_cast<size_type>(-1) / sizeof(T);  }

   friend bool operator==(const stateful_allocator& a, const stateful_allocator& b)
   {  return a.state == b.state; }
   friend bool operator!=(const stateful_allocator& a, const stateful_allocator& b)
   {  return a.state != b.state; }
};

template<>
class stateful_allocator<void>
{
   public:
   typedef void             value_type;
   typedef void*            pointer;
   typedef const void*      const_pointer;

   template<class U>
   struct rebind { typedef stateful_allocator<U> other; };

   int state;

   stateful_allocator() : state(0) {}
   explicit stateful_allocator(int s) : state(s) {}

   template<class U>
   stateful_allocator(const stateful_allocator<U>& o) : state(o.state) {}
};

//////////////////////////////////////////////////////////////////////////////
//
//   Rebind helpers: produce nest<U, Alloc<U>> from nest<T, Alloc<T>>
//
//////////////////////////////////////////////////////////////////////////////

//! Uses Nest::allocator_type (rather than the 2nd template argument) to cope
//! with nests instantiated with the default allocator argument (which maps
//! to boost::container::new_allocator internally).
template<class Nest_, class U>
struct rebind_value_type
{
   private:
   typedef typename Nest_::allocator_type                    allocator_type;
   typedef typename allocator_type::template rebind<U>::other other_alloc;
   public:
   typedef boost::container::nest<U, other_alloc>            type;
};

//////////////////////////////////////////////////////////////////////////////
//
//   tracked<T>: tracks whether the last copy/move operation was copy or move
//
//////////////////////////////////////////////////////////////////////////////

enum tracked_provenance { ab_ovo = 0, from_copy, from_move };

template<class T>
struct tracked
{
   BOOST_COPYABLE_AND_MOVABLE(tracked)

   public:
   T                  x;
   tracked_provenance origin;
   tracked_provenance last_op;

   tracked()
      : x(), origin(ab_ovo), last_op(ab_ovo) {}

   explicit tracked(const T& x_)
      : x(x_), origin(from_copy), last_op(ab_ovo) {}

   tracked(const tracked& o)
      : x(o.x), origin(o.origin), last_op(from_copy) {}

   tracked(BOOST_RV_REF(tracked) o)
      : x(o.x), origin(o.origin), last_op(from_move)
   {}

   tracked& operator=(BOOST_COPY_ASSIGN_REF(tracked) o)
   {
      x       = o.x;
      origin  = o.origin;
      last_op = from_copy;
      return *this;
   }

   tracked& operator=(BOOST_RV_REF(tracked) o)
   {
      x       = o.x;
      origin  = o.origin;
      last_op = from_move;
      return *this;
   }

   friend bool operator==(const tracked& a, const tracked& b)
   {  return a.x == b.x; }
   friend bool operator!=(const tracked& a, const tracked& b)
   {  return !(a == b); }
};

//////////////////////////////////////////////////////////////////////////////
//
//   Namespace-scope functors (C++03 does not allow local types as template
//   arguments, so everything used with for_each / find_if / erase_if lives here)
//
//////////////////////////////////////////////////////////////////////////////

template<class T>
struct even_pred
{
   bool operator()(const T& v) const { return (int)(v) % 2 == 0; }
};

template<class T>
struct accum_functor
{
   unsigned int* res;
   accum_functor(unsigned int& r) : res(&r) {}
   void operator()(T& v)       const { *res += (unsigned int)v; }
   void operator()(const T& v) const { *res += (unsigned int)v; }
};

template<class T>
struct bounded_accum_functor
{
   unsigned int* res;
   std::size_t*  n;
   bounded_accum_functor(unsigned int& r, std::size_t& nn) : res(&r), n(&nn) {}

   bool operator()(T& v) const
   {
      if(*n == 0) return false;
      --(*n);
      *res += (unsigned int)v;
      return true;
   }
   bool operator()(const T& v) const
   {
      if(*n == 0) return false;
      --(*n);
      *res += (unsigned int)v;
      return true;
   }
};

template<class T>
struct negated_bounded_accum
{
   bounded_accum_functor<T> f;
   negated_bounded_accum(const bounded_accum_functor<T>& f_) : f(f_) {}
   bool operator()(T& v) const       { return !f(v); }
   bool operator()(const T& v) const { return !f(v); }
};

//////////////////////////////////////////////////////////////////////////////
//
//                   Test sub-suites (templated on the nest type)
//
//////////////////////////////////////////////////////////////////////////////

template<class Nest, class R>
void test_global_erase(const R& rng, const typename Nest::allocator_type& al)
{
   typedef typename Nest::value_type value_type;
   typedef typename Nest::size_type  size_type;

   Nest x(al);
   const value_type* odd_value_ptr = 0;
   for(typename R::const_iterator it = rng.begin(); it != rng.end(); ++it) {
      if((int)(*it) % 2 != 0) { odd_value_ptr = &*it; break; }
   }
   BOOST_TEST(odd_value_ptr != 0);
   const value_type& odd_value = *odd_value_ptr;

   BOOST_TEST_EQ(erase(x, odd_value),           (size_type)0);
   BOOST_TEST_EQ(x.size(),                      (size_type)0);
   BOOST_TEST_EQ(erase_if(x, even_pred<value_type>()), (size_type)0);
   BOOST_TEST_EQ(x.size(),                      (size_type)0);

   x.insert(rng.begin(), rng.end());
   size_type s = x.size();
   size_type n = erase(x, odd_value);
   size_type rng_odd_count =
      (size_type)std::count(rng.begin(), rng.end(), odd_value);
   BOOST_TEST_EQ(n, rng_odd_count);
   BOOST_TEST_EQ(
      (size_type)std::count(x.begin(), x.end(), odd_value), (size_type)0);
   BOOST_TEST_EQ(x.size(), s - n);

   s = x.size();
   n = erase_if(x, even_pred<value_type>());
   size_type rng_even_count = (size_type)std::count_if(
      rng.begin(), rng.end(), even_pred<value_type>());
   BOOST_TEST_EQ(n, rng_even_count);
   BOOST_TEST_EQ((size_type)std::count_if(
      x.begin(), x.end(), even_pred<value_type>()), (size_type)0);
   BOOST_TEST_EQ(x.size(), s - n);
}

template<class Nest>
void test_construct_copy_destroy(const typename Nest::allocator_type& al)
{
   typedef typename Nest::value_type             value_type;
   typedef typename Nest::size_type              size_type;

   std::vector<value_type> rng      = make_range<value_type>(200);
   std::vector<value_type> zeros(70, value_type());
   std::vector<value_type> repeated(100, rng[10]);

   {
      Nest x(al);
      BOOST_TEST(x.empty());
   }
   {
      Nest x(zeros.size(), al);
      test_equal(x, zeros);
   }
   {
      Nest x(repeated.size(), repeated.front(), al);
      test_equal(x, repeated);
   }
   {
      // [sequence.reqmts/69.1]: (20, 20) picks the count/value overload
      Nest x((size_type)20, (value_type)20, al);
      BOOST_TEST_EQ(x.size(), (size_type)20);
   }
   {
      Nest x(rng.begin(), rng.end(), al);
      test_equal(x, rng);
   }
   {
      const Nest x(rng.begin(), rng.end(), al);
      Nest y(x);
      Nest z(y, al);
      test_equal(x, y);
      test_equal(x, z);
   }
   {
      Nest x(rng.begin(), rng.end(), al);
      Nest y(boost::move(x));
      BOOST_TEST(x.empty());
      test_equal(y, rng);

      Nest z(boost::move(y), al);
      BOOST_TEST(y.empty());
      test_equal(z, rng);
   }
   {
      Nest x(rng.begin(), rng.end(), al);
      Nest y(al);
      Nest& ry = (y = x);
      BOOST_TEST_EQ(&ry, &y);
      test_equal(x, y);
   }
   {
      Nest x(rng.begin(), rng.end(), al);
      Nest y(al);
      Nest& ry = (y = boost::move(x));
      BOOST_TEST_EQ(&ry, &y);
      BOOST_TEST(x.empty());
      test_equal(y, rng);
   }
   {
      Nest x(rng.begin(), rng.begin() + std::ptrdiff_t(rng.size() / 2), al);
      puncture(x);
      x.assign(rng.begin(), rng.end());
      test_equal(x, rng);
   }
   {
      Nest x(zeros.size(), al);
      puncture(x);
      x.assign(repeated.size(), repeated[0]);
      test_equal(x, repeated);
   }
   {
      const Nest x(al);
      BOOST_TEST(x.get_allocator() == al);
   }
}

template<class Nest>
void test_iterators(const typename Nest::allocator_type& al)
{
   typedef typename Nest::value_type     value_type;
   typedef typename Nest::iterator       iterator;
   typedef typename Nest::const_iterator const_iterator;

   std::vector<value_type> rng = make_range<value_type>(200);

   std::vector<value_type> data = rng;
   Nest x(data.begin(), data.end(), al);
   const Nest& cx = x;
   puncture(data);
   puncture(x);

   BOOST_TEST(x.rbegin().base()  == x.end());
   BOOST_TEST(cx.rbegin().base() == cx.end());
   BOOST_TEST(x.rend().base()    == x.begin());
   BOOST_TEST(cx.rend().base()   == cx.begin());
   BOOST_TEST(cx.cbegin()        == cx.begin());
   BOOST_TEST(cx.cend()          == cx.end());
   BOOST_TEST(cx.crbegin()       == cx.rbegin());
   BOOST_TEST(cx.crend()         == cx.rend());

   iterator       it  = x.begin();
   iterator       it2 = x.end();
   const_iterator cit = it;
   BOOST_TEST(cit == it);
   cit = it2;
   BOOST_TEST(cit == it2);
   it = it2;
   BOOST_TEST(it == it2);

   test_traversal(x.begin(),  x.end(),  data);
   test_traversal(x.cbegin(), x.cend(), data);
}

template<class Nest>
void test_capacity(const typename Nest::allocator_type& al)
{
   typedef typename Nest::value_type value_type;
   typedef typename Nest::size_type  size_type;

   std::vector<value_type> rng = make_range<value_type>(200);

   Nest        x(al);
   const Nest& cx = x;

   x.reserve(1000);
   x.insert(rng.begin(), rng.end());
   BOOST_TEST(!cx.empty());
   BOOST_TEST_EQ(cx.size(), (size_type)rng.size());
   BOOST_TEST_GT(cx.max_size(), (size_type)0);
   BOOST_TEST_GE(cx.capacity(), (size_type)1000);

   Nest x2 = x;
   x.shrink_to_fit();
   test_equal(x, x2);
   BOOST_TEST_EQ(cx.size(), (size_type)rng.size());
   BOOST_TEST_GE(cx.capacity(), (size_type)rng.size());

   size_type c = cx.capacity();
   x.reserve(c + 1000);
   x.trim_capacity(c + 500);
   BOOST_TEST_LT(cx.capacity(), c + 1000);
   BOOST_TEST_GE(cx.capacity(), c + 500);
   x.trim_capacity();
   BOOST_TEST_EQ(cx.capacity(), c);
   test_equal(x, x2);

   if(cx.max_size() < (size_type)(-1)) {
      BOOST_TEST_THROWS(x.reserve(cx.max_size() + 1), boost::container::length_error);
   }
}

//! Exercises the partitioned available list: empty (reserved) blocks are
//! kept grouped at the back, so trim_capacity()/shrink_to_fit() reclaim
//! exactly the empty blocks regardless of how/where they were emptied
//! (single erase, range erase or compaction).
template<class Nest>
void test_trim_partition(const typename Nest::allocator_type& al)
{
   typedef typename Nest::value_type value_type;
   typedef typename Nest::size_type  size_type;
   typedef typename Nest::iterator   iterator;

   //Implementation-defined block size (slots per block).
   const size_type N = 64;

   //1) Erase a whole block's worth from the front (single erases); one
   //   element survives, so a single block suffices after trimming.
   {
      Nest x(N + 1, value_type(), al);
      x.reserve(10 * N);
      for(size_type i = 0; i < N; ++i) x.erase(x.begin());
      x.trim_capacity();
      BOOST_TEST_EQ(x.size(), (size_type)1);
      BOOST_TEST_EQ(x.capacity(), N);
   }

   //2) Erase a 2N range; one element survives.
   {
      Nest x(2 * N + 1, value_type(), al);
      x.reserve(10 * N);
      iterator last = x.begin();
      std::advance(last, (std::ptrdiff_t)(2 * N));
      x.erase(x.begin(), last);
      x.trim_capacity();
      BOOST_TEST_EQ(x.size(), (size_type)1);
      BOOST_TEST_EQ(x.capacity(), N);
   }

   //3) Punch half of each of three blocks, then compact + trim: the 1.5N
   //   surviving elements must end up packed into two blocks.
   {
      Nest x(3 * N, value_type(), al);
      x.reserve(10 * N);
      iterator pos0 = x.begin();
      iterator pos1 = x.begin(); std::advance(pos1, (std::ptrdiff_t)N);
      iterator pos2 = x.begin(); std::advance(pos2, (std::ptrdiff_t)(2 * N));
      iterator e;
      e = pos2; std::advance(e, (std::ptrdiff_t)(N / 2)); x.erase(pos2, e);
      e = pos1; std::advance(e, (std::ptrdiff_t)(N / 2)); x.erase(pos1, e);
      e = pos0; std::advance(e, (std::ptrdiff_t)(N / 2)); x.erase(pos0, e);
      x.shrink_to_fit();
      BOOST_TEST_EQ(x.size(), (size_type)(3 * N - 3 * (N / 2)));
      BOOST_TEST_EQ(x.capacity(), (size_type)(2 * N));
   }

   //4) trim_capacity(n) releases only empties and keeps at least n capacity.
   {
      Nest x(N, value_type(), al);
      x.reserve(10 * N);
      x.trim_capacity(3 * N);
      BOOST_TEST_EQ(x.capacity(), (size_type)(3 * N));
      x.trim_capacity();
      BOOST_TEST_EQ(x.capacity(), N);
      BOOST_TEST_EQ(x.size(), N);
   }

   //5) Empty non-adjacent blocks (range erases): trim must reclaim exactly
   //   those blocks and the freed capacity must remain reusable afterwards.
   {
      Nest x(5 * N, value_type(), al);
      iterator b0 = x.begin();
      iterator b2 = x.begin(); std::advance(b2, (std::ptrdiff_t)(2 * N));
      iterator e;
      e = b2; std::advance(e, (std::ptrdiff_t)N); x.erase(b2, e);
      e = b0; std::advance(e, (std::ptrdiff_t)N); x.erase(b0, e);
      BOOST_TEST_EQ(x.size(), (size_type)(3 * N));
      x.trim_capacity();
      BOOST_TEST_EQ(x.capacity(), (size_type)(3 * N));
      BOOST_TEST_EQ(x.size(), (size_type)(3 * N));
      x.insert((size_type)(2 * N), value_type());
      BOOST_TEST_EQ(x.size(), (size_type)(5 * N));
   }
}

template<class Nest>
void test_modifiers_provenance(const typename Nest::allocator_type& al)
{
   typedef typename Nest::value_type                                value_type;
   typedef tracked<value_type>                                      tracked_value_type;
   typedef typename rebind_value_type<Nest, tracked_value_type>::type tracked_nest;
   typedef typename tracked_nest::iterator                          tr_iterator;

   typename tracked_nest::allocator_type tal(al);
   tracked_nest       x(tal);
   tracked_value_type v((value_type()));
   tr_iterator        it;

   it = x.emplace(v);
   BOOST_TEST(it->x == v.x);
   BOOST_TEST_EQ((int)it->last_op, (int)from_copy);

   v.x += value_type(1);
   {
      tracked_value_type tmp(v);
      it = x.emplace(boost::move(tmp));
   }
   BOOST_TEST(it->x == v.x);
   BOOST_TEST_EQ((int)it->last_op, (int)from_move);

   v.x += value_type(1);
   it = x.emplace_hint(x.cbegin(), v);
   BOOST_TEST(it->x == v.x);
   BOOST_TEST_EQ((int)it->last_op, (int)from_copy);

   v.x += value_type(1);
   {
      tracked_value_type tmp(v);
      it = x.emplace_hint(x.cbegin(), boost::move(tmp));
   }
   BOOST_TEST(it->x == v.x);
   BOOST_TEST_EQ((int)it->last_op, (int)from_move);

   v.x += value_type(1);
   it = x.insert(v);
   BOOST_TEST(it->x == v.x);
   BOOST_TEST_EQ((int)it->last_op, (int)from_copy);

   v.x += value_type(1);
   {
      tracked_value_type tmp(v);
      it = x.insert(boost::move(tmp));
   }
   BOOST_TEST(it->x == v.x);
   BOOST_TEST_EQ((int)it->last_op, (int)from_move);

   v.x += value_type(1);
   it = x.insert(x.cbegin(), v);
   BOOST_TEST(it->x == v.x);
   BOOST_TEST_EQ((int)it->last_op, (int)from_copy);

   v.x += value_type(1);
   {
      tracked_value_type tmp(v);
      it = x.insert(x.cbegin(), boost::move(tmp));
   }
   BOOST_TEST(it->x == v.x);
   BOOST_TEST_EQ((int)it->last_op, (int)from_move);
}

template<class Nest>
void test_modifiers(const typename Nest::allocator_type& al)
{
   typedef typename Nest::value_type     value_type;
   typedef typename Nest::size_type      size_type;
   typedef typename Nest::iterator       iterator;
   typedef typename Nest::const_iterator const_iterator;

   std::vector<value_type> rng = make_range<value_type>(200);

   test_modifiers_provenance<Nest>(al);

   {
      Nest x(al);
      x.insert(rng.begin(), rng.begin());
      BOOST_TEST(x.empty());

      x.insert(rng.begin(), rng.end());
      test_equal(x, rng);
   }
   {
      Nest x(al);
      x.assign((size_type)1, (value_type)1);
      x.assign((size_type)0, (value_type)1);
      BOOST_TEST_EQ(x.size(), (size_type)0);

      x.assign((size_type)65, (value_type)1);
      x.assign((size_type)65, (value_type)1);
      BOOST_TEST_EQ(x.size(), (size_type)65);
   }
   {
      Nest x(rng.begin(), rng.end(), al);

      iterator it = x.erase(x.cbegin());
      BOOST_TEST_EQ(x.size(), (size_type)(rng.size() - 1));
      BOOST_TEST(*it == rng[1]);

      it = x.erase(x.cend(), x.cend());
      BOOST_TEST_EQ(x.size(), (size_type)(rng.size() - 1));
      BOOST_TEST(it == x.cend());

      const_iterator last_pos = x.cend();
      --last_pos;
      it = x.erase(last_pos, last_pos);
      BOOST_TEST_EQ(x.size(), (size_type)(rng.size() - 1));
      const_iterator last_pos2 = x.cend();
      --last_pos2;
      BOOST_TEST(it == last_pos2);

      const_iterator mid = x.cbegin();
      std::advance(mid, (std::ptrdiff_t)(x.size() / 2));
      it = x.erase(mid, x.cend());
      BOOST_TEST_EQ(x.size(), (size_type)((rng.size() - 1) / 2));
      BOOST_TEST(it == x.cend());
   }
   {
      Nest x0(rng.begin(), rng.end(), al);
      Nest y0(rng.begin(), rng.begin() + std::ptrdiff_t(rng.size() / 2), al);
      Nest x(x0), y(y0);

      x.swap(x);
      test_equal(x, x0);

      swap(x, x);
      test_equal(x, x0);

      x.swap(y);
      test_equal(x, y0);
      test_equal(y, x0);

      swap(x, y);
      test_equal(x, x0);
      test_equal(y, y0);
   }
   {
      Nest x(rng.begin(), rng.end(), al);
      x.clear();
      BOOST_TEST(x.empty());
      x.clear();
      BOOST_TEST(x.empty());
   }
}

template<class Nest>
void test_hive_operations(const typename Nest::allocator_type& al)
{
   typedef typename Nest::value_type     value_type;
   typedef typename Nest::size_type      size_type;
   typedef typename Nest::pointer        pointer; (void)sizeof(pointer);
   typedef typename Nest::const_pointer  const_pointer;
   typedef typename Nest::iterator       iterator;
   typedef typename Nest::const_iterator const_iterator;

   std::vector<value_type> rng = make_range<value_type>(200);

   {
      Nest x(rng.begin(), rng.end(), al);
      Nest y(x);

      iterator it = y.begin();
      y.reserve(y.capacity() + 100);
      x.splice(y);
      BOOST_TEST_EQ(x.size(), (size_type)(2 * rng.size()));
      BOOST_TEST(y.empty());
      BOOST_TEST_GE(y.capacity(), (size_type)100);
      BOOST_TEST(*it == rng[0]);

      y.splice(boost::move(x));
      BOOST_TEST(x.empty());
      BOOST_TEST_EQ(y.size(), (size_type)(2 * rng.size()));
      BOOST_TEST(*it == rng[0]);
   }
   {
      Nest x(al);
      for(typename std::vector<value_type>::const_iterator it = rng.begin();
          it != rng.end(); ++it) {
         x.insert(*it);
         x.insert(*it);
      }

      size_type removed = x.unique(std::equal_to<value_type>());
      BOOST_TEST_EQ(removed, (size_type)rng.size());
      BOOST_TEST_EQ(x.size(),  (size_type)rng.size());
   }
   {
      Nest x(al);
      x.insert(rng.begin(), rng.end());
      x.insert(rng.begin(), rng.end());

      x.sort(std::less<value_type>());
      BOOST_TEST(is_sorted_cxx03(x.begin(), x.end()));

      x.sort(std::greater<value_type>());
      BOOST_TEST(is_sorted_cxx03(x.rbegin(), x.rend()));
   }
   {
      Nest        x(rng.begin(), rng.end(), al);
      const Nest& cx = x;

      for(const_iterator it = x.cbegin(); it != x.cend(); ++it) {
         const_pointer p = boost::pointer_traits<const_pointer>::pointer_to(*it);
         BOOST_TEST(x.get_iterator(p) == it);
         BOOST_TEST(cx.get_iterator(p) == it);
      }
   }
}

template<class Nest>
void test_visitation(const typename Nest::allocator_type& al)
{
   typedef typename Nest::value_type     value_type;
   typedef typename Nest::iterator       iterator;
   typedef typename Nest::const_iterator const_iterator;

   std::vector<value_type> rng = make_range<value_type>(200);

   // for_each at iterator and container scope
   {
      Nest        x(rng.begin(), rng.end(), al);
      const Nest& cx = x;
      puncture(x);

      for(std::size_t i = 0; i < x.size() / 2; ++i) {
         iterator first = x.begin();
         iterator last  = x.end();
         std::advance(first, (std::ptrdiff_t)i);
         std::advance(last, -(std::ptrdiff_t)i);

         const_iterator cfirst = x.cbegin();
         const_iterator clast  = x.cend();
         std::advance(cfirst, (std::ptrdiff_t)i);
         std::advance(clast, -(std::ptrdiff_t)i);

         unsigned int res = 0;
         boost::container::for_each(first, last, accum_functor<value_type>(res));
         unsigned int res1 = res;

         res = 0;
         boost::container::for_each(cfirst, clast, accum_functor<value_type>(res));
         unsigned int res2 = res;

         res = 0;
         std::for_each(first, last, accum_functor<value_type>(res));
         unsigned int res3 = res;

         BOOST_TEST_EQ(res1, res3);
         BOOST_TEST_EQ(res2, res3);
      }

      unsigned int res = 0;
      boost::container::for_each(x, accum_functor<value_type>(res));
      unsigned int res1 = res;

      res = 0;
      boost::container::for_each(cx, accum_functor<value_type>(res));
      unsigned int res2 = res;

      res = 0;
      std::for_each(x.begin(), x.end(), accum_functor<value_type>(res));
      unsigned int res3 = res;

      BOOST_TEST_EQ(res1, res3);
      BOOST_TEST_EQ(res2, res3);
   }

   // for_each_while at iterator and container scope
   {
      Nest        x(rng.begin(), rng.end(), al);
      const Nest& cx = x;
      puncture(x);

      for(std::size_t i = 0; i <= x.size(); ++i) {
         iterator       first  = x.begin();
         const_iterator cfirst = x.cbegin();
         std::advance(first,  (std::ptrdiff_t)i);
         std::advance(cfirst, (std::ptrdiff_t)i);

         unsigned int res = 0;
         std::size_t  n   = (std::size_t)std::distance(first, x.end()) / 2;
         iterator     it1 = boost::container::for_each_while(
            first, x.end(), bounded_accum_functor<value_type>(res, n)).first;
         unsigned int res1 = res;

         res = 0;
         n   = (std::size_t)std::distance(first, x.end()) / 2;
         const_iterator it2 = boost::container::for_each_while(
            cfirst, cx.end(), bounded_accum_functor<value_type>(res, n)).first;
         unsigned int res2 = res;

         res = 0;
         n   = (std::size_t)std::distance(first, x.end()) / 2;
         bounded_accum_functor<value_type> baf(res, n);
         iterator it3 = std::find_if(
            first, x.end(), negated_bounded_accum<value_type>(baf));
         unsigned int res3 = res;

         BOOST_TEST(it1 == it3);
         BOOST_TEST_EQ(res1, res3);
         BOOST_TEST(it2 == it3);
         BOOST_TEST_EQ(res2, res3);
      }

      unsigned int res = 0;
      std::size_t  n   = x.size();
      iterator     it1 = boost::container::for_each_while(
         x, bounded_accum_functor<value_type>(res, n)).first;
      unsigned int res1 = res;

      res = 0;
      n   = x.size();
      const_iterator it2 = boost::container::for_each_while(
         cx, bounded_accum_functor<value_type>(res, n)).first;
      unsigned int res2 = res;

      res = 0;
      n   = x.size();
      bounded_accum_functor<value_type> baf(res, n);
      iterator it3 = std::find_if(
         x.begin(), x.end(), negated_bounded_accum<value_type>(baf));
      unsigned int res3 = res;

      BOOST_TEST(it1 == it3);
      BOOST_TEST_EQ(res1, res3);
      BOOST_TEST(it2 == it3);
      BOOST_TEST_EQ(res2, res3);
   }
}

//////////////////////////////////////////////////////////////////////////////
//
//                   Generic test driver
//
//////////////////////////////////////////////////////////////////////////////

template<class Nest>
void test_all(const typename Nest::allocator_type& al)
{
   typedef typename Nest::value_type value_type;

   std::vector<value_type> rng = make_range<value_type>(200);

   test_construct_copy_destroy<Nest>(al);
   test_iterators<Nest>(al);
   test_capacity<Nest>(al);
   test_trim_partition<Nest>(al);
   test_modifiers<Nest>(al);
   test_hive_operations<Nest>(al);
   test_visitation<Nest>(al);
   test_global_erase<Nest>(rng, al);
}

template<class Nest>
void test_all_default_alloc()
{
   typename Nest::allocator_type al;
   test_all<Nest>(al);
}

//! Extra check: move-construction with unequal (stateful) allocators must
//! fall back to element-by-element move.
template<class Nest>
void test_move_with_unequal_allocators()
{
   typedef typename Nest::value_type     value_type;
   typedef typename Nest::allocator_type allocator_type;

   std::vector<value_type> rng = make_range<value_type>(200);

   Nest x(rng.begin(), rng.end(), allocator_type(0));
   Nest y(boost::move(x), allocator_type(1));
   BOOST_TEST_EQ(x.get_allocator().state, 0);
   BOOST_TEST(x.empty());
   BOOST_TEST_EQ(y.get_allocator().state, 1);
   test_equal(y, rng);
}

//////////////////////////////////////////////////////////////////////////////
//
//                                  main
//
//////////////////////////////////////////////////////////////////////////////

int main()
{
   {
      typedef nest<int> nest_int_t;
      test_all_default_alloc<nest_int_t>();
   }
   {
      typedef nest<std::size_t> nest_sz_t;
      test_all_default_alloc<nest_sz_t>();
   }
   {
      typedef nest<int, stateful_allocator<int> > nest_stateful_t;
      test_all<nest_stateful_t>(stateful_allocator<int>(42));
      test_move_with_unequal_allocators<nest_stateful_t>();
   }

   return boost::report_errors();
}
