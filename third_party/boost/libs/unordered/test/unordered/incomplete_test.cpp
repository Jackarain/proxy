
// Copyright 2009 Daniel James.
// Copyright 2022-2023 Christian Mazakas.
// Copyright 2025 Joaquin M Lopez Munoz
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "../helpers/unordered.hpp"
#ifdef BOOST_UNORDERED_CFOA_TESTS
#include <boost/unordered/concurrent_flat_set.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/unordered/concurrent_node_map.hpp>
#include <boost/unordered/concurrent_node_set.hpp>
#endif


#include <utility>

namespace x {
  struct D
  {
#if defined(BOOST_UNORDERED_CFOA_TESTS)
    boost::concurrent_flat_map<D, D> x;
    boost::concurrent_node_map<D, D> y;
#elif defined(BOOST_UNORDERED_FOA_TESTS)
    boost::unordered_flat_map<D, D> x;
    boost::unordered_node_map<D, D> y;
#else
    boost::unordered_map<D, D> x;
#endif
  };
} // namespace x

namespace incomplete_test {
  // Declare, but don't define some types.

  struct value;
  struct hash;
  struct equals;
  template <class T> struct allocator;

  // Declare some instances

#if defined(BOOST_UNORDERED_CFOA_TESTS)
  typedef boost::concurrent_flat_map<value, value, hash, equals,
    allocator<std::pair<value const, value> > >
    map1;
  typedef boost::concurrent_flat_set<value, hash, equals, allocator<value> > set1;
  typedef boost::concurrent_node_map<value, value, hash, equals,
    allocator<std::pair<value const, value> > >
    map2;
  typedef boost::concurrent_node_set<value, hash, equals, allocator<value> >
    set2;
#elif defined(BOOST_UNORDERED_FOA_TESTS)
  typedef boost::unordered_flat_map<value, value, hash, equals,
    allocator<std::pair<value const, value> > >
    map1;
  typedef boost::unordered_flat_set<value, hash, equals, allocator<value> > set1;
  typedef boost::unordered_node_map<value, value, hash, equals,
    allocator<std::pair<value const, value> > >
    map2;
  typedef boost::unordered_node_set<value, hash, equals, allocator<value> >
    set2;
#else
  typedef boost::unordered_map<value, value, hash, equals,
    allocator<std::pair<value const, value> > >
    map1;
  typedef boost::unordered_multimap<value, value, hash, equals,
    allocator<std::pair<value const, value> > >
    map2;
  typedef boost::unordered_set<value, hash, equals, allocator<value> > set1;
  typedef boost::unordered_multiset<value, hash, equals, allocator<value> >
    set2;
#endif

  // Now define the types which are stored as members, as they are needed for
  // declaring struct members.

  struct hash
  {
    template <typename T> std::size_t operator()(T const&) const { return 0; }
  };

  struct equals
  {
    template <typename T> bool operator()(T const&, T const&) const
    {
      return true;
    }
  };

  // This is a dubious way to implement an allocator, but good enough
  // for this test.
  template <typename T> struct allocator : std::allocator<T>
  {
    allocator() {}

    template <typename T2>
    allocator(const allocator<T2>& other) : std::allocator<T>(other)
    {
    }

    template <typename T2>
    allocator(const std::allocator<T2>& other) : std::allocator<T>(other)
    {
    }
  };

  // Declare some members of a structs.
  //
  // Incomplete hash, equals and allocator aren't here supported at the
  // moment.
#ifdef BOOST_UNORDERED_FOA_TESTS
  struct struct1
  {
    boost::unordered_flat_map<struct1, struct1, hash, equals,
      allocator<std::pair<struct1 const, struct1> > >
      x;
  };
  struct struct2
  {
    boost::unordered_node_map<struct2, struct2, hash, equals,
      allocator<std::pair<struct2 const, struct2> > >
      x;
  };
  struct struct3
  {
    boost::unordered_flat_set<struct3, hash, equals, allocator<struct3> > x;
  };
  struct struct4
  {
    boost::unordered_node_set<struct4, hash, equals, allocator<struct4> > x;
  };
#else
  struct struct1
  {
    boost::unordered_map<struct1, struct1, hash, equals,
      allocator<std::pair<struct1 const, struct1> > >
      x;
  };
  struct struct2
  {
    boost::unordered_multimap<struct2, struct2, hash, equals,
      allocator<std::pair<struct2 const, struct2> > >
      x;
  };
  struct struct3
  {
    boost::unordered_set<struct3, hash, equals, allocator<struct3> > x;
  };
  struct struct4
  {
    boost::unordered_multiset<struct4, hash, equals, allocator<struct4> > x;
  };
#endif

  // Create some instances.

