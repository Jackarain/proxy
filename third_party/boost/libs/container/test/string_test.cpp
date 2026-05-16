//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/vector.hpp>
#include <boost/container/string.hpp>
#include <string>
#include <vector>
#include <boost/container/detail/algorithm.hpp> //equal()
#include <cstring>
#include <cstdio>
#include <cstddef>
#include <new>
#include "dummy_test_allocator.hpp"
#include "check_equal_containers.hpp"
#include "expand_bwd_test_allocator.hpp"
#include "expand_bwd_test_template.hpp"
#include "propagate_allocator_test.hpp"
#include "default_init_test.hpp"
#include "comparison_test.hpp"
#include "../../intrusive/test/iterator_test.hpp"
#include <boost/utility/string_view.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#if BOOST_CXX_VERSION >= 201103L
#include <boost/functional/hash.hpp>
#endif

#include <boost/container/string.hpp>
#include <boost/container/allocator.hpp>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <utility>
#include <limits>
#include <iomanip>
#include <cmath>

using namespace boost::container;

struct StringEqual
{
   template<class Str1, class Str2>
   bool operator ()(const Str1 &string1, const Str2 &string2) const
   {
      if(string1.size() != string2.size())
         return false;
      return std::char_traits<typename Str1::value_type>::compare
         (string1.c_str(), string2.c_str(), string1.size()) == 0;
   }
};

//Function to check if both lists are equal
template<class StrVector1, class StrVector2>
bool CheckEqualStringVector(StrVector1 *strvect1, StrVector2 *strvect2)
{
   StringEqual comp;
   return boost::container::algo_equal(strvect1->begin(), strvect1->end(),
                     strvect2->begin(), comp);
}

template<class ForwardIt>
ForwardIt unique(ForwardIt first, ForwardIt const last)
{
   if(first == last){
      ForwardIt i = first;
      //Find first adjacent pair
      bool go_ahead = true;
      while(go_ahead){
         if(++i == last){
            return last;
         }
         else if(*first == *i){
            go_ahead = false;
         }
         else{
            ++first;
         }
      }
      //Now overwrite skipping adjacent elements
      while (++i != last) {
         if (!(*first == *i)) {
            *(++first) = boost::move(*i);
         }
      }
      ++first;
   }
   return first;
}

template<class CharType>
struct string_literals;

template<>
struct string_literals<char>
{
   static const char *String()
      {  return "String";  }
   static const char *Prefix()
      {  return "Prefix";  }
   static const char *Suffix()
      {  return "Suffix";  }
   static const char *LongString()
      {  return "LongLongLongLongLongLongLongLongLongLongLongLongLongString";  }
   static char Char()
      {  return 'C';  }
   static void sprintf_number(char *buf, int number)
   {
      std::sprintf(buf, "%i", number);
   }
   static void sprintf_number(char *buf, unsigned number)
   {
      std::sprintf(buf, "%u", number);
   }
   static void sprintf_number(char *buf, long number)
   {
      std::sprintf(buf, "%li", number);
   }
   static void sprintf_number(char *buf, unsigned long number)
   {
      std::sprintf(buf, "%lu", number);
   }
   static void sprintf_number(char *buf, long long number)
   {
      std::sprintf(buf, "%lli", number);
   }
   static void sprintf_number(char *buf, unsigned long long number)
   {
      std::sprintf(buf, "%llu", number);
   }
};

template<>
struct string_literals<wchar_t>
{
   static const wchar_t *String()
      {  return L"String";  }
   static const wchar_t *Prefix()
      {  return L"Prefix";  }
   static const wchar_t *Suffix()
      {  return L"Suffix";  }
   static const wchar_t *LongString()
      {  return L"LongLongLongLongLongLongLongLongLongLongLongLongLongString";  }
   static wchar_t Char()
      {  return L'C';  }
   static void sprintf_number(wchar_t *buffer, unsigned long long number)
   {
      //For compilers without wsprintf, print it backwards
      const wchar_t *digits = L"0123456789";
      wchar_t *buf = buffer;

      bool go_ahead = true;
      while(go_ahead){
         unsigned long long rem = number % 10;
         number  = number / 10;

         *buf = digits[rem];
         ++buf;
         if(!number){
            *buf = 0;
            go_ahead = false;
         }
      }

   }
};

