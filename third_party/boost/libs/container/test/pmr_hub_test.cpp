/* Copyright 2025-2026 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/container for documentation.
 */

#include <boost/config.hpp>

#if BOOST_CXX_VERSION < 201103L

int main() { return 0; }

#else

#include <boost/container/pmr/hub.hpp>
#include <boost/container/detail/type_traits.hpp>

int main()
{
   using namespace boost::container;
   using boost::container::dtl::is_same;

   typedef hub<int, pmr::polymorphic_allocator<int> > intcontainer_t;
   BOOST_CONTAINER_STATIC_ASSERT(( is_same<intcontainer_t, pmr::hub_of<int>::type >::value ));
   #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
      BOOST_CONTAINER_STATIC_ASSERT(( is_same<intcontainer_t, pmr::hub<int> >::value ));
   #endif

   intcontainer_t cont(pmr::get_default_resource());
   typedef intcontainer_t::value_type value_type;
   cont.insert(value_type());
   return 0;
}

#endif   //BOOST_CXX_VERSION < 201103L