  incomplete_test::map1 m1;
#ifndef BOOST_UNORDERED_CFOA_TESTS
  incomplete_test::map1::iterator itm1;
  incomplete_test::map1::const_iterator citm1;
#ifndef BOOST_UNORDERED_FOA_TESTS
  incomplete_test::map1::local_iterator litm1;
  incomplete_test::map1::const_local_iterator clitm1;
#endif
#endif

  incomplete_test::map2 m2;
#ifndef BOOST_UNORDERED_CFOA_TESTS
  incomplete_test::map2::iterator itm2;
  incomplete_test::map2::const_iterator citm2;
#ifndef BOOST_UNORDERED_FOA_TESTS
  incomplete_test::map2::local_iterator litm2;
  incomplete_test::map2::const_local_iterator clitm2;
#endif
#endif

  incomplete_test::set1 s1;
#ifndef BOOST_UNORDERED_CFOA_TESTS
  incomplete_test::set1::iterator its1;
  incomplete_test::set1::const_iterator cits1;
#ifndef BOOST_UNORDERED_FOA_TESTS
  incomplete_test::set1::local_iterator lits1;
  incomplete_test::set1::const_local_iterator clits1;
#endif
#endif

  incomplete_test::set2 s2;
#ifndef BOOST_UNORDERED_CFOA_TESTS
  incomplete_test::set2::iterator its2;
  incomplete_test::set2::const_iterator cits2;
#ifndef BOOST_UNORDERED_FOA_TESTS
  incomplete_test::set2::local_iterator lits2;
  incomplete_test::set2::const_local_iterator clits2;
#endif
#endif

  incomplete_test::struct1 c1;
  incomplete_test::struct2 c2;
  incomplete_test::struct3 c3;
  incomplete_test::struct4 c4;

  // Now define the value type.

  struct value
  {
  };

  // Now declare, but don't define, the operators required for comparing
  // elements.

  std::size_t hash_value(value const&);
  bool operator==(value const&, value const&);

  std::size_t hash_value(struct1 const&);
  std::size_t hash_value(struct2 const&);
  std::size_t hash_value(struct3 const&);
  std::size_t hash_value(struct4 const&);

  bool operator==(struct1 const&, struct1 const&);
  bool operator==(struct2 const&, struct2 const&);
  bool operator==(struct3 const&, struct3 const&);
  bool operator==(struct4 const&, struct4 const&);

  // And finally use these

  void use_types()
  {
    incomplete_test::value x;
    m1.insert(std::make_pair(x, x));
#ifndef BOOST_UNORDERED_CFOA_TESTS
    itm1 = m1.begin();
    citm1 = m1.cbegin();
#ifndef BOOST_UNORDERED_FOA_TESTS
    litm1 = m1.begin(0);
    clitm1 = m1.cbegin(0);
#endif
#endif

    m2.insert(std::make_pair(x, x));
#ifndef BOOST_UNORDERED_CFOA_TESTS
    itm2 = m2.begin();
    citm2 = m2.cbegin();
#ifndef BOOST_UNORDERED_FOA_TESTS
    litm2 = m2.begin(0);
    clitm2 = m2.cbegin(0);
#endif
#endif

    s1.insert(x);
#ifndef BOOST_UNORDERED_CFOA_TESTS
    its1 = s1.begin();
    cits1 = s1.cbegin();
#ifndef BOOST_UNORDERED_FOA_TESTS
    lits1 = s1.begin(0);
    clits1 = s1.cbegin(0);
#endif
#endif

    s2.insert(x);
#ifndef BOOST_UNORDERED_CFOA_TESTS
    its2 = s2.begin();
    cits2 = s2.cbegin();
#ifndef BOOST_UNORDERED_FOA_TESTS
    lits2 = s2.begin(0);
    clits2 = s2.cbegin(0);
#endif
#endif
    c1.x.insert(std::make_pair(c1, c1));
    c2.x.insert(std::make_pair(c2, c2));
    c3.x.insert(c3);
    c4.x.insert(c4);
  }

  // And finally define the operators required for comparing elements.

  std::size_t hash_value(value const&) { return 0; }
  bool operator==(value const&, value const&) { return true; }

  std::size_t hash_value(struct1 const&) { return 0; }
  std::size_t hash_value(struct2 const&) { return 0; }
  std::size_t hash_value(struct3 const&) { return 0; }
  std::size_t hash_value(struct4 const&) { return 0; }

  bool operator==(struct1 const&, struct1 const&) { return true; }
  bool operator==(struct2 const&, struct2 const&) { return true; }
  bool operator==(struct3 const&, struct3 const&) { return true; }
  bool operator==(struct4 const&, struct4 const&) { return true; }
} // namespace incomplete_test

int main()
{
  // This could just be a compile test, but I like to be able to run these
  // things. It's probably irrational, but I find it reassuring.

  incomplete_test::use_types();
}