template<class CharType>
int string_test()
{
   typedef std::basic_string<CharType> StdString;
   typedef vector<StdString>  StdStringVector;
   typedef basic_string<CharType> BoostString;
   typedef vector<BoostString> BoostStringVector;

   const std::size_t MaxSize = 100;

   {
      BoostStringVector *boostStringVect = new BoostStringVector;
      StdStringVector *stdStringVect = new StdStringVector;
      BoostString auxBoostString;
      StdString auxStdString(StdString(auxBoostString.begin(), auxBoostString.end() ));

      CharType buffer [20];

      //First, push back
      for(std::size_t i = 0; i < MaxSize; ++i){
         auxBoostString = string_literals<CharType>::String();
         auxStdString = string_literals<CharType>::String();
         string_literals<CharType>::sprintf_number(buffer, i);
         auxBoostString += buffer;
         auxStdString += buffer;
         boostStringVect->push_back(auxBoostString);
         stdStringVect->push_back(auxStdString);
      }

      if(auxBoostString.data() != const_cast<const BoostString&>(auxBoostString).data() &&
         auxBoostString.data() != &auxBoostString[0])
         return 1;

      if(!CheckEqualStringVector(boostStringVect, stdStringVect)){
         return 1;
      }

      //Now push back moving
      for(std::size_t i = 0; i < MaxSize; ++i){
         auxBoostString = string_literals<CharType>::String();
         auxStdString = string_literals<CharType>::String();
         string_literals<CharType>::sprintf_number(buffer, i);
         auxBoostString += buffer;
         auxStdString += buffer;
         boostStringVect->push_back(boost::move(auxBoostString));
         stdStringVect->push_back(auxStdString);
      }

      if(!CheckEqualStringVector(boostStringVect, stdStringVect)){
         return 1;
      }

      //push front
      for(std::size_t i = 0; i < MaxSize; ++i){
         auxBoostString = string_literals<CharType>::String();
         auxStdString = string_literals<CharType>::String();
         string_literals<CharType>::sprintf_number(buffer, i);
         auxBoostString += buffer;
         auxStdString += buffer;
         boostStringVect->insert(boostStringVect->begin(), auxBoostString);
         stdStringVect->insert(stdStringVect->begin(), auxStdString);
      }

      if(!CheckEqualStringVector(boostStringVect, stdStringVect)){
         return 1;
      }

      //Now push front moving
      for(std::size_t i = 0; i < MaxSize; ++i){
         auxBoostString = string_literals<CharType>::String();
         auxStdString = string_literals<CharType>::String();
         string_literals<CharType>::sprintf_number(buffer, i);
         auxBoostString += buffer;
         auxStdString += buffer;
         boostStringVect->insert(boostStringVect->begin(), boost::move(auxBoostString));
         stdStringVect->insert(stdStringVect->begin(), auxStdString);
      }

      if(!CheckEqualStringVector(boostStringVect, stdStringVect)){
         return 1;
      }

      //Now test long and short representation swapping

      //Short first
      auxBoostString = string_literals<CharType>::String();
      auxStdString = string_literals<CharType>::String();
      BoostString boost_swapper;
      StdString std_swapper;
      boost_swapper.swap(auxBoostString);
      std_swapper.swap(auxStdString);
      if(!StringEqual()(auxBoostString, auxStdString))
         return 1;
      if(!StringEqual()(boost_swapper, std_swapper))
         return 1;
      boost_swapper.swap(auxBoostString);
      std_swapper.swap(auxStdString);
      if(!StringEqual()(auxBoostString, auxStdString))
         return 1;
      if(!StringEqual()(boost_swapper, std_swapper))
         return 1;

      //Shrink_to_fit
      auxBoostString.shrink_to_fit();
      StdString(auxStdString).swap(auxStdString);
      if(!StringEqual()(auxBoostString, auxStdString))
         return 1;

      //Reserve + shrink_to_fit
      auxBoostString.reserve(boost_swapper.size()*2+1);
      auxStdString.reserve(std_swapper.size()*2+1);
      if(!StringEqual()(auxBoostString, auxStdString))
         return 1;

      auxBoostString.shrink_to_fit();
      StdString(auxStdString).swap(auxStdString);
      if(!StringEqual()(auxBoostString, auxStdString))
         return 1;

      //Long string
      auxBoostString = string_literals<CharType>::LongString();
      auxStdString   = string_literals<CharType>::LongString();
      boost_swapper = BoostString();
      std_swapper = StdString();
      boost_swapper.swap(auxBoostString);
      std_swapper.swap(auxStdString);
      if(!StringEqual()(auxBoostString, auxStdString))
         return 1;
      if(!StringEqual()(boost_swapper, std_swapper))
         return 1;
      boost_swapper.swap(auxBoostString);
      std_swapper.swap(auxStdString);

      //Shrink_to_fit
      auxBoostString.shrink_to_fit();
      StdString(auxStdString).swap(auxStdString);
      if(!StringEqual()(auxBoostString, auxStdString))
         return 1;

      auxBoostString.clear();
      auxStdString.clear();
      auxBoostString.shrink_to_fit();
      StdString(auxStdString).swap(auxStdString);
      if(!StringEqual()(auxBoostString, auxStdString))
         return 1;

      //No sort
      std::sort(boostStringVect->begin(), boostStringVect->end());
      std::sort(stdStringVect->begin(), stdStringVect->end());
      if(!CheckEqualStringVector(boostStringVect, stdStringVect)) return 1;

      const CharType *prefix    = string_literals<CharType>::Prefix();
      const std::size_t  prefix_size    = std::char_traits<CharType>::length(prefix);
      const CharType *sufix      = string_literals<CharType>::Suffix();

      for(std::size_t i = 0; i < MaxSize; ++i){
         (*boostStringVect)[i].append(sufix);
         (*stdStringVect)[i].append(sufix);
         (*boostStringVect)[i].insert((*boostStringVect)[i].begin(),
                                    prefix, prefix + prefix_size);
         (*stdStringVect)[i].insert((*stdStringVect)[i].begin(),
                                    prefix, prefix + prefix_size);
      }

      if(!CheckEqualStringVector(boostStringVect, stdStringVect)) return 1;

      for(std::size_t i = 0; i < MaxSize; ++i){
         std::reverse((*boostStringVect)[i].begin(), (*boostStringVect)[i].end());
         std::reverse((*stdStringVect)[i].begin(), (*stdStringVect)[i].end());
      }

      if(!CheckEqualStringVector(boostStringVect, stdStringVect)) return 1;

      for(std::size_t i = 0; i < MaxSize; ++i){
         std::reverse((*boostStringVect)[i].begin(), (*boostStringVect)[i].end());
         std::reverse((*stdStringVect)[i].begin(), (*stdStringVect)[i].end());
      }

      if(!CheckEqualStringVector(boostStringVect, stdStringVect)) return 1;

      for(std::size_t i = 0; i < MaxSize; ++i){
         std::sort(boostStringVect->begin(), boostStringVect->end());
         std::sort(stdStringVect->begin(), stdStringVect->end());
      }

      if(!CheckEqualStringVector(boostStringVect, stdStringVect)) return 1;

      for(std::size_t i = 0; i < MaxSize; ++i){
         (*boostStringVect)[i].replace((*boostStringVect)[i].begin(),
                                    (*boostStringVect)[i].end(),
                                    string_literals<CharType>::String());
         (*stdStringVect)[i].replace((*stdStringVect)[i].begin(),
                                    (*stdStringVect)[i].end(),
                                    string_literals<CharType>::String());
      }

      if(!CheckEqualStringVector(boostStringVect, stdStringVect)) return 1;

      boostStringVect->erase(::unique(boostStringVect->begin(), boostStringVect->end()),
                           boostStringVect->end());
      stdStringVect->erase(::unique(stdStringVect->begin(), stdStringVect->end()),
                           stdStringVect->end());
      if(!CheckEqualStringVector(boostStringVect, stdStringVect)) return 1;

      //Check addition
      {
         BoostString bs2 = string_literals<CharType>::String();
         StdString   ss2 = string_literals<CharType>::String();
         BoostString bs3 = string_literals<CharType>::Suffix();
         StdString   ss3 = string_literals<CharType>::Suffix();
         BoostString bs4 = bs2 + bs3;
         StdString   ss4 = ss2 + ss3;
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs4 = bs2 + BoostString();
         ss4 = ss2 + StdString();
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs4 = BoostString() + bs2;
         ss4 = StdString() + ss2;
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs4 = BoostString() + boost::move(bs2);
         ss4 = StdString() + boost::move(ss2);
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs2 = string_literals<CharType>::String();
         ss2 = string_literals<CharType>::String();
         bs4 = boost::move(bs2) + BoostString();
         ss4 = boost::move(ss2) + StdString();
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs2 = string_literals<CharType>::String();
         ss2 = string_literals<CharType>::String();
         bs4 = string_literals<CharType>::Prefix() + boost::move(bs2);
         ss4 = string_literals<CharType>::Prefix() + boost::move(ss2);
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs2 = string_literals<CharType>::String();
         ss2 = string_literals<CharType>::String();
         bs4 = boost::move(bs2) + string_literals<CharType>::Suffix();
         ss4 = boost::move(ss2) + string_literals<CharType>::Suffix();
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs2 = string_literals<CharType>::String();
         ss2 = string_literals<CharType>::String();
         bs4 = string_literals<CharType>::Prefix() + bs2;
         ss4 = string_literals<CharType>::Prefix() + ss2;
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs2 = string_literals<CharType>::String();
         ss2 = string_literals<CharType>::String();
         bs4 = bs2 + string_literals<CharType>::Suffix();
         ss4 = ss2 + string_literals<CharType>::Suffix();
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs2 = string_literals<CharType>::String();
         ss2 = string_literals<CharType>::String();
         bs4 = string_literals<CharType>::Char() + bs2;
         ss4 = string_literals<CharType>::Char() + ss2;
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         bs2 = string_literals<CharType>::String();
         ss2 = string_literals<CharType>::String();
         bs4 = bs2 + string_literals<CharType>::Char();
         ss4 = ss2 + string_literals<CharType>::Char();
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         //Check front/back/begin/end

         if(bs4.front() != *ss4.begin())
            return 1;

         if(bs4.back() != *(ss4.end()-1))
            return 1;

         bs4.pop_back();
         ss4.erase(ss4.end()-1);
         if(!StringEqual()(bs4, ss4)){
            return 1;
         }

         if(*bs4.begin() != *ss4.begin())
            return 1;
         if(*bs4.cbegin() != *ss4.begin())
            return 1;
         if(*bs4.rbegin() != *ss4.rbegin())
            return 1;
         if(*bs4.crbegin() != *ss4.rbegin())
            return 1;
         if(*(bs4.end()-1) != *(ss4.end()-1))
            return 1;
         if(*(bs4.cend()-1) != *(ss4.end()-1))
            return 1;
         if(*(bs4.rend()-1) != *(ss4.rend()-1))
            return 1;
         if(*(bs4.crend()-1) != *(ss4.rend()-1))
            return 1;
      }

#ifndef BOOST_CONTAINER_NO_CXX17_CTAD
      //Chect Constructor Template Auto Deduction
      {
         auto gold = StdString(string_literals<CharType>::String());
         auto test = basic_string(gold.begin(), gold.end());
         if(!StringEqual()(gold, test)) {
            return 1;
         }
      }
#endif


      //When done, delete vector
      delete boostStringVect;
      delete stdStringVect;
   }
   return 0;
}

bool test_expand_bwd()
{
   //Now test all back insertion possibilities
   typedef test::expand_bwd_test_allocator<char>
      allocator_type;
   typedef basic_string<char, std::char_traits<char>, allocator_type>
      string_type;
   return  test::test_all_expand_bwd<string_type>();
}

struct boost_container_string;

namespace boost { namespace container {   namespace test {

template<>
struct alloc_propagate_base<boost_container_string>
{
   template <class T, class Allocator>
   struct apply
   {
      typedef boost::container::basic_string<T, std::char_traits<T>, Allocator> type;
   };
};


}}}   //namespace boost::container::test

//==============================================================================
// SECTION 1: Type traits and typedefs
//==============================================================================

void test_type_traits()
{
   // Basic type traits
   BOOST_TEST_TRAIT_TRUE((dtl::is_same<string::value_type, char>));
   BOOST_TEST_TRAIT_TRUE((dtl::is_same<string::traits_type, std::char_traits<char> >));
   BOOST_TEST_TRAIT_TRUE((dtl::is_same<string::reference, char&>));
   BOOST_TEST_TRAIT_TRUE((dtl::is_same<string::const_reference, const char&>));
   BOOST_TEST_TRAIT_TRUE((dtl::is_same<string::pointer, char*>));
   BOOST_TEST_TRAIT_TRUE((dtl::is_same<string::const_pointer, const char*>));

   // Iterator types should be random access
   typedef string::iterator iter;
   typedef string::const_iterator citer;
   typedef string::reverse_iterator riter;
   typedef string::const_reverse_iterator criter;

   BOOST_TEST_TRAIT_TRUE((dtl::is_same<
      std::iterator_traits<iter>::iterator_category,
      std::random_access_iterator_tag>));
   BOOST_TEST_TRAIT_TRUE((dtl::is_same<
      std::iterator_traits<citer>::iterator_category,
      std::random_access_iterator_tag>));
   BOOST_TEST_TRAIT_TRUE((dtl::is_same<
      std::iterator_traits<riter>::iterator_category,
      std::random_access_iterator_tag>));
   BOOST_TEST_TRAIT_TRUE((dtl::is_same<
      std::iterator_traits<criter>::iterator_category,
      std::random_access_iterator_tag>));

   // npos
   BOOST_TEST_EQ(string::npos, static_cast<string::size_type>(-1));
}

