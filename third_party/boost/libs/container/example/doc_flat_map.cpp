//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2024-2024. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//[doc_flat_map
#include <boost/container/flat_map.hpp>
#include <boost/move/utility_core.hpp>   //boost::move

//Make sure assertions are active
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main ()
{
   using namespace boost::container;

   typedef flat_map<int, int> map_t;

   map_t m;

   //Like vector, we can reserve storage to avoid reallocations while filling.
   m.reserve(8);

   //Insertions keep the underlying vector sorted by key. They are O(N)
   //because the elements after the insertion point must be shifted.
   m[30] = 3;
   m[10] = 1;
   m[20] = 2;
   assert(m.size() == 3);

   //Iteration is in key order and over contiguous memory, using random-access
   //iterators, so it is much faster than a node-based std::map.
   {
      map_t::const_iterator it = m.begin();
      assert(it->first == 10 && it->second == 1); ++it;
      assert(it->first == 20 && it->second == 2); ++it;
      assert(it->first == 30 && it->second == 3);
   }

   //Lookup uses binary search: O(log N).
   map_t::iterator f = m.find(20);
   assert(f != m.end() && f->second == 2);

   //All values live in a single underlying sequence of value_type, which is
   //std::pair<Key, T> (an array of structs). This is a key difference with the
   //C++23 std::flat_map, a container *adaptor* that keeps keys and mapped
   //values in two separate, parallel containers (a structure of arrays).
   const map_t::value_type *raw = &*m.begin();
   assert(raw[0].first == 10 && raw[2].first == 30);

   //The underlying sorted vector can be moved out with extract_sequence() and
   //moved back in with adopt_sequence(). Because the extracted sequence is
   //already ordered and free of duplicates, we can re-adopt it in O(1) using
   //the ordered_unique_range_t overload.
   map_t::sequence_type seq = m.extract_sequence();
   assert(m.empty());
   m.adopt_sequence(ordered_unique_range_t(), boost::move(seq));
   assert(m.size() == 3 && m.find(30) != m.end());

   return 0;
}
//]
