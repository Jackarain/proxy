//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2006-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/interprocess/detail/workaround.hpp>
//[doc_complex_map_uses_allocator
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/container/map.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/string.hpp>
//=#include <functional>
//<-
#include "../test/get_process_id_name.hpp"
//->

using namespace boost::interprocess;
namespace bc = boost::container;

//Typedefs of allocators and containers
typedef managed_shared_memory::segment_manager                   seg_mngr_t;
typedef allocator<void, seg_mngr_t>                              void_alloc_t;
typedef bc::vector<int, allocator<int, seg_mngr_t> >             int_vec_t;
typedef bc::vector<int_vec_t, allocator<int_vec_t, seg_mngr_t> > int_vec_vec_t;
typedef bc::basic_string
   <char, std::char_traits<char>, allocator<char, seg_mngr_t> >  string_t;

class complex_data
{
   string_t      string_;
   int_vec_vec_t int_vec_vec_;

   public:
   typedef void_alloc_t allocator_type; //Activates the uses-allocator protocol

   complex_data(const char *name, const void_alloc_t &valloc) //void_alloc_t is convertible to allocator<T>
      : string_(name, valloc), int_vec_vec_(valloc)
   {}
   //<-
   string_t get_char_string() { return string_; };
   int_vec_vec_t get_int_vector_vector() { return int_vec_vec_; };
   //->
};

//<-
struct lessthan //A transparent comparison functor
{
   typedef void is_transparent;
   template<class T, class U> bool operator() (const T &t, const U &u) const { return t < u; }
};
//->
//Definition of a shared memory map<string_t, complex_data...> with transparent comparison
typedef std::pair<const string_t, complex_data>          map_value_type;
typedef allocator<map_value_type, seg_mngr_t>            map_value_type_allocator;
//=typedef bc::map< string_t, complex_data, std::less<>
//<-
typedef bc::map< string_t, complex_data, lessthan
//->
               , map_value_type_allocator>               complex_map_type;

int main ()
{
   //<-
   //Remove shared memory on construction and destruction
   struct shm_remove
   {
      shm_remove() { shared_memory_object::remove(test::get_process_id_name()); }
      ~shm_remove(){ shared_memory_object::remove(test::get_process_id_name()); }
   } remover;
   (void)remover;
   //->
   managed_shared_memory segment(create_only, test::get_process_id_name(), 65536); //Create shared memory

   //Construct the map calling map(key_compare, allocator_type), the allocator argument is implicit
   complex_map_type *mymap = segment.construct<complex_map_type>("MyMap")();

   //Efficient transparent insertion, string_t and complex_data are constructed in-place with implicit allocators
   mymap->try_emplace("key_str", "default_name");

   return 0;
}
//]
