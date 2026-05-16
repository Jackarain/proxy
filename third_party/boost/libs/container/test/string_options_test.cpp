//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/container/string.hpp>
#include <boost/container/allocator.hpp>
#include <boost/container/detail/next_capacity.hpp>
#include <boost/core/lightweight_test.hpp>
#include <climits>

using namespace boost::container;

template<class Unsigned, class StringType>
void test_stored_size_type_impl()
{
   #ifndef BOOST_NO_EXCEPTIONS
   StringType s;
   typedef typename StringType::size_type    size_type;
   typedef typename StringType::value_type   value_type;
   //We need a bit to store short/long info, minus the null terminator
   size_type const max = Unsigned(-1)/2u - 1u;
   s.resize(5);
   s.resize(max);
   BOOST_TEST_THROWS(s.resize(max+1),                    std::exception);
   BOOST_TEST_THROWS(s.push_back(value_type(1)),         std::exception);
   BOOST_TEST_THROWS(s.insert(s.begin(), value_type(1)), std::exception);
   BOOST_TEST_THROWS(s.reserve(max+1),                   std::exception);
   #endif
}

template<class Unsigned>
void test_stored_size_type()
{
   #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
   using options_t = string_options_t< stored_size<Unsigned> >;
   #else
   typedef typename string_options
      < stored_size<Unsigned> >::type options_t;
   #endif

   basic_string<char, std::char_traits<char>, void> default_string_t;
   //Test first with a typical allocator
   {
      typedef basic_string<char, std::char_traits<char>, void, options_t> string_t;
      BOOST_CONTAINER_STATIC_ASSERT(sizeof(string_t) < sizeof(default_string_t));
      test_stored_size_type_impl<Unsigned, string_t>();
   }
   //Test with a V2 allocator
   {
      basic_string<char, std::char_traits<char>, allocator<char>, options_t> string_t;
      BOOST_CONTAINER_STATIC_ASSERT(sizeof(string_t) < sizeof(default_string_t));
      //test_stored_size_type_impl<Unsigned, string_t>();
   }
   {
      //Test initial capacity
      typedef basic_string<char, std::char_traits<char>, void, options_t> string_t;
      const std::size_t theoretical_long_repr_t = ((2 * sizeof(Unsigned) - 1u) / sizeof(void*) + 1u) * sizeof(void*) + sizeof(void*);
      BOOST_CONTAINER_STATIC_ASSERT(sizeof(string_t)  == theoretical_long_repr_t);
      const std::size_t theoretical_sso_cap = theoretical_long_repr_t - 2u;   //-2 --> 1 for short header, 1 for null terminator
      string_t s;
      BOOST_TEST(s.capacity() == theoretical_sso_cap);
   }
}

void test_growth_factor_50()
{
   #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
   using options_t = string_options_t< growth_factor<growth_factor_50> >;
   #else
   typedef string_options
      < growth_factor<growth_factor_50> >::type options_t;
   #endif

   basic_string<char, std::char_traits<char>, void, options_t> s;

   s.resize(5);
   s.resize(s.capacity());
   std::size_t old_capacity = s.capacity();
   s.push_back(0);
   std::size_t new_capacity = s.capacity();
   std::size_t old_storage = old_capacity+1u;
   std::size_t new_storage = new_capacity + 1u;
   BOOST_TEST(new_storage == (old_storage + old_storage/2));
}

void test_growth_factor_60()
{
   #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
   using options_t = string_options_t< growth_factor<growth_factor_60> >;
   #else
   typedef string_options
      < growth_factor<growth_factor_60> >::type options_t;
   #endif

   basic_string<char, std::char_traits<char>, void, options_t> s;

   s.resize(5);
   s.resize(s.capacity());
   std::size_t old_capacity = s.capacity();
   s.push_back(0);
   std::size_t new_capacity = s.capacity();
   std::size_t old_storage = old_capacity+1u;
   std::size_t new_storage = new_capacity + 1u;
   BOOST_TEST(new_storage == (old_storage + 3*old_storage/5));
}

void test_growth_factor_100()
{
   #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
   using options_t = string_options_t< growth_factor<growth_factor_100> >;
   #else
   typedef string_options
      < growth_factor<growth_factor_100> >::type options_t;
   #endif

   basic_string<char, std::char_traits<char>, void, options_t> s;

   s.resize(5);
   s.resize(s.capacity());
   std::size_t old_capacity = s.capacity();
   s.push_back(0);
   std::size_t new_capacity = s.capacity();
   std::size_t old_storage = old_capacity+1u;
   std::size_t new_storage = new_capacity + 1u;
   BOOST_TEST(new_storage == 2u*old_storage);
}

int main()
{
   test_growth_factor_50();
   test_growth_factor_60();
   test_growth_factor_100();
   test_stored_size_type<unsigned char>();
   test_stored_size_type<unsigned short>();

   return ::boost::report_errors();
}
