//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2024-2024. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//[doc_small_vector_base
#include <boost/container/small_vector.hpp>

#include <cstddef>

//Make sure assertions are active
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

using namespace boost::container;

//small_vector_base<T> erases the preallocated element count "N" from the type,
//so the following non-template functions accept any small_vector<int, N>.

//Reads any small_vector<int, N> (taken by const reference to the base).
int sum(const small_vector_base<int> &sv)
{
   int res = 0;
   for(std::size_t i = 0, n = sv.size(); i != n; ++i)
      res += sv[i];
   return res;
}

//Modifies any small_vector<int, N> (taken by reference to the base).
void push_range(small_vector_base<int> &sv, int first, int last)
{
   for(int i = first; i < last; ++i)
      sv.push_back(i);
}

int main ()
{
   //Two small_vectors with different in-place capacities (N = 4 and N = 64)...
   small_vector<int, 4>  sv_small;
   small_vector<int, 64> sv_big;

   //...both implicitly convert to small_vector_base<int>, so the very same
   //non-template functions can operate on either of them.
   push_range(sv_small, 0, 5);   //grows beyond its 4 preallocated elements
   push_range(sv_big,   0, 5);   //stays within its 64 preallocated elements

   //The behaviour is identical regardless of the preallocated capacity N.
   assert(sum(sv_small) == (0+1+2+3+4));
   assert(sum(sv_small) == sum(sv_big));

   return 0;
}
//]
