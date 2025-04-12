// Copyright (c) 2016 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#include <boost/process/v1/args.hpp>
#include <boost/process/v1/async.hpp>
#include <boost/process/v1/async_pipe.hpp>
#include <boost/process/v1/async_system.hpp>
#include <boost/process/v1/child.hpp>
#include <boost/process/v1/cmd.hpp>
#include <boost/process/v1/env.hpp>
#include <boost/process/v1/environment.hpp>
#include <boost/process/v1/error.hpp>
#include <boost/process/v1/exception.hpp>
#include <boost/process/v1/exe.hpp>
#include <boost/process/v1/extend.hpp>
#include <boost/process/v1/filesystem.hpp>
#include <boost/process/v1/group.hpp>
#include <boost/process/v1/handles.hpp>
#include <boost/process/v1/io.hpp>
#include <boost/process/v1/locale.hpp>
#include <boost/process/v1/pipe.hpp>
#include <boost/process/v1/search_path.hpp>
#include <boost/process/v1/shell.hpp>
#include <boost/process/v1/spawn.hpp>
#include <boost/process/v1/start_dir.hpp>
#include <boost/process/v1/system.hpp>

#if defined(BOOST_POSIX_API)
#include <boost/process/v1/posix.hpp>
#else
#include <boost/process/v1/windows.hpp>
#endif