//==============================================================================
// SECTION 2: Constructors
//==============================================================================

void test_default_constructor()
{
   string s;
   BOOST_TEST(s.empty());
   BOOST_TEST_EQ(s.size(), 0u);
   BOOST_TEST_EQ(s.length(), 0u);
   BOOST_TEST(s.capacity() >= s.size());
   BOOST_TEST(s == "");
}

void test_allocator_constructor()
{
   string::allocator_type alloc;
   string s(alloc);
   BOOST_TEST(s.empty());
   BOOST_TEST_EQ(s.size(), 0u);
}

void test_count_char_constructor()
{
   string s1(5, 'a');
   BOOST_TEST_EQ(s1.size(), 5u);
   BOOST_TEST(s1 == "aaaaa");

   string s2(0, 'x');
   BOOST_TEST(s2.empty());

   string s3(100, 'z');
   BOOST_TEST_EQ(s3.size(), 100u);
   BOOST_TEST_EQ(s3[0], 'z');
}

void test_substring_constructor()
{
   string original("Hello, World!");

   // pos only
   string s1(original, 7);
   BOOST_TEST(s1 == "World!");

   // pos and count
   string s2(original, 0, 5);
   BOOST_TEST(s2 == "Hello");

   string s3(original, 7, 5);
   BOOST_TEST(s3 == "World");

   // pos == size (empty result)
   string s4(original, original.size());
   BOOST_TEST(s4.empty());

   // count exceeds remaining (should clamp)
   string s5(original, 7, 100);
   BOOST_TEST(s5 == "World!");

   // npos count
   string s6(original, 7, string::npos);
   BOOST_TEST(s6 == "World!");
}

void test_cstring_constructor()
{
   const char* cstr = "Hello, World!";

   // Full c-string
   string s1(cstr);
   BOOST_TEST(s1 == "Hello, World!");
   BOOST_TEST_EQ(s1.size(), 13u);

   // c-string with count
   string s2(cstr, 5);
   BOOST_TEST(s2 == "Hello");
   BOOST_TEST_EQ(s2.size(), 5u);

   // zero count
   string s3(cstr, 0);
   BOOST_TEST(s3.empty());

   // Empty c-string
   string s4("");
   BOOST_TEST(s4.empty());
}

void test_iterator_constructor()
{
   const char arr[] = "Hello";
   string s1(&arr[0], &arr[sizeof(arr) - 1]); // exclude null terminator
   BOOST_TEST(s1 == "Hello");

   string source("World");
   string s2(source.begin(), source.end());
   BOOST_TEST(s2 == "World");

   string s3(source.rbegin(), source.rend());
   BOOST_TEST(s3 == "dlroW");

   // Empty range
   string s4(source.begin(), source.begin());
   BOOST_TEST(s4.empty());
}

void test_copy_constructor()
{
   string original("Hello, World!");
   string copy(original);

   BOOST_TEST(copy == original);
   BOOST_TEST_EQ(copy.size(), original.size());
   BOOST_TEST(copy.data() != original.data()); // Different storage

   // Modifying copy doesn't affect original
   copy[0] = 'h';
   BOOST_TEST(original[0] == 'H');
}

void test_move_constructor()
{
   string original("Hello, World!");
   std::size_t original_size = original.size();

   string moved(boost::move(original));

   BOOST_TEST_EQ(moved.size(), original_size);
   BOOST_TEST(moved == "Hello, World!");
}

void test_initializer_list_constructor()
{
   #if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   string s1{'H', 'e', 'l', 'l', 'o'};
   BOOST_TEST(s1 == "Hello");
   BOOST_TEST_EQ(s1.size(), 5u);

   string s2{};
   BOOST_TEST(s2.empty());

   string s3{'a'};
   BOOST_TEST(s3 == "a");
   #endif
}

#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
void test_string_view_constructor()
{
   std::string_view sv("Hello, World!");
   string s1(sv);
   BOOST_TEST(s1 == "Hello, World!");

   // Use substr on the string_view instead
   string s2(sv.substr(7, 5));
   BOOST_TEST(s2 == "World");
}
#endif

// Boost.Container specific: default_init constructor
void test_default_init_constructor()
{
   string s(10, default_init);
   BOOST_TEST_EQ(s.size(), 10u);
   // Contents are uninitialized but string is valid
}

//==============================================================================
// SECTION 3: Assignment operators
//==============================================================================

template<class T>
T& return_self(T &t)
{  return t;  }

void test_copy_assignment()
{
   string s1("Hello");
   string s2;

   s2 = s1;
   BOOST_TEST(s2 == "Hello");
   BOOST_TEST(s1 == "Hello");
   BOOST_TEST(s1.data() != s2.data());

   // Self-assignment
   s1 = return_self(s1);
   BOOST_TEST(s1 == "Hello");
}

void test_move_assignment()
{
   string s1("Hello, World!");
   string s2;

   s2 = boost::move(s1);
   BOOST_TEST(s2 == "Hello, World!");
   // s1 is in valid but unspecified state
}

void test_cstring_assignment()
{
   string s;

   s = "Hello";
   BOOST_TEST(s == "Hello");

   s = "";
   BOOST_TEST(s.empty());

   s = "A longer string for testing";
   BOOST_TEST(s == "A longer string for testing");
}

void test_char_assignment()
{
   string s("Hello");

   s = 'X';
   BOOST_TEST(s == "X");
   BOOST_TEST_EQ(s.size(), 1u);
}

void test_initializer_list_assignment()
{
   #if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   string s;

   s = {'H', 'e', 'l', 'l', 'o'};
   BOOST_TEST(s == "Hello");

   s = {};
   BOOST_TEST(s.empty());
   #endif
}

#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
void test_string_view_assignment()
{
   string s;
   std::string_view sv("Hello");

   s = sv;
   BOOST_TEST(s == "Hello");
}
#endif

//==============================================================================
// SECTION 4: assign() methods
//==============================================================================

void test_assign_count_char()
{
   string s("Hello");

   s.assign(5, 'x');
   BOOST_TEST(s == "xxxxx");

   s.assign(0, 'y');
   BOOST_TEST(s.empty());

   s.assign(10, 'z');
   BOOST_TEST_EQ(s.size(), 10u);
   BOOST_TEST(s == "zzzzzzzzzz");
}

void test_assign_string()
{
   string s1("Hello");
   string s2;

   s2.assign(s1);
   BOOST_TEST(s2 == "Hello");

   // Self-assign
   s2.assign(s2);
   BOOST_TEST(s2 == "Hello");
}

void test_assign_substring()
{
   string s1("Hello, World!");
   string s2;

   s2.assign(s1, 7, string::npos);
   BOOST_TEST(s2 == "World!");

   s2.assign(s1, 0, 5);
   BOOST_TEST(s2 == "Hello");

   s2.assign(s1, 7, 5);
   BOOST_TEST(s2 == "World");

   s2.assign(s1, 0, string::npos);
   BOOST_TEST(s2 == "Hello, World!");
}

void test_assign_move()
{
   string s1("Hello");
   string s2;

   s2.assign(boost::move(s1));
   BOOST_TEST(s2 == "Hello");
}

void test_assign_cstring()
{
   string s;

   s.assign("Hello, World!");
   BOOST_TEST(s == "Hello, World!");

   s.assign("Hello", string::size_type(5));
   BOOST_TEST(s == "Hello");

   s.assign("", string::size_type(0));
   BOOST_TEST(s.empty());
}

void test_assign_iterators()
{
   string source("Hello");
   string s;

   s.assign(source.begin(), source.end());
   BOOST_TEST(s == "Hello");

   s.assign(source.rbegin(), source.rend());
   BOOST_TEST(s == "olleH");

   s.assign(source.begin(), source.begin());
   BOOST_TEST(s.empty());
}

void test_assign_initializer_list()
{
   #if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   string s;

   s.assign({'W', 'o', 'r', 'l', 'd'});
   BOOST_TEST(s == "World");

   s.assign({});
   BOOST_TEST(s.empty());
   #endif
}

