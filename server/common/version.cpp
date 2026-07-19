//
// version.cpp
// ~~~~~~~~~~~
//
// Copyright (c) 2023 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//


#ifdef __linux__
#  include <sys/resource.h>

# ifndef HAVE_UNAME
#  define HAVE_UNAME
# endif

#elif _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <fcntl.h>
#  include <io.h>
#  include <windows.h>

#endif // _WIN32

#ifdef HAVE_UNAME
#  include <sys/utsname.h>
#endif

#if __has_include("wolfssl/version.h")
#  include "wolfssl/version.h"
#endif

#if __has_include("openssl/opensslv.h")
#  include "openssl/opensslv.h"
#else
#  ifndef OPENSSL_VERSION_TEXT
#    define OPENSSL_VERSION_TEXT "NONE"
#  endif
#endif

#include <boost/version.hpp>
#include <boost/config.hpp>

#include <string>
#include <string_view>
#include <iterator>
#include <algorithm>
#include <optional>
#include <iostream>
#include <sstream>
#include <memory>
#include <ios>


std::string version_info()
{
	std::ostringstream oss;
	std::string os_name;

#ifdef _WIN32
# ifdef _WIN64
	os_name = "Windows (64bit)";
# else
	os_name = "Windows (32bit)";
# endif
#elif defined(HAVE_UNAME)
	struct utsname un;
	uname(&un);
	os_name = std::string(un.sysname) + " " + un.release;

	int ma_ver, mi_ver, patch_ver;
	sscanf(un.release, "%d.%d.%d", &ma_ver, &mi_ver, &patch_ver);

	if (os_name.find("Linux") != std::string::npos && ma_ver < 4)
		std::cerr << "WARNING: kernel too old, please upgrade your system!" << std::endl;
#else
	os_name = BOOST_PLATFORM;
#endif

	std::string git_version;

#ifdef VERSION_GIT
	git_version = VERSION_GIT;
#else
	git_version = __DATE__ + std::string(" ") + __TIME__;
#endif

	oss << "Built: " << git_version
		<< ", " << os_name
		<< ", " << BOOST_COMPILER
		<< ", Boost " << BOOST_LIB_VERSION
		<< ", SSL: " << OPENSSL_VERSION_TEXT
		;

	std::cerr << oss.str() << "\n";
	return oss.str();
}

