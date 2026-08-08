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
// Simple segmented container for testing segmented algorithms.
// Models a vector-of-vectors with a sentinel empty segment at the end.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_TEST_SEGMENTED_TEST_HELPER_HPP
#define BOOST_CONTAINER_TEST_SEGMENTED_TEST_HELPER_HPP

#include <boost/container/experimental/segmented_iterator_traits.hpp>
#include <boost/container/detail/iterator.hpp>
#include <boost/container/vector.hpp>
#include <cstddef>
#include <iosfwd>

namespace test_detail {

class movable_int
{
   BOOST_MOVABLE_BUT_NOT_COPYABLE(movable_int)

   int val_;
public:
   explicit movable_int(int v = 0) : val_(v) {}
   movable_int(BOOST_RV_REF(movable_int) other) BOOST_NOEXCEPT
      : val_(other.val_)
   { other.val_ = -1; }

   movable_int& operator=(BOOST_RV_REF(movable_int) other) BOOST_NOEXCEPT
   { val_ = other.val_; other.val_ = -1; return *this; }

   int value() const { return val_; }

   friend bool operator==(const movable_int& a, const movable_int& b)      { return a.val_ == b.val_; }
   friend bool operator!=(const movable_int& a, const movable_int& b)      { return a.val_ != b.val_; }
   friend std::ostream& operator<<(std::ostream& os, const movable_int& m) { return os << m.val_; }
};

template<class T>
class seg_vector_iterator
{
public:
   typedef T                               value_type;
   typedef T&                              reference;
   typedef T*                              pointer;
   typedef std::ptrdiff_t                  difference_type;
   typedef std::bidirectional_iterator_tag  iterator_category;

   typedef boost::container::vector<boost::container::vector<T> >    segments_type;
   typedef typename segments_type::iterator          seg_iter_t;
   typedef typename boost::container::vector<T>::iterator         local_iter_t;

   seg_iter_t    seg_;
   local_iter_t  local_;

   seg_vector_iterator()
      : seg_(), local_()
   {}

   seg_vector_iterator(seg_iter_t s, local_iter_t l)
      : seg_(s), local_(l)
   {}

   reference operator*()  const { return *local_; }
   pointer   operator->() const { return &*local_; }

   seg_vector_iterator& operator++()
   {
      ++local_;
      if(local_ == seg_->end()) {
         ++seg_;
         local_ = seg_->begin();
      }
      return *this;
   }

   seg_vector_iterator operator++(int)
   {
      seg_vector_iterator tmp = *this;
      ++*this;
      return tmp;
   }

   seg_vector_iterator& operator--()
   {
      if(local_ == seg_->begin()) {
         --seg_;
         local_ = seg_->end();
      }
      --local_;
      return *this;
   }

   seg_vector_iterator operator--(int)
   {
      seg_vector_iterator tmp = *this;
      --*this;
      return tmp;
   }

   friend bool operator==(const seg_vector_iterator& a, const seg_vector_iterator& b)
   { return a.seg_ == b.seg_ && a.local_ == b.local_; }

   friend bool operator!=(const seg_vector_iterator& a, const seg_vector_iterator& b)
   { return !(a == b); }
};

template<class T>
class seg_vector
{
   // Last entry is always an empty sentinel segment, following
   // the convention from Austern's segmented-iterator paper.
   boost::container::vector<boost::container::vector<T> > segments_;

   BOOST_COPYABLE_AND_MOVABLE(seg_vector)

public:
   typedef seg_vector_iterator<T> iterator;

   seg_vector()
   {
      segments_.push_back(boost::container::vector<T>());
   }

   seg_vector(const seg_vector &x)
      : segments_(x.segments_)
   {}

   seg_vector(BOOST_RV_REF(seg_vector) x)
      : segments_(boost::move(x.segments_))
   {}

   seg_vector &operator= (BOOST_RV_REF(seg_vector) x)
   {
      segments_ = boost::move(x.segments_);
      return *this;
   }

   seg_vector &operator= (BOOST_COPY_ASSIGN_REF(seg_vector) x)
   {
      segments_ = x.segments_;
      return *this;
   }

   void add_segment(std::size_t n, const T& val)
   {
      segments_.insert(segments_.end() - 1, boost::container::vector<T>(n, val));
   }