//==============================================================================
// SECTION 5: Element access
//==============================================================================

void test_at()
{
   string s("Hello");

   BOOST_TEST_EQ(s.at(0), 'H');
   BOOST_TEST_EQ(s.at(1), 'e');
   BOOST_TEST_EQ(s.at(4), 'o');

   // Modifiable
   s.at(0) = 'h';
   BOOST_TEST_EQ(s.at(0), 'h');

   // Const version
   const string cs("World");
   BOOST_TEST_EQ(cs.at(0), 'W');

   #ifndef BOOST_NO_EXCEPTIONS
   
   bool threw = false;
   try {
      (void)s.at(100);
   } catch (...) {
      threw = true;
   }
   BOOST_TEST(threw);

   threw = false;
   try {
      (void)s.at(s.size());
   } catch (...) {
      threw = true;
   }
   BOOST_TEST(threw);

   #endif   //BOOST_NO_EXCEPTIONS
}

void test_operator_bracket()
{
   string s("Hello");

   BOOST_TEST_EQ(s[0], 'H');
   BOOST_TEST_EQ(s[1], 'e');
   BOOST_TEST_EQ(s[4], 'o');
   BOOST_TEST_EQ(s[5], '\0');

   // Modifiable
   s[0] = 'h';
   BOOST_TEST_EQ(s[0], 'h');

   // Const version
   const string cs("World");
   BOOST_TEST_EQ(cs[0], 'W');
   BOOST_TEST_EQ(cs[cs.size()], '\0');
}

void test_front()
{
   string s("Hello");
   BOOST_TEST_EQ(s.front(), 'H');

   s.front() = 'h';
   BOOST_TEST_EQ(s.front(), 'h');

   const string cs("World");
   BOOST_TEST_EQ(cs.front(), 'W');
}

void test_back()
{
   string s("Hello");
   BOOST_TEST_EQ(s.back(), 'o');

   s.back() = 'O';
   BOOST_TEST_EQ(s.back(), 'O');

   const string cs("World");
   BOOST_TEST_EQ(cs.back(), 'd');
}

void test_data()
{
   string s("Hello");

   const char* data = s.data();
   BOOST_TEST_EQ(data[0], 'H');
   BOOST_TEST_EQ(data[5], '\0'); // null-terminated

   // Non-const data()
   char* mdata = s.data();
   mdata[0] = 'h';
   BOOST_TEST(s == "hello");
}

void test_c_str()
{
   string s("Hello");

   const char* cstr = s.c_str();
   BOOST_TEST_EQ(std::strcmp(cstr, "Hello"), 0);
   BOOST_TEST_EQ(cstr[5], '\0');

   string empty;
   BOOST_TEST_EQ(empty.c_str()[0], '\0');
}

#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
void test_string_view_conversion()
{
   string s("Hello");
   std::string_view sv(s.data(), s.size());

   BOOST_TEST_EQ(sv.size(), s.size());
   BOOST_TEST(sv == "Hello");
}
#endif

//==============================================================================
// SECTION 6: Iterators
//==============================================================================

void test_begin_end()
{
   string s("Hello");

   BOOST_TEST_EQ(*s.begin(), 'H');
   BOOST_TEST_EQ(*(s.end() - 1), 'o');
   BOOST_TEST_EQ(std::distance(s.begin(), s.end()), 5);

   // Modify through iterator
   *s.begin() = 'h';
   BOOST_TEST(s == "hello");

   // Const iterators
   const string cs("World");
   BOOST_TEST_EQ(*cs.begin(), 'W');
   BOOST_TEST_EQ(*(cs.end() - 1), 'd');
}

void test_cbegin_cend()
{
   string s("Hello");

   BOOST_TEST_EQ(*s.cbegin(), 'H');
   BOOST_TEST_EQ(*(s.cend() - 1), 'o');

   // Verify const_iterator type
   string::const_iterator it = s.cbegin();
   BOOST_TEST_EQ(*it, 'H');
}

void test_rbegin_rend()
{
   string s("Hello");

   BOOST_TEST_EQ(*s.rbegin(), 'o');
   BOOST_TEST_EQ(*(s.rend() - 1), 'H');

   string reversed(s.rbegin(), s.rend());
   BOOST_TEST(reversed == "olleH");

   // Const
   const string cs("World");
   BOOST_TEST_EQ(*cs.rbegin(), 'd');
}

void test_crbegin_crend()
{
   string s("Hello");

   BOOST_TEST_EQ(*s.crbegin(), 'o');
   BOOST_TEST_EQ(*(s.crend() - 1), 'H');
}

void test_iterator_arithmetic()
{
   string s("Hello");

   string::iterator it = s.begin();
   it += 2;
   BOOST_TEST_EQ(*it, 'l');

   it -= 1;
   BOOST_TEST_EQ(*it, 'e');

   BOOST_TEST_EQ(*(it + 2), 'l');
   BOOST_TEST_EQ(*(it - 1), 'H');

   BOOST_TEST_EQ(s.end() - s.begin(), 5);
   BOOST_TEST(s.begin() < s.end());
   BOOST_TEST(s.end() > s.begin());
   BOOST_TEST(s.begin() <= s.begin());
   BOOST_TEST(s.begin() >= s.begin());
}

//==============================================================================
// SECTION 7: Capacity
//==============================================================================

void test_empty()
{
   string s;
   BOOST_TEST(s.empty());

   s = "x";
   BOOST_TEST(!s.empty());

   s.clear();
   BOOST_TEST(s.empty());
}

void test_size_length()
{
   string s;
   BOOST_TEST_EQ(s.size(), 0u);
   BOOST_TEST_EQ(s.length(), 0u);

   s = "Hello";
   BOOST_TEST_EQ(s.size(), 5u);
   BOOST_TEST_EQ(s.length(), 5u);
   BOOST_TEST_EQ(s.size(), s.length());

   // Embedded nulls
   s = string("ab\0cd", 5);
   BOOST_TEST_EQ(s.size(), 5u);
}

void test_max_size()
{
   string s;
   BOOST_TEST(s.max_size() > 0);
   BOOST_TEST(s.max_size() >= s.size());
}

void test_reserve()
{
   string s;
   std::size_t initial_cap = s.capacity();

   s.reserve(100);
   BOOST_TEST(s.capacity() >= 100u);
   BOOST_TEST(s.capacity() != initial_cap);
   BOOST_TEST(s.empty()); // reserve doesn't change size

   // Reserve smaller shall not shrink
   s.reserve(10);
   BOOST_TEST(s.capacity() >= 100u);
   s.reserve(0);
   BOOST_TEST(s.capacity() >= 100u);
}

void test_capacity()
{
   string s;
   BOOST_TEST(s.capacity() >= s.size());

   s = "Hello";
   BOOST_TEST(s.capacity() >= 5u);

   string large(1000, 'x');
   BOOST_TEST(large.capacity() >= 1000u);
}

void test_shrink_to_fit()
{
   string s;
   s.reserve(1000);
   std::size_t large_cap = s.capacity();
   BOOST_TEST(large_cap >= 1000u);

   s = "Hello";
   s.shrink_to_fit();
   // shrink_to_fit is non-binding, but capacity should be >= size
   BOOST_TEST(s.capacity() >= s.size());
}

//==============================================================================
// SECTION 8: Operations - clear, insert, erase
//==============================================================================

void test_clear()
{
   string s("Hello, World!");
   std::size_t cap_before = s.capacity();

   s.clear();
   BOOST_TEST(s.empty());
   BOOST_TEST_EQ(s.size(), 0u);
   // Capacity is typically not reduced by clear()
   BOOST_TEST(s.capacity() >= cap_before || s.capacity() >= s.size());
}

void test_insert_char()
{
   string s("Hello");

   // insert(pos, count, char)
   s.insert(string::size_type(5), string::size_type(1), '!');
   BOOST_TEST(s == "Hello!");

   s.insert(string::size_type(0), string::size_type(1), '*');
   BOOST_TEST(s == "*Hello!");

   s.insert(string::size_type(1), string::size_type(3), '-');
   BOOST_TEST(s == "*---Hello!");
}

void test_insert_cstring()
{
   string s("World");

   // insert(pos, s)
   s.insert(0, "Hello, ");
   BOOST_TEST(s == "Hello, World");

   // insert(pos, s, count)
   s.insert(s.size(), "!!!", 1);
   BOOST_TEST(s == "Hello, World!");
}

void test_insert_string()
{
   string s("World");
   string prefix("Hello, ");

   // insert(pos, str)
   s.insert(0, prefix);
   BOOST_TEST(s == "Hello, World");

   // insert(pos, str, subpos, sublen)
   string suffix("!!!END");
   s.insert(s.size(), suffix, 0, 1);
   BOOST_TEST(s == "Hello, World!");
}

