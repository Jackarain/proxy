//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2024-2024. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//[doc_slist
#include <boost/container/slist.hpp>

//Make sure assertions are active
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main ()
{
   using namespace boost::container;

   typedef slist<int> slist_t;

   slist_t l;

   //A singly linked list offers constant-time insertion/removal at the front.
   l.push_front(3);
   l.push_front(2);
   l.push_front(1);                       // {1, 2, 3}

   //Unlike std::forward_list, Boost.Container slist keeps track of its length,
   //so size() is a constant-time operation.
   assert(l.size() == 3);
   assert(l.front() == 1);

   //The efficient way to insert into a singly linked list is *after* a known
   //position (constant time). insert_after/erase_after avoid the linear scan
   //that plain insert/erase would need to locate the previous node.
   slist_t::iterator it = l.begin();      // -> 1
   ++it;                                   // -> 2
   l.insert_after(it, 20);                // {1, 2, 20, 3}
   assert(l.size() == 4);

   //erase_after removes the element following the iterator in constant time.
   l.erase_after(it);                     // removes 20 -> {1, 2, 3}
   assert(l.size() == 3);

   //before_begin() denotes the position just before the first element, so
   //inserting after it is equivalent to push_front.
   l.insert_after(l.before_begin(), 0);   // {0, 1, 2, 3}
   assert(l.front() == 0);

   //slist provides a size-aware splice_after overload that merges a range from
   //another list in constant time when the number of elements is known.
   slist_t other;
   other.push_front(6);
   other.push_front(5);                   // {5, 6}

   slist_t::iterator before_last = other.begin();
   ++before_last;                          // points to the last element (6)
   //Move the whole 'other' list to the front of 'l' in O(1).
   l.splice_after(l.before_begin(), other,
                  other.before_begin(), before_last, other.size());
   assert(other.empty());                  // {5, 6, 0, 1, 2, 3}

   //Forward, in-order traversal.
   const int expected[] = { 5, 6, 0, 1, 2, 3 };
   int i = 0;
   for(slist_t::const_iterator b = l.begin(), e = l.end(); b != e; ++b, ++i)
      assert(*b == expected[i]);
   assert(l.size() == 6);

   return 0;
}
//]
