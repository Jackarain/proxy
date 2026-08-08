/* Boost.MultiIndex test for memory relocatability.
 *
 * Copyright 2003-2026 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#include "test_mmap.hpp"

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/detail/lightweight_test.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ranked_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <iterator>
#include <memory>
#include <string>

void test_mmap()
{
  using namespace boost::multi_index;
  namespace bip=boost::interprocess;
  using container=multi_index_container<
    int,
    indexed_by<
      ordered_unique<identity<int>>,
      hashed_unique<identity<int>>,
      ranked_unique<identity<int>>,
      sequenced<>,
      random_access<>
    >,
    bip::allocator<int,bip::managed_mapped_file::segment_manager>
  >;
  using iterator0=nth_index<container,0>::type::iterator;
  using iterator1=nth_index<container,1>::type::iterator;
  using local_iterator1=nth_index<container,1>::type::local_iterator;
  using iterator2=nth_index<container,2>::type::iterator;
  using iterator3=nth_index<container,3>::type::iterator;
  using iterator4=nth_index<container,4>::type::iterator;
  constexpr auto container_name="container";

  static auto file_name_str= 
    std::string("./test_mmap_")+
    to_string(boost::uuids::random_generator()());
  static auto file_name=file_name_str.c_str();
  
  bip::file_mapping::remove(file_name);
  std::unique_ptr<bip::managed_mapped_file> pseg1(
    new bip::managed_mapped_file(bip::create_only,file_name,65536));
  container& c1=*pseg1->construct<container>(container_name)(
    container::ctor_args_list(),
    container::allocator_type(pseg1->get_segment_manager()));
  for(int i=0;i<100;++i)c1.insert(i);
  auto v0=**pseg1->construct<iterator0>("it0")(c1.get<0>().begin());
  auto v1=**pseg1->construct<iterator1>("it1")(c1.get<1>().begin());
  auto lv1=**pseg1->construct<local_iterator1>("lit1")(c1.get<1>().begin(0));
  auto v2=**pseg1->construct<iterator2>("it2")(c1.get<2>().begin());
  auto v3=**pseg1->construct<iterator3>("it3")(c1.get<3>().begin());
  auto v4=**pseg1->construct<iterator4>("it4")(c1.get<4>().begin());

  bip::managed_mapped_file seg2(bip::open_only,file_name);
  pseg1=nullptr;
  container& c2=*seg2.find<container>(container_name).first;
  auto it0=*seg2.find<iterator0>("it0").first;
  auto it1=*seg2.find<iterator1>("it1").first;
  auto lit1=*seg2.find<local_iterator1>("lit1").first;
  auto it2=*seg2.find<iterator2>("it2").first;
  auto it3=*seg2.find<iterator3>("it3").first;
  auto it4=*seg2.find<iterator4>("it4").first;
  BOOST_TEST_EQ(*it0,v0);
  BOOST_TEST_EQ(std::distance(it0,c2.get<0>().end()),c2.size());
  BOOST_TEST_EQ(*it1,v1);
  BOOST_TEST_EQ(std::distance(it1,c2.get<1>().end()),c2.size());
  BOOST_TEST_EQ(*lit1,lv1);
  BOOST_TEST_EQ(
    std::distance(lit1,c2.get<1>().end(0)),c2.get<1>().bucket_size(0));
  BOOST_TEST_EQ(*it2,v2);
  BOOST_TEST_EQ(std::distance(it2,c2.get<2>().end()),c2.size());
  BOOST_TEST_EQ(*it3,v3);
  BOOST_TEST_EQ(std::distance(it3,c2.get<3>().end()),c2.size());
  BOOST_TEST_EQ(*it4,v4);
  BOOST_TEST_EQ(std::distance(it4,c2.get<4>().end()),c2.size());
}
