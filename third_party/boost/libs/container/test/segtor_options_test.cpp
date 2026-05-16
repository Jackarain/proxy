//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/container/segtor.hpp>
#include <boost/container/new_allocator.hpp>
#include <boost/container/allocator.hpp>
#include <boost/container/options.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::container;

template<class Unsigned, class VectorType>
void test_stored_size_type_impl()
{
   #ifndef BOOST_NO_EXCEPTIONS
   VectorType v;
   typedef typename VectorType::size_type    size_type;
   size_type const max = Unsigned(-1);
   v.resize(5);
   BOOST_TEST_THROWS(v.resize(max+1),                    std::exception);
   BOOST_TEST_THROWS(VectorType v2(max+1),               std::exception);
   #endif
}

template<class Unsigned>
void test_stored_size_type()
{
   #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
   using options_t = segtor_options_t< stored_size<Unsigned> >;
   #else
   typedef typename segtor_options
      < stored_size<Unsigned> >::type options_t;
   #endif

   typedef segtor<unsigned char, new_allocator<unsigned char> > default_segtor_t;
   {
      typedef segtor<unsigned char, new_allocator<unsigned char>, options_t> segtor_t;
      BOOST_CONTAINER_STATIC_ASSERT(sizeof(segtor_t) < sizeof(default_segtor_t));
      test_stored_size_type_impl<Unsigned, segtor_t>();
   }
   {
      typedef segtor<unsigned char, allocator<unsigned char>, options_t> segtor_t;
      BOOST_CONTAINER_STATIC_ASSERT(sizeof(segtor_t) < sizeof(default_segtor_t));
      test_stored_size_type_impl<Unsigned, segtor_t>();
   }
}

void test_block_bytes()
{
   #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
   using options_t = segtor_options_t< block_bytes<128u> >;
   #else
   typedef segtor_options< block_bytes<128u> >::type options_t;
   #endif
   typedef segtor<unsigned short, void, options_t> segtor_t;
   BOOST_TEST(segtor_t::get_block_size() == 128u/sizeof(unsigned short));
}

void test_block_elements()
{
   #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
   using options_t = segtor_options_t< block_size<64> >;
   #else
   typedef segtor_options< block_size<64> >::type options_t;
   #endif
   typedef segtor<unsigned char, void, options_t> segtor_t;
   BOOST_TEST(segtor_t::get_block_size() == 64U);
}

void test_reservable()
{
   {
      #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
      using options_t = segtor_options_t< reservable<true> >;
      #else
      typedef segtor_options< reservable<true> >::type options_t;
      #endif
      typedef segtor<unsigned short, void, options_t> segtor_t;
      BOOST_CONTAINER_STATIC_ASSERT(segtor_t::is_reservable == true);
   }
   {
      #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
      using options_t = segtor_options_t< reservable<false> >;
      #else
      typedef segtor_options< reservable<false> >::type options_t;
      #endif
      typedef segtor<unsigned short, void, options_t> segtor_t;
      BOOST_CONTAINER_STATIC_ASSERT(segtor_t::is_reservable == false);
   }
}

int main()
{
   test_block_bytes();
   test_block_elements();
   test_stored_size_type<unsigned char>();
   test_stored_size_type<unsigned short>();
   test_reservable();
   return ::boost::report_errors();
}
