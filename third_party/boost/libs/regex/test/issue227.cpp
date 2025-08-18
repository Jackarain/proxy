/*
* Copyright (c) 2024
* Christian Mazakas
*
* Use, modification and distribution are subject to the
* Boost Software License, Version 1.0. (See accompanying file
* LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*
*/

#include <boost/regex.hpp>
#include <string>

int main() {
   boost::regex rx("(*ACCEPT)*+\\1((*ACCEPT)*+\\K)");
   std::string str = "Z";
   boost::smatch what;
   boost::regex_search(str, what, rx, boost::match_default | boost::match_partial);
}