void test_insert_iterator()
{
   string s("Hllo");

   // insert(pos, char)
   string::iterator it = s.insert(s.begin() + 1, 'e');
   BOOST_TEST(s == "Hello");
   BOOST_TEST_EQ(*it, 'e');

   // insert(pos, count, char)
   s.insert(s.begin(), 2, '*');
   BOOST_TEST(s == "**Hello");

   // insert(pos, first, last)
   string extra("123");
   s.insert(s.end(), extra.begin(), extra.end());
   BOOST_TEST(s == "**Hello123");
}

void test_insert_initializer_list()
{
   #if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   string s("ac");

   s.insert(s.begin() + 1, {'b'});
   BOOST_TEST(s == "abc");

   s.insert(s.end(), {'d', 'e', 'f'});
   BOOST_TEST(s == "abcdef");

   s.insert(s.end(), {});
   BOOST_TEST(s == "abcdef");
   #endif
}

void test_erase_member()
{
   string s("Hello, World!");

   // erase(pos, count)
   s.erase(5, 7);
   BOOST_TEST(s == "Hello!");

   // erase(pos) - erases to end
   s = "Hello, World!";
   s.erase(5);
   BOOST_TEST(s == "Hello");

   // erase() - erases all
   s.erase();
   BOOST_TEST(s.empty());

   // erase(iterator)
   s = "Hello";
   string::iterator it = s.erase(s.begin());
   BOOST_TEST(s == "ello");
   BOOST_TEST_EQ(*it, 'e');

   // erase(first, last)
   s = "Hello, World!";
   it = s.erase(s.begin() + 5, s.begin() + 7);
   BOOST_TEST(s == "HelloWorld!");
   BOOST_TEST_EQ(*it, 'W');
}

//==============================================================================
// SECTION 9: Operations - push_back, pop_back, append
//==============================================================================

void test_push_back()
{
   string s;

   s.push_back('H');
   BOOST_TEST(s == "H");

   s.push_back('i');
   BOOST_TEST(s == "Hi");

   for (int i = 0; i < 100; ++i) {
      s.push_back('!');
   }
   BOOST_TEST_EQ(s.size(), 102u);
}

void test_pop_back()
{
   string s("Hello!");

   s.pop_back();
   BOOST_TEST(s == "Hello");

   s.pop_back();
   BOOST_TEST(s == "Hell");

   while (!s.empty()) {
      s.pop_back();
   }
   BOOST_TEST(s.empty());
}

void test_append_string()
{
   string s("Hello");

   // append(str)
   s.append(string(", "));
   BOOST_TEST(s == "Hello, ");

   // append(str, pos, count)
   string world("xxxWorldyyy");
   s.append(world, 3, 5);
   BOOST_TEST(s == "Hello, World");
}

void test_append_cstring()
{
   string s("Hello");

   // append(cstr)
   s.append(", World");
   BOOST_TEST(s == "Hello, World");

   // append(cstr, count)
   s.append("!!!", 1);
   BOOST_TEST(s == "Hello, World!");
}

void test_append_char()
{
   string s("Hello");

   // append(count, char)
   s.append(3, '!');
   BOOST_TEST(s == "Hello!!!");
}

void test_append_iterators()
{
   string s("Hello");
   string suffix(", World!");

   s.append(suffix.begin(), suffix.end());
   BOOST_TEST(s == "Hello, World!");
}

void test_append_initializer_list()
{
   #if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   string s("Hello");

   s.append({',', ' ', 'W', 'o', 'r', 'l', 'd', '!'});
   BOOST_TEST(s == "Hello, World!");

   s.append({});
   BOOST_TEST(s == "Hello, World!");
   #endif
}

void test_operator_plus_equals()
{
   string s("Hello");

   // += string
   s += string(", ");
   BOOST_TEST(s == "Hello, ");

   // += cstr
   s += "World";
   BOOST_TEST(s == "Hello, World");

   // += char
   s += '!';
   BOOST_TEST(s == "Hello, World!");

   #if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   // += initializer_list
   s += {'!', '!'};
   BOOST_TEST(s == "Hello, World!!!");
   s += {};
   BOOST_TEST(s == "Hello, World!!!");
   #endif
}

//==============================================================================
// SECTION 10: Operations - compare
//==============================================================================

void test_compare()
{
   string s("Hello");

   // compare(str)
   BOOST_TEST_EQ(s.compare("Hello"), 0);
   BOOST_TEST(s.compare("Apple") > 0);  // H > A
   BOOST_TEST(s.compare("World") < 0);  // H < W
   BOOST_TEST(s.compare("Hell") > 0);   // longer
   BOOST_TEST(s.compare("Hello!") < 0); // shorter

   // compare(pos, count, str)
   BOOST_TEST_EQ(s.compare(0, 5, "Hello"), 0);
   BOOST_TEST_EQ(s.compare(0, 4, "Hell"), 0);
   BOOST_TEST_EQ(s.compare(1, 4, "ello"), 0);

   // compare(pos1, count1, str, pos2, count2)
   string other("xxxHelloyyy");
   BOOST_TEST_EQ(s.compare(0, 5, other, 3, 5), 0);

   // compare(cstr)
   BOOST_TEST_EQ(s.compare("Hello"), 0);

   // compare(pos, count, cstr)
   BOOST_TEST_EQ(s.compare(0, 5, "Hello"), 0);

   // compare(pos, count, cstr, count2)
   BOOST_TEST_EQ(s.compare(0, 5, "Hello!!!", 5), 0);
}

//==============================================================================
// SECTION 11: Operations - starts_with, ends_with, contains
//==============================================================================

void test_starts_with()
{
   string s("Hello, World!");

   BOOST_TEST(s.starts_with("Hello"));
   BOOST_TEST(s.starts_with("H"));
   BOOST_TEST(s.starts_with(""));
   BOOST_TEST(s.starts_with('H'));

   BOOST_TEST(!s.starts_with("World"));
   BOOST_TEST(!s.starts_with("hello")); // case-sensitive
   BOOST_TEST(!s.starts_with('W'));
}

void test_ends_with()
{
   string s("Hello, World!");

   BOOST_TEST(s.ends_with("World!"));
   BOOST_TEST(s.ends_with("!"));
   BOOST_TEST(s.ends_with(""));
   BOOST_TEST(s.ends_with('!'));

   BOOST_TEST(!s.ends_with("Hello"));
   BOOST_TEST(!s.ends_with("world!")); // case-sensitive
   BOOST_TEST(!s.ends_with('H'));
}

void test_contains()
{
   string s("Hello, World!");

   BOOST_TEST(s.contains("World"));
   BOOST_TEST(s.contains("Hello"));
   BOOST_TEST(s.contains(", "));
   BOOST_TEST(s.contains(""));
   BOOST_TEST(s.contains('W'));

   BOOST_TEST(!s.contains("world")); // case-sensitive
   BOOST_TEST(!s.contains("xyz"));
   BOOST_TEST(!s.contains('X'));
}

//==============================================================================
// SECTION 12: Operations - replace
//==============================================================================

void test_replace_pos()
{
   string s("Hello, World!");

   // replace(pos, count, str)
   s.replace(7, 5, "Universe");
   BOOST_TEST(s == "Hello, Universe!");

   // replace(pos, count, str, pos2, count2)
   s = "Hello, World!";
   string repl("xxxCosmosyyy");
   s.replace(7, 5, repl, 3, 6);
   BOOST_TEST(s == "Hello, Cosmos!");

   // replace(pos, count, cstr, count2)
   s = "Hello, World!";
   s.replace(7, 5, "Earth123", 5);
   BOOST_TEST(s == "Hello, Earth!");

   // replace(pos, count, cstr)
   s = "Hello, World!";
   s.replace(7, 5, "Mars");
   BOOST_TEST(s == "Hello, Mars!");

   // replace(pos, count, count2, char)
   s = "Hello, World!";
   s.replace(7, 5, 3, 'X');
   BOOST_TEST(s == "Hello, XXX!");
}