   template<class InpIt>
   void add_segment_range(InpIt first, InpIt last)
   {
      segments_.insert(segments_.end() - 1, boost::container::vector<T>(first, last));
   }

   void add_segment_from_ints(const int* first, const int* last)
   {
      boost::container::vector<T> v;
      v.reserve(static_cast<std::size_t>(last - first));
      for(; first != last; ++first)
         v.push_back(T(*first));
      segments_.insert(segments_.end() - 1, boost::move(v));
   }

   iterator begin()
   {
      typename boost::container::vector<boost::container::vector<T> >::iterator s = segments_.begin();
      return iterator(s, s->begin());
   }

   iterator end()
   {
      typename boost::container::vector<boost::container::vector<T> >::iterator s = segments_.end() - 1;
      return iterator(s, s->begin());
   }

   std::size_t total_size() const
   {
      std::size_t n = 0;
      for(std::size_t i = 0; i + 1 < segments_.size(); ++i)
         n += segments_[i].size();
      return n;
   }

   boost::container::vector<boost::container::vector<T> >& segments() { return segments_; }
   const boost::container::vector<boost::container::vector<T> >& segments() const { return segments_; }
};

template<class Iter>
class sentinel_wrapper
{
   Iter it_;
public:
   explicit sentinel_wrapper(Iter it) : it_(it) {}

   operator Iter() const { return it_; }

   friend bool operator==(const Iter& a, const sentinel_wrapper& b) { return a == b.it_; }
   friend bool operator!=(const Iter& a, const sentinel_wrapper& b) { return !(a == b.it_); }
   friend bool operator==(const sentinel_wrapper& a, const Iter& b) { return a.it_ == b; }
   friend bool operator!=(const sentinel_wrapper& a, const Iter& b) { return !(a.it_ == b); }
};

template<class Iter>
sentinel_wrapper<Iter> make_sentinel(Iter it) { return sentinel_wrapper<Iter>(it); }

template<class Iter>
class sized_sentinel_wrapper
{
   Iter it_;
public:
   explicit sized_sentinel_wrapper(Iter it) : it_(it) {}

   operator Iter() const { return it_; }

   friend bool operator==(const Iter& a, const sized_sentinel_wrapper& b) { return a == b.it_; }
   friend bool operator!=(const Iter& a, const sized_sentinel_wrapper& b) { return !(a == b.it_); }
   friend bool operator==(const sized_sentinel_wrapper& a, const Iter& b) { return a.it_ == b; }
   friend bool operator!=(const sized_sentinel_wrapper& a, const Iter& b) { return !(a.it_ == b); }

   friend typename boost::container::iterator_traits<Iter>::difference_type
   operator-(const sized_sentinel_wrapper& a, const Iter& b) { return a.it_ - b; }

   friend typename boost::container::iterator_traits<Iter>::difference_type
   operator-(const Iter& a, const sized_sentinel_wrapper& b) { return a - b.it_; }
};

template<class Iter>
sized_sentinel_wrapper<Iter> make_sized_sentinel(Iter it) { return sized_sentinel_wrapper<Iter>(it); }

template<class T>
class seg2_vector_iterator
{
public:
   typedef T                               value_type;
   typedef T&                              reference;
   typedef T*                              pointer;
   typedef std::ptrdiff_t                  difference_type;
   typedef std::bidirectional_iterator_tag iterator_category;

   typedef boost::container::vector<seg_vector<T> >     segments_type;
   typedef typename segments_type::iterator seg_iter_t;
   typedef seg_vector_iterator<T>           local_iter_t;

   seg_iter_t   seg_;
   local_iter_t local_;

   seg2_vector_iterator()
      : seg_(), local_()
   {}

   seg2_vector_iterator(seg_iter_t s, local_iter_t l)
      : seg_(s), local_(l)
   {}

   reference operator*()  const { return *local_; }
   pointer   operator->() const { return &*local_; }

   seg2_vector_iterator& operator++()
   {
      ++local_;
      if(local_ == seg_->end()) {
         ++seg_;
         local_ = seg_->begin();
      }
      return *this;
   }

