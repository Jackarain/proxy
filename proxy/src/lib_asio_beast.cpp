//
// lib_asio_beast.cpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2022 Jack (jack.arain at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
//

#include <boost/asio/impl/src.hpp>

#if __has_include(<openssl/ssl.h>)
#	include <boost/asio/ssl/impl/src.hpp>
#endif

#include <boost/beast/src.hpp>
