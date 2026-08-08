// Copyright (C) 2001-2003
// William E. Kempf
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_THREAD_PROVIDES_ONCE_CXX11

#include <boost/thread/thread.hpp>
#include <boost/thread/once.hpp>
#include <cassert>

int g_value=0;
#ifdef BOOST_THREAD_PROVIDES_ONCE_CXX11
static boost::once_flag g_once;
//static boost::once_flag g_once2 = BOOST_ONCE_INIT;
#else
static boost::once_flag g_once = BOOST_ONCE_INIT;
//static boost::once_flag g_once2 = g_once;
#endif

void init()
{
    ++g_value;
}

void thread_proc()
{
    boost::call_once(&init, g_once);
}

int main()
{
    boost::thread_group threads;
    for (int i=0; i<5; ++i)
        threads.create_thread(&thread_proc);
    threads.join_all();
    assert(g_value == 1);
}
