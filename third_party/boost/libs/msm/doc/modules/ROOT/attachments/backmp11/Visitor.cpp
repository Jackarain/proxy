// Copyright 2026 Christian Granzin
// Copyright 2010 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <random>

#include <boost/msm/backmp11/state_machine.hpp>
#include <boost/msm/front/functor_row.hpp>
#include <boost/msm/front/state_machine_def.hpp>

namespace back = boost::msm::backmp11;
namespace front = boost::msm::front;
using front::none;
using front::Row;
namespace mp11 = boost::mp11;

namespace
{

// Events
struct Play
{
    std::string_view song;
};

struct Stop {};

// States
struct Idle : front::state<> {};

template <const char* Name>
struct Song : front::state<>
{
    template <typename Fsm>
    void on_entry(const Play&, Fsm&)
    {
        times_played += 1;
    }

    static constexpr const char* name = Name;
    size_t times_played{};
};

constexpr const char hey_jude[] = "Hey Jude";
constexpr const char all_you_need_is_love[] = "All You Need Is Love";
constexpr const char paint_it_black[] = "Paint It Black";

// Guards
template <const char* Name>
struct IsSong
{
    template <typename Fsm>
    bool operator()(const Play& play, Fsm&)
    {
        return std::string_view{Name} == std::string_view{play.song};
    }
};

// State machine
struct Playing_ : front::state_machine_def<Playing_>
{
    template <typename Fsm>
    void on_entry(const Play& play, Fsm& fsm)
    {
        fsm.enqueue_event(play);
    }

    using initial_state = Idle;
    using transition_table = mp11::mp_list<
        Row<Idle, Play, Song<hey_jude>            , none, IsSong<hey_jude>>,
        Row<Idle, Play, Song<all_you_need_is_love>, none, IsSong<all_you_need_is_love>>,
        Row<Idle, Play, Song<paint_it_black>      , none, IsSong<paint_it_black>>
    >;
};

using Playing = back::state_machine<Playing_>;

struct Jukebox_ : front::state_machine_def<Jukebox_>
{
    using initial_state = Idle;
    using transition_table = mp11::mp_list<
        Row<Idle   , Play, Playing>,
        Row<Playing, Stop, Idle>
    >;
};

using Jukebox = back::state_machine<Jukebox_>;

constexpr const char* songs[] = {
    hey_jude,
    all_you_need_is_love,
    paint_it_black,
};

// Type trait to identify a song state in a lambda.
template <typename T>
struct is_song : std::false_type {};
template <const char* Name>
struct is_song<Song<Name>> : std::true_type {};

// Functor with a call overload for a song state.
struct check_times_played
{
    template <const char* Name>
    void operator()(const Song<Name>& song)
    {
        std::cout << "Song " << song.name << " played " << song.times_played
                  << " times" << std::endl;
    }

    template <typename State>
    void operator()(const State&)
    {
    }
};

[[maybe_unused]] void visitor_example()
{
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<size_t> dist{0, 2};
    Jukebox jukebox;
    jukebox.start();

    // Play 100 + 1 songs
    for (size_t i = 0; i < 100; i++)
    {
        const size_t n = dist(rng);
        jukebox.process_event(Play{songs[n]});
        jukebox.process_event(Stop{});
    }
    jukebox.process_event(Play{songs[dist(rng)]});

    // Check the active song - using a lambda.
    jukebox.visit(
    [](const auto& maybe_song)
    {
        if constexpr (is_song<std::decay_t<decltype(maybe_song)>>::value)
        {
            std::cout << "Currently playing: " << maybe_song.name << std::endl;
        }
    });

    // Check how many times each song has been played - using a functor for more precise overloads.
    jukebox.visit<back::visit_mode::all_recursive>(check_times_played{});
}

} // namespace
