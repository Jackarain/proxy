// Copyright (C) 2001-2003
// William E. Kempf
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#undef BOOST_THREAD_VERSION
#define BOOST_THREAD_VERSION 2

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/thread_only.hpp>
#include <boost/thread/xtime.hpp>
#include <iostream>

#if defined(BOOST_HAS_WINTHREADS)
#   include <windows.h>
#   include <process.h>
#endif

enum game_state
{
    START,
    PLAYER_A,
    PLAYER_B,
    GAME_OVER,
    ONE_PLAYER_GONE,
    BOTH_PLAYERS_GONE
};

int g_state;
boost::mutex g_mutex;
boost::condition g_cond;

const char* player_name(int state)
{
    if (state == PLAYER_A)
        return "PLAYER-A";
    if (state == PLAYER_B)
        return "PLAYER-B";
    throw "bad player";
    //return 0;
}

void player(int active)
{
    boost::unique_lock<boost::mutex> lock(g_mutex);

    int other = active == PLAYER_A ? PLAYER_B : PLAYER_A;

    while (g_state < GAME_OVER)
    {
        //std::cout << player_name(active) << ": Play." << std::endl;
        g_state = other;
        g_cond.notify_all();
        do
        {
            g_cond.wait(lock);
            if (g_state == other)
            {
                std::cout << "---" << player_name(active)
                          << ": Spurious wakeup!" << std::endl;
            }
        } while (g_state == other);
    }

    ++g_state;
    std::cout << player_name(active) << ": Gone." << std::endl;
    g_cond.notify_all();
}

struct thread_adapt
{
    thread_adapt(void (*func)(void*), void* param)
        : _func(func), _param(param)
    {
    }
    int operator()() const
    {
        _func(_param);
        return 0;
    }

    void (*_func)(void*);
    void* _param;
};

class thread_adapter
{
public:
    thread_adapter(void (*func)(void*), void* param)
        : _func(func), _param(param)
    {
    }
    void operator()() const { _func(_param); }
private:
    void (*_func)(void*);
    void* _param;
};

int main()
{
    g_state = START;

    boost::thread thrda(&player, PLAYER_A);
    boost::thread thrdb(&player, PLAYER_B);

    boost::xtime xt;
    boost::xtime_get(&xt, boost::TIME_UTC_);
    xt.sec += 1;
    boost::thread::sleep(xt);
    {
        boost::unique_lock<boost::mutex> lock(g_mutex);
        std::cout << "---Noise ON..." << std::endl;
    }

    for (int i = 0; i < 10; ++i)
        g_cond.notify_all();

    {
        boost::unique_lock<boost::mutex> lock(g_mutex);
        std::cout << "---Noise OFF..." << std::endl;
        g_state = GAME_OVER;
        g_cond.notify_all();
        do
        {
            g_cond.wait(lock);
        } while (g_state != BOTH_PLAYERS_GONE);
    }

    std::cout << "GAME OVER" << std::endl;

    thrda.join();
    thrdb.join();

    return 0;
}
