//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//


#include <boost/url/detail/config.hpp>
#include <boost/url/error.hpp>

// In C++20+, the instance is inline constexpr in the header.
#if !defined(BOOST_URL_HAS_CXX20_CONSTEXPR)

namespace boost {
namespace urls {
namespace detail {

// msvc 14.0 has a bug that warns about inability
// to use constexpr construction here, even though
// there's no constexpr construction
#if defined(_MSC_VER) && _MSC_VER <= 1900
# pragma warning( push )
# pragma warning( disable : 4592 )
#endif

error_cat_type error_cat;

#if defined(_MSC_VER) && _MSC_VER <= 1900
# pragma warning( pop )
#endif

} // detail
} // urls
} // boost

#endif
