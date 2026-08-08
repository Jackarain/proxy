//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2024-2024. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//[doc_devector
#include <boost/container/devector.hpp>

#include <cstddef>

//Make sure assertions are active
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main ()
{
   using namespace boost::container;

   devector<int> dv;

   //Unlike vector, a devector can grow cheaply (amortized constant time)
   //at *both* ends, while keeping all elements in contiguous memory.
   dv.push_back(2);              // {2}
   dv.push_back(3);              // {2, 3}
   dv.push_front(1);            // {1, 2, 3}
   dv.push_front(0);            // {0, 1, 2, 3}

   assert(dv.size() == 4);
   assert(dv.front() == 0);
   assert(dv.back()  == 3);

   //Random access and contiguous storage, just like vector.
   assert(dv[0] == 0 && dv[3] == 3);
   const int *p = dv.data();
   for(std::size_t i = 0, n = dv.size(); i != n; ++i)
      assert(p[i] == (int)i);

   //We can ask for free capacity at each end independently, so that the
   //following insertions at that end do not trigger a reallocation.
   dv.reserve_front(dv.size() + 4);
   dv.reserve_back(dv.size() + 4);

   //emplace variants construct the element in place at either end.
   dv.emplace_front(-1);        // {-1, 0, 1, 2, 3}
   dv.emplace_back(4);          // {-1, 0, 1, 2, 3, 4}
   assert(dv.front() == -1);
   assert(dv.back()  == 4);

   //Removal at both ends is constant time and never reallocates.
   dv.pop_front();              // {0, 1, 2, 3, 4}
   dv.pop_back();               // {0, 1, 2, 3}
   assert(dv.size() == 4);
   assert(dv.front() == 0);
   assert(dv.back()  == 3);

   return 0;
}
//]