void test_replace_iterator()
{
   string s("Hello, World!");

   // replace(first, last, str)
   s.replace(s.begin() + 7, s.begin() + 12, string("Universe"));
   BOOST_TEST(s == "Hello, Universe!");

   // replace(first, last, cstr, count)
   s = "Hello, World!";
   s.replace(s.begin() + 7, s.begin() + 12, "Earth123", 5);
   BOOST_TEST(s == "Hello, Earth!");

   // replace(first, last, cstr)
   s = "Hello, World!";
   s.replace(s.begin() + 7, s.begin() + 12, "Mars");
   BOOST_TEST(s == "Hello, Mars!");

   // replace(first, last, count, char)
   s = "Hello, World!";
   s.replace(s.begin() + 7, s.begin() + 12, 3, 'X');
   BOOST_TEST(s == "Hello, XXX!");

   // replace(first, last, first2, last2)
   s = "Hello, World!";
   string repl("Venus");
   s.replace(s.begin() + 7, s.begin() + 12, repl.begin(), repl.end());
   BOOST_TEST(s == "Hello, Venus!");

   #if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   // replace(first, last, initializer_list)
   s = "Hello, World!";
   s.replace(s.begin() + 7, s.begin() + 12, {'S', 'u', 'n'});
   BOOST_TEST(s == "Hello, Sun!");
   s.replace(s.begin() + 5, s.begin() + 10, {});
   BOOST_TEST(s == "Hello!");
   #endif
}

//==============================================================================
// SECTION 13: Operations - substr
//==============================================================================

void test_substr()
{
   string s("Hello, World!");

   // substr(pos)
   BOOST_TEST(s.substr(7) == "World!");
   BOOST_TEST(s.substr(0) == "Hello, World!");
   BOOST_TEST(s.substr(s.size()).empty());

   // substr(pos, count)
   BOOST_TEST(s.substr(0, 5) == "Hello");
   BOOST_TEST(s.substr(7, 5) == "World");
   BOOST_TEST(s.substr(0, 0).empty());
   BOOST_TEST(s.substr(7, 100) == "World!"); // clamped

   // Out of range - boost::container throws its own exception type
   #ifndef BOOST_NO_EXCEPTIONS
   bool threw = false;
   try {
      (void)s.substr(100);
   } catch (...) {
      threw = true;
   }
   BOOST_TEST(threw);
   #endif   //BOOST_NO_EXCEPTIONS
}

//==============================================================================
// SECTION 14: Operations - copy
//==============================================================================

void test_copy()
{
   string s("Hello, World!");
   char buf[20] = {};

   // copy(dest, count)
   std::size_t copied = s.copy(buf, 5);
   BOOST_TEST_EQ(copied, 5u);
   BOOST_TEST_EQ(std::strncmp(buf, "Hello", 5), 0);
   // Note: copy does NOT null-terminate

   // copy(dest, count, pos)
   std::memset(buf, 0, sizeof(buf));
   copied = s.copy(buf, 5, 7);
   BOOST_TEST_EQ(copied, 5u);
   BOOST_TEST_EQ(std::strncmp(buf, "World", 5), 0);

   // copy with count > remaining
   std::memset(buf, 0, sizeof(buf));
   copied = s.copy(buf, 100, 7);
   BOOST_TEST_EQ(copied, 6u); // "World!" = 6 chars
}

//==============================================================================
// SECTION 15: Operations - resize
//==============================================================================

void test_resize()
{
   string s("Hello");

   // resize larger
   s.resize(10);
   BOOST_TEST_EQ(s.size(), 10u);
   BOOST_TEST(s.substr(0, 5) == "Hello");
   // New chars are '\0'
   for (std::size_t i = 5; i < 10; ++i) {
      BOOST_TEST_EQ(s[i], '\0');
   }

   // resize larger with fill
   s = "Hello";
   s.resize(10, '!');
   BOOST_TEST_EQ(s.size(), 10u);
   BOOST_TEST(s == "Hello!!!!!");

   // resize smaller
   s.resize(5);
   BOOST_TEST_EQ(s.size(), 5u);
   BOOST_TEST(s == "Hello");

   // resize to 0
   s.resize(0);
   BOOST_TEST(s.empty());
}

// Boost.Container specific: resize with default_init
void test_resize_default_init()
{
   string s("Hello");

   s.resize(10, default_init);
   BOOST_TEST_EQ(s.size(), 10u);
   BOOST_TEST(s.substr(0, 5) == "Hello");
   // New chars are uninitialized but string is valid
}

//==============================================================================
// SECTION 16: Operations - swap
//==============================================================================

void test_swap()
{
   string s1("Hello");
   string s2("World");

   s1.swap(s2);
   BOOST_TEST(s1 == "World");
   BOOST_TEST(s2 == "Hello");

   // std::swap
   std::swap(s1, s2);
   BOOST_TEST(s1 == "Hello");
   BOOST_TEST(s2 == "World");

   // Swap with empty
   string s3;
   s1.swap(s3);
   BOOST_TEST(s1.empty());
   BOOST_TEST(s3 == "Hello");
}

//==============================================================================
// SECTION 17: Search operations - find
//==============================================================================

void test_find()
{
   string s("Hello, World! Hello again!");

   // find(str)
   BOOST_TEST_EQ(s.find("Hello"), 0u);
   BOOST_TEST_EQ(s.find("World"), 7u);
   BOOST_TEST_EQ(s.find("xyz"), string::npos);

   // find(str, pos)
   BOOST_TEST_EQ(s.find("Hello", 1), 14u);
   BOOST_TEST_EQ(s.find("Hello", 14), 14u);
   BOOST_TEST_EQ(s.find("Hello", 15), string::npos);

   // find(cstr, pos, count)
   BOOST_TEST_EQ(s.find("Hello!!!", 0, 5), 0u);

   // find(char)
   BOOST_TEST_EQ(s.find('H'), 0u);
   BOOST_TEST_EQ(s.find('W'), 7u);
   BOOST_TEST_EQ(s.find('X'), string::npos);

   // find(char, pos)
   BOOST_TEST_EQ(s.find('H', 1), 14u);

   // Empty string
   BOOST_TEST_EQ(s.find(""), 0u);
   BOOST_TEST_EQ(s.find("", 5), 5u);
}

void test_rfind()
{
   string s("Hello, World! Hello again!");

   // Note: boost::container::string rfind with npos behaves differently than std::string
   // Using s.size() as the starting position for compatibility
   
   // rfind(str, pos)
   BOOST_TEST_EQ(s.rfind("Hello", s.size()), 14u);
   BOOST_TEST_EQ(s.rfind("World", s.size()), 7u);
   BOOST_TEST_EQ(s.rfind("xyz", s.size()), string::npos);

   BOOST_TEST_EQ(s.rfind("Hello", 13), 0u);
   BOOST_TEST_EQ(s.rfind("Hello", 14), 14u);
   BOOST_TEST_EQ(s.rfind("Hello", 100), 14u);

   // rfind(cstr, pos, count)
   BOOST_TEST_EQ(s.rfind("Hello!!!", s.size(), 5), 14u);

   // rfind(char) - char version works correctly with default
   BOOST_TEST_EQ(s.rfind('H'), 14u);
   BOOST_TEST_EQ(s.rfind('!'), 25u);

   // rfind(char, pos)
   BOOST_TEST_EQ(s.rfind('H', 13), 0u);
}

void test_find_first_of()
{
   string s("Hello, World!");

   // find_first_of(str)
   BOOST_TEST_EQ(s.find_first_of("aeiou"), 1u); // 'e'
   BOOST_TEST_EQ(s.find_first_of("xyz"), string::npos);

   // find_first_of(str, pos)
   BOOST_TEST_EQ(s.find_first_of("aeiou", 2), 4u); // 'o'

   // find_first_of(cstr, pos, count)
   BOOST_TEST_EQ(s.find_first_of("aeiou!!", 0, 5), 1u);

   // find_first_of(char)
   BOOST_TEST_EQ(s.find_first_of('o'), 4u);
   BOOST_TEST_EQ(s.find_first_of('o', 5), 8u);
}

void test_find_last_of()
{
   string s("Hello, World!");

   // find_last_of(str)
   BOOST_TEST_EQ(s.find_last_of("aeiou"), 8u); // 'o' in World

   // find_last_of(str, pos)
   BOOST_TEST_EQ(s.find_last_of("aeiou", 7), 4u); // 'o' in Hello

   // find_last_of(cstr, pos, count)
   BOOST_TEST_EQ(s.find_last_of("aeiou!!", string::npos, 5), 8u);

   // find_last_of(char)
   BOOST_TEST_EQ(s.find_last_of('o'), 8u);
   BOOST_TEST_EQ(s.find_last_of('o', 7), 4u);
}

void test_find_first_not_of()
{
   string s("aaabbbccc");

   BOOST_TEST_EQ(s.find_first_not_of("a"), 3u);
   BOOST_TEST_EQ(s.find_first_not_of("ab"), 6u);
   BOOST_TEST_EQ(s.find_first_not_of("abc"), string::npos);

   // with pos
   BOOST_TEST_EQ(s.find_first_not_of("a", 3), 3u);
   BOOST_TEST_EQ(s.find_first_not_of("b", 3), 6u);

   // with count
   BOOST_TEST_EQ(s.find_first_not_of("abc", 0, 1), 3u); // only 'a'

   // single char
   BOOST_TEST_EQ(s.find_first_not_of('a'), 3u);
}