   seg2_vector_iterator operator++(int)
   {
      seg2_vector_iterator tmp = *this;
      ++*this;
      return tmp;
   }

   seg2_vector_iterator& operator--()
   {
      if(local_ == seg_->begin()) {
         --seg_;
         local_ = seg_->end();
      }
      --local_;
      return *this;
   }

   seg2_vector_iterator operator--(int)
   {
      seg2_vector_iterator tmp = *this;
      --*this;
      return tmp;
   }

   friend bool operator==(const seg2_vector_iterator& a, const seg2_vector_iterator& b)
   { return a.seg_ == b.seg_ && a.local_ == b.local_; }

   friend bool operator!=(const seg2_vector_iterator& a, const seg2_vector_iterator& b)
   { return !(a == b); }
};

template<class T>
class seg2_vector
{
   boost::container::vector<seg_vector<T> > segments_;

public:
   typedef seg2_vector_iterator<T> iterator;

   seg2_vector()
   {
      segments_.push_back(seg_vector<T>());
   }

   void add_segment(const seg_vector<T>& sv)
   {
      segments_.insert(segments_.end() - 1, sv);
   }

   template<class InpIt>
   void add_flat_segment_range(InpIt first, InpIt last)
   {
      seg_vector<T> sv;
      sv.add_segment_range(first, last);
      segments_.insert(segments_.end() - 1, boost::move(sv));
   }

   void add_flat_segment_from_ints(const int* first, const int* last)
   {
      seg_vector<T> sv;
      sv.add_segment_from_ints(first, last);
      segments_.insert(segments_.end() - 1, boost::move(sv));
   }

   iterator begin()
   {
      typename boost::container::vector<seg_vector<T> >::iterator s = segments_.begin();
      return iterator(s, s->begin());
   }

   iterator end()
   {
      typename boost::container::vector<seg_vector<T> >::iterator s = segments_.end() - 1;
      return iterator(s, s->begin());
   }

   std::size_t total_size() const
   {
      std::size_t n = 0;
      for(std::size_t i = 0; i + 1 < segments_.size(); ++i)
         n += segments_[i].total_size();
      return n;
   }

   boost::container::vector<seg_vector<T> >& segments() { return segments_; }
   const boost::container::vector<seg_vector<T> >& segments() const { return segments_; }
};

} // namespace test_detail

namespace boost {
namespace container {

template<class T>
struct segmented_iterator_traits<test_detail::seg_vector_iterator<T> >
{
   typedef segmented_iterator_tag is_segmented_iterator;

   typedef typename boost::container::vector<boost::container::vector<T> >::iterator segment_iterator;
   typedef typename boost::container::vector<T>::iterator               local_iterator;
   typedef test_detail::seg_vector_iterator<T>             iterator;

   static segment_iterator segment(iterator it) { return it.seg_; }
   static local_iterator   local(iterator it)   { return it.local_; }

   static iterator compose(segment_iterator s, local_iterator l)
   {
      if(l == s->end() && !s->empty()) {
         ++s;
         l = s->begin();
      }
      return iterator(s, l);
   }

   static local_iterator begin(segment_iterator s) { return s->begin(); }
   static local_iterator end(segment_iterator s)   { return s->end(); }
};

template<class T>
struct segmented_iterator_traits<test_detail::seg2_vector_iterator<T> >
{
   typedef segmented_iterator_tag is_segmented_iterator;

   typedef typename boost::container::vector<test_detail::seg_vector<T> >::iterator segment_iterator;
   typedef test_detail::seg_vector_iterator<T>                         local_iterator;
   typedef test_detail::seg2_vector_iterator<T>                        iterator;

   static segment_iterator segment(iterator it) { return it.seg_; }
   static local_iterator   local(iterator it)   { return it.local_; }

   static iterator compose(segment_iterator s, local_iterator l)
   {
      if(l == s->end() && s->total_size() != 0) {
         ++s;
         l = s->begin();
      }
      return iterator(s, l);
   }

   static local_iterator begin(segment_iterator s) { return s->begin(); }
   static local_iterator end(segment_iterator s)   { return s->end(); }
};

} // namespace container
} // namespace boost

#endif // BOOST_CONTAINER_TEST_SEGMENTED_TEST_HELPER_HPP
