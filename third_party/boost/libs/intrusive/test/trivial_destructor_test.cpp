/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2025
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#include <boost/intrusive/detail/workaround.hpp>

//Conditional triviality is based on destructor overloads based on concepts
#if defined(BOOST_INTRUSIVE_CONCEPTS_BASED_OVERLOADING)

#include <type_traits>

#include <boost/intrusive/list.hpp>
#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/set.hpp>
#include <boost/intrusive/unordered_set.hpp>
#include <boost/intrusive/splay_set.hpp>
#include <boost/intrusive/avl_set.hpp>
#include <boost/intrusive/sg_set.hpp>
#include <boost/intrusive/treap_set.hpp>
#include <boost/intrusive/bs_set.hpp>
#include <boost/intrusive/pointer_traits.hpp>
#include <boost/intrusive/any_hook.hpp>

using namespace boost::intrusive;

typedef list_base_hook
   < void_pointer<void*>, link_mode<normal_link> > list_base_hook_t;
typedef slist_base_hook
   < void_pointer<void*>, link_mode<normal_link> > slist_base_hook_t;
typedef set_base_hook
   < void_pointer<void*>, link_mode<normal_link> > set_base_hook_t;
typedef avl_set_base_hook
   < void_pointer<void*>, link_mode<normal_link> > avl_base_hook_t;
typedef bs_set_base_hook
   < void_pointer<void*>, link_mode<normal_link> > bs_base_hook_t;
typedef unordered_set_base_hook
   < void_pointer<void*>, link_mode<normal_link> > unordered_base_hook_t;
typedef any_base_hook
   < void_pointer<void*>, link_mode<normal_link> > any_base_hook_t;

BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<list_base_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<slist_base_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<set_base_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<avl_base_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<bs_base_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<unordered_base_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<any_base_hook_t> ));

BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_default_constructible_v<list_base_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_default_constructible_v<slist_base_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_default_constructible_v<set_base_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_default_constructible_v<avl_base_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_default_constructible_v<bs_base_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_default_constructible_v<unordered_base_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_default_constructible_v<any_base_hook_t> ));

typedef list_member_hook
   < void_pointer<void*>, link_mode<normal_link> > list_member_hook_t;
typedef slist_member_hook
   < void_pointer<void*>, link_mode<normal_link> > slist_member_hook_t;
typedef set_member_hook
   < void_pointer<void*>, link_mode<normal_link> > set_member_hook_t;
typedef avl_set_member_hook
   < void_pointer<void*>, link_mode<normal_link> > avl_member_hook_t;
typedef bs_set_member_hook
   < void_pointer<void*>, link_mode<normal_link> > bs_member_hook_t;
typedef unordered_set_member_hook
   < void_pointer<void*>, link_mode<normal_link> > unordered_member_hook_t;
typedef any_member_hook
   < void_pointer<void*>, link_mode<normal_link> > any_member_hook_t;

BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<list_member_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<slist_member_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<set_member_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<avl_member_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<bs_member_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<unordered_member_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<any_member_hook_t> ));

BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_default_constructible_v<list_member_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_default_constructible_v<slist_member_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_default_constructible_v<set_member_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_default_constructible_v<avl_member_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_default_constructible_v<bs_member_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_default_constructible_v<unordered_member_hook_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_default_constructible_v<any_member_hook_t> ));

struct MyType
   : public list_base_hook_t
   , public slist_base_hook_t
   , public set_base_hook_t
   , public avl_base_hook_t
   , public bs_base_hook_t
   , public unordered_base_hook_t
   , public any_base_hook_t
{
   list_member_hook_t      limh;
   slist_member_hook_t     slmh;
   set_member_hook_t       semh;
   avl_member_hook_t       avmh;
   bs_member_hook_t        bsmh;
   unordered_member_hook_t unmh;
   any_member_hook_t       anmh;

   friend bool operator<(const MyType &, const MyType &)          {   return true;   }
   friend std::size_t hash_value(const MyType &, const MyType &)  {   return 0u;     }
};

BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<MyType> ));

typedef list<MyType> list_t;
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<list_t> ));

typedef slist<MyType> slist_t;
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<slist_t> ));

typedef set<MyType> set_t;
typedef multiset<MyType> multiset_t;
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<set_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<multiset_t> ));

typedef avl_set<MyType> avl_set_t;
typedef avl_multiset<MyType> avl_multiset_t;
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<avl_set_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<avl_multiset_t> ));

typedef bs_set<MyType> bs_set_t;
typedef bs_multiset<MyType> bs_multiset_t;
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<bs_set_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<bs_multiset_t> ));

typedef sg_set<MyType> sg_set_t;
typedef sg_multiset<MyType> sg_multiset_t;
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<sg_set_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<sg_multiset_t> ));

typedef treap_set<MyType> treap_set_t;
typedef treap_multiset<MyType> treap_multiset_t;
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<treap_set_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<treap_multiset_t> ));

typedef treap_set<MyType> treap_set_t;
typedef treap_multiset<MyType> treap_multiset_t;
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<treap_set_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<treap_multiset_t> ));

typedef unordered_set<MyType> unordered_set_t;
typedef unordered_multiset<MyType> unordered_multiset_t;
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<unordered_set_t> ));
BOOST_INTRUSIVE_STATIC_ASSERT(( std::is_trivially_destructible_v<unordered_multiset_t> ));

#endif

int main()
{
   return 0;
}