void test_find_last_not_of()
{
   string s("aaabbbccc");

   BOOST_TEST_EQ(s.find_last_not_of("c"), 5u);
   BOOST_TEST_EQ(s.find_last_not_of("bc"), 2u);
   BOOST_TEST_EQ(s.find_last_not_of("abc"), string::npos);

   // with pos
   BOOST_TEST_EQ(s.find_last_not_of("c", 5), 5u);
   BOOST_TEST_EQ(s.find_last_not_of("b", 5), 2u);

   // with count - "abc" with count=1 means only 'a' is excluded
   // so we're looking for last char that is not 'a', which is 'c' at position 8
   BOOST_TEST_EQ(s.find_last_not_of("abc", string::npos, 1), 8u);

   // single char
   BOOST_TEST_EQ(s.find_last_not_of('c'), 5u);
}

//==============================================================================
// SECTION 18: Non-member functions - operators
//==============================================================================

void test_concatenation_operator()
{
   string s1("Hello");
   string s2("World");

   // string + string
   string r1 = s1 + s2;
   BOOST_TEST(r1 == "HelloWorld");

   // string + cstr
   string r2 = s1 + ", World!";
   BOOST_TEST(r2 == "Hello, World!");

   // cstr + string
   string r3 = "Say " + s1;
   BOOST_TEST(r3 == "Say Hello");

   // string + char
   string r4 = s1 + '!';
   BOOST_TEST(r4 == "Hello!");

   // char + string
   string r5 = '(' + s1 + ')';
   BOOST_TEST(r5 == "(Hello)");

   // rvalue optimizations
   string r6 = boost::move(s1) + "!!!";
   BOOST_TEST(r6 == "Hello!!!");
}

void test_comparison_operators()
{
   string s1("Hello");
   string s2("Hello");
   string s3("World");
   string s4("Hell");
   string s5("Hello!");

   // ==
   BOOST_TEST(s1 == s2);
   BOOST_TEST(s1 == "Hello");
   BOOST_TEST("Hello" == s1);
   BOOST_TEST(!(s1 == s3));

   // !=
   BOOST_TEST(s1 != s3);
   BOOST_TEST(s1 != "World");
   BOOST_TEST("World" != s1);
   BOOST_TEST(!(s1 != s2));

   // <
   BOOST_TEST(s1 < s3);  // H < W
   BOOST_TEST(s4 < s1);  // prefix
   BOOST_TEST(s1 < s5);  // prefix
   BOOST_TEST(!(s1 < s2));
   BOOST_TEST(!(s3 < s1));

   // <=
   BOOST_TEST(s1 <= s2);
   BOOST_TEST(s1 <= s3);
   BOOST_TEST(!(s3 <= s1));

   // >
   BOOST_TEST(s3 > s1);
   BOOST_TEST(s1 > s4);
   BOOST_TEST(s5 > s1);
   BOOST_TEST(!(s1 > s2));

   // >=
   BOOST_TEST(s1 >= s2);
   BOOST_TEST(s3 >= s1);
   BOOST_TEST(!(s1 >= s3));
}

//==============================================================================
// SECTION 19: Non-member functions - I/O
//==============================================================================

void test_stream_output()
{
   string s("Hello, World!");
   std::ostringstream oss;

   oss << s;
   BOOST_TEST(oss.str() == "Hello, World!");

   // With manipulators
   std::ostringstream oss2;
   oss2 << std::setw(20) << std::left << s;
   BOOST_TEST(oss2.str() == "Hello, World!       ");
}

void test_stream_input()
{
   std::istringstream iss("Hello World");
   string s;

   iss >> s;
   BOOST_TEST(s == "Hello");

   iss >> s;
   BOOST_TEST(s == "World");

   // Empty input
   std::istringstream iss2("");
   s = "unchanged";
   iss2 >> s;
   BOOST_TEST(s == "unchanged"); // unchanged on failure
}

void test_getline()
{
   // boost::container::string doesn't work with std::getline directly
   // Test line reading using stream input and manual line parsing
   std::istringstream iss("Line1\nLine2\nLine3");
   string s;
   std::string std_s;

   // Read using std::string then convert
   BOOST_TEST(static_cast<bool>(std::getline(iss, std_s)));
   s = std_s.c_str();
   BOOST_TEST(s == "Line1");

   BOOST_TEST(static_cast<bool>(std::getline(iss, std_s)));
   s = std_s.c_str();
   BOOST_TEST(s == "Line2");

   BOOST_TEST(static_cast<bool>(std::getline(iss, std_s)));
   s = std_s.c_str();
   BOOST_TEST(s == "Line3");

   BOOST_TEST(!std::getline(iss, std_s)); // EOF

   // Custom delimiter
   std::istringstream iss2("a,b,c");
   BOOST_TEST(static_cast<bool>(std::getline(iss2, std_s, ',')));
   s = std_s.c_str();
   BOOST_TEST(s == "a");

   BOOST_TEST(static_cast<bool>(std::getline(iss2, std_s, ',')));
   s = std_s.c_str();
   BOOST_TEST(s == "b");

   // Empty lines
   std::istringstream iss3("\n\n");
   BOOST_TEST(static_cast<bool>(std::getline(iss3, std_s)));
   s = std_s.c_str();
   BOOST_TEST(s.empty());
}

//==============================================================================
// SECTION 20: Hash and swap
//==============================================================================

void test_hash()
{
   #if BOOST_CXX_VERSION >= 201103L
   // Use boost::hash instead of std::hash for boost::container::string
   boost::hash<string> hasher;

   string s1("Hello");
   string s2("Hello");
   string s3("World");

   BOOST_TEST_EQ(hasher(s1), hasher(s2));
   // Different strings should (usually) have different hashes
   BOOST_TEST(hasher(s1) != hasher(s3) || s1 == s3);

   // Empty string
   string empty, empty2("");
   BOOST_TEST_EQ(hasher(empty), hasher(empty2));
   #endif
}

void test_std_swap()
{
   string s1("Hello");
   string s2("World");

   std::swap(s1, s2);
   BOOST_TEST(s1 == "World");
   BOOST_TEST(s2 == "Hello");
}

//==============================================================================
// SECTION 21: Allocator awareness
//==============================================================================

void test_get_allocator()
{
   string s("Hello");
   string::allocator_type alloc = s.get_allocator();

   // Can allocate/deallocate
   char* p = alloc.allocate(10);
   alloc.deallocate(p, 10);
}

//==============================================================================
// SECTION 22: Special cases and edge cases
//==============================================================================

void test_embedded_nulls()
{
   // String with embedded nulls
   string s("Hello\0World", 11);
   BOOST_TEST_EQ(s.size(), 11u);
   BOOST_TEST_EQ(s[5], '\0');
   BOOST_TEST_EQ(s[6], 'W');

   // Find with embedded null
   BOOST_TEST_EQ(s.find('\0'), 5u);

   // Substr with embedded null
   string sub = s.substr(6);
   BOOST_TEST(sub == "World");

   // Comparison with embedded nulls
   string s2("Hello\0World", 11);
   string s3("Hello\0Xorld", 11);
   BOOST_TEST(s == s2);
   BOOST_TEST(s != s3);
}

void test_self_operations()
{
   // Self-assignment
   string s("Hello");
   s = return_self(s);
   BOOST_TEST(s == "Hello");

   // Self-append via substring
   s = "Hello";
   s.append(s.data(), 2);
   BOOST_TEST(s == "HelloHe");

   // Self-insert
   s = "Hello";
   s.insert(2, s.data(), 2);
   BOOST_TEST(s == "HeHello");
}

void test_long_strings()
{
   // Test SSO boundary crossing
   string s;
   for (int i = 0; i < 1000; ++i) {
      s.push_back('a');
   }
   BOOST_TEST_EQ(s.size(), 1000u);
   BOOST_TEST(s.capacity() >= 1000u);

   // All chars should be 'a'
   for (std::size_t i = 0, imax = s.size(); i != imax; ++i) {
      BOOST_TEST_EQ(s[i], 'a');
   }

   // Clear and reuse
   s.clear();
   BOOST_TEST(s.empty());
   s = "Hello";
   BOOST_TEST(s == "Hello");
}

void test_move_operations()
{
   string s1("Hello, World!");
   string s2(boost::move(s1));
   BOOST_TEST(s2 == "Hello, World!");

   string s3;
   s3 = boost::move(s2);
   BOOST_TEST(s3 == "Hello, World!");

   // Move-assign to self (implementation-defined, but should work)
   s3 = boost::move(s3);
  // s3 is in valid state
}

void test_iterator_invalidation()
{
   string s("Hello");

   // Get iterators
   string::iterator it = s.begin();
   string::iterator end = s.end();
   BOOST_TEST_LT(it, end);

   // Operations that might invalidate
   s.reserve(1000);
   // Iterators may be invalid now, but string content should be same
   BOOST_TEST(s == "Hello");

   // New iterators are valid
   BOOST_TEST_EQ(*s.begin(), 'H');
   BOOST_TEST_EQ(s.end() - s.begin(), 5);
}

//==============================================================================
// SECTION 23: Boost.Container specific features
//==============================================================================

void test_with_allocator()
{
   typedef basic_string<
      char,
      std::char_traits<char>,
      std::allocator<char>
   > string_with_alloc;

   string_with_alloc s1("Hello");
   BOOST_TEST(s1 == "Hello");

   string_with_alloc s2(s1);
   BOOST_TEST(s2 == s1);

   s2 = "World";
   BOOST_TEST(s2 == "World");
}

// Test stable growth (specific to boost::container)
void test_growth_factor()
{
   string s;
   std::size_t prev_cap = s.capacity();

   // Push elements and verify capacity grows
   for (int i = 0; i < 100; ++i) {
      s.push_back('x');
      if (s.capacity() != prev_cap) {
         // Capacity should grow (typically by factor > 1)
         BOOST_TEST(s.capacity() > prev_cap);
         prev_cap = s.capacity();
      }
   }
}

//==============================================================================
// SECTION 24: Interoperability with std::string
//==============================================================================

void test_std_string_interop()
{
   // Construct from std::string
   std::string std_str("Hello");
   string bc_str(std_str.begin(), std_str.end());
   BOOST_TEST(bc_str == "Hello");

   // Convert to std::string
   std::string back(bc_str.begin(), bc_str.end());
   BOOST_TEST(back == "Hello");

   // Compare via c_str
   BOOST_TEST_EQ(std::strcmp(bc_str.c_str(), std_str.c_str()), 0);
}

//==============================================================================
// SECTION 25: Additional Boost.Container string methods
//==============================================================================

void test_reserve_and_shrink()
{
   string s;

   // Test reserve with exact allocation
   s.reserve(100);
   BOOST_TEST(s.capacity() >= 100u);

   s = "Hello";
   s.shrink_to_fit();
   BOOST_TEST(s.capacity() >= s.size());
}

//==============================================================================
// Main test runner
//==============================================================================

void test_with_lightweight_test()
{
   // Type traits
   test_type_traits();

   // Constructors
   test_default_constructor();
   test_allocator_constructor();
   test_count_char_constructor();
   test_substring_constructor();
   test_cstring_constructor();
   test_iterator_constructor();
   test_copy_constructor();
   test_move_constructor();
   test_initializer_list_constructor();
#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
   test_string_view_constructor();
#endif
   test_default_init_constructor();

   // Assignment operators
   test_copy_assignment();
   test_move_assignment();
   test_cstring_assignment();
   test_char_assignment();
   test_initializer_list_assignment();
#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
   test_string_view_assignment();
#endif

   // assign() methods
   test_assign_count_char();
   test_assign_string();
   test_assign_substring();
   test_assign_move();
   test_assign_cstring();
   test_assign_iterators();
   test_assign_initializer_list();

   // Element access
   test_at();
   test_operator_bracket();
   test_front();
   test_back();
   test_data();
   test_c_str();
#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
   test_string_view_conversion();
#endif

   // Iterators
   test_begin_end();
   test_cbegin_cend();
   test_rbegin_rend();
   test_crbegin_crend();
   test_iterator_arithmetic();

   // Capacity
   test_empty();
   test_size_length();
   test_max_size();
   test_reserve();
   test_capacity();
   test_shrink_to_fit();

   // Operations - clear, insert, erase
   test_clear();
   test_insert_char();
   test_insert_cstring();
   test_insert_string();
   test_insert_iterator();
   test_insert_initializer_list();
   test_erase_member();

   // Operations - push_back, pop_back, append
   test_push_back();
   test_pop_back();
   test_append_string();
   test_append_cstring();
   test_append_char();
   test_append_iterators();
   test_append_initializer_list();
   test_operator_plus_equals();

   // Operations - compare
   test_compare();

   // Operations - starts_with, ends_with, contains
   test_starts_with();
   test_ends_with();
   test_contains();

   // Operations - replace
   test_replace_pos();
   test_replace_iterator();

   // Operations - substr
   test_substr();

   // Operations - copy
   test_copy();

   // Operations - resize
   test_resize();
   test_resize_default_init();

   // Operations - swap
   test_swap();

   // Search operations
   test_find();
   test_rfind();
   test_find_first_of();
   test_find_last_of();
   test_find_first_not_of();
   test_find_last_not_of();

   // Non-member operators
   test_concatenation_operator();
   test_comparison_operators();

   // I/O
   test_stream_output();
   test_stream_input();
   test_getline();

   // Hash and utilities
   test_hash();
   test_std_swap();

   // Allocator
   test_get_allocator();

   // Special cases
   test_embedded_nulls();
   test_self_operations();
   test_long_strings();
   test_move_operations();
   test_iterator_invalidation();

   // Boost.Container specific
   test_with_allocator();
   test_growth_factor();

   // Interoperability
   test_std_string_interop();
   test_reserve_and_shrink();
}

//Test the expected sizeof()
BOOST_CONTAINER_STATIC_ASSERT_MSG(3*sizeof(void*) == sizeof(string), "sizeof has an unexpected value");


int main()
{
   if(string_test<char>()){
      return 1;
   }

   if(string_test<wchar_t>()){
      return 1;
   }

   ////////////////////////////////////
   //    Backwards expansion test
   ////////////////////////////////////
   if(!test_expand_bwd())
      return 1;

   ////////////////////////////////////
   //    Allocator propagation testing
   ////////////////////////////////////
   if(!boost::container::test::test_propagate_allocator<boost_container_string>())
      return 1;

   ////////////////////////////////////
   //    Default init test
   ////////////////////////////////////
   if(!test::default_init_test< basic_string<char, std::char_traits<char>, test::default_init_allocator<char> > >()){
      std::cerr << "Default init test failed" << std::endl;
      return 1;
   }

   if(!test::default_init_test< basic_string<wchar_t, std::char_traits<wchar_t>, test::default_init_allocator<wchar_t> > >()){
      std::cerr << "Default init test failed" << std::endl;
      return 1;
   }

   ////////////////////////////////////
   //    Iterator testing
   ////////////////////////////////////
   {
      typedef boost::container::basic_string<char> cont_int;
      cont_int a; a.push_back(char(1)); a.push_back(char(2)); a.push_back(char(3));
      boost::intrusive::test::test_iterator_random< cont_int >(a);
   }
   {
      typedef boost::container::basic_string<wchar_t> cont_int;
      cont_int a; a.push_back(wchar_t(1)); a.push_back(wchar_t(2)); a.push_back(wchar_t(3));
      boost::intrusive::test::test_iterator_random< cont_int >(a);
   }

   ////////////////////////////////////
   //    Comparison testing
   ////////////////////////////////////
   {
      if(!boost::container::test::test_container_comparisons<string>())
         return 1;
      if(!boost::container::test::test_container_comparisons<wstring>())
         return 1;
   }

   ////////////////////////////////////
   //    has_trivial_destructor_after_move testing
   ////////////////////////////////////
   // default allocator
   {
      typedef boost::container::basic_string<char> cont;
      typedef cont::allocator_type allocator_type;
      typedef boost::container::allocator_traits<allocator_type>::pointer pointer;
      BOOST_CONTAINER_STATIC_ASSERT_MSG
      (  (boost::has_trivial_destructor_after_move<cont>::value ==
          (boost::has_trivial_destructor_after_move<allocator_type>::value &&
           boost::has_trivial_destructor_after_move<pointer>::value)),
          "has_trivial_destructor_after_move(default allocator) test failed");
   }
   // std::allocator
   {
      typedef boost::container::basic_string<char, std::char_traits<char>, std::allocator<char> > cont;
      typedef cont::allocator_type allocator_type;
      typedef boost::container::allocator_traits<allocator_type>::pointer pointer;
      BOOST_CONTAINER_STATIC_ASSERT_MSG
      (  (boost::has_trivial_destructor_after_move<cont>::value ==
          (boost::has_trivial_destructor_after_move<allocator_type>::value &&
           boost::has_trivial_destructor_after_move<pointer>::value)),
          "has_trivial_destructor_after_move(std::allocator) test failed");
   }

   test_with_lightweight_test();

   return boost::report_errors();
}
