// Copyright 2025 Christian Granzin
// Copyright 2008 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MSM_BACKMP11_HISTORY_IMPL_H
#define BOOST_MSM_BACKMP11_HISTORY_IMPL_H

#include <boost/msm/front/history_policies.hpp>
#include <boost/mp11.hpp>

namespace boost::msm::backmp11
{
namespace detail
{

// Implementations for history policies.

template<typename History, int NumberOfRegions>
class history_impl;

template <int NumberOfRegions>
class history_impl<front::no_history, NumberOfRegions>
{
  public:
    void reset_active_state_ids(const std::array<int, NumberOfRegions>& initial_state_ids)
    {
        m_initial_state_ids = initial_state_ids;
    }

    template <class Event>
    const std::array<int, NumberOfRegions>& on_entry(Event const&)
    {
        return m_initial_state_ids;
    }

    void on_exit(const std::array<int, NumberOfRegions>&)
    {
        // ignore
    }

    // this policy deletes all waiting deferred events
    template <class Event>
    bool process_deferred_events(Event const&) const
    {
        return false;
    }

  private:
    // Allow access to private members for serialization.
    template<typename T, int N>
    friend void serialize(T&, history_impl<front::no_history, N>&);

    std::array<int, NumberOfRegions> m_initial_state_ids;
};

template <int NumberOfRegions>
class history_impl<front::always_shallow_history, NumberOfRegions>
{
public:
    void reset_active_state_ids(const std::array<int, NumberOfRegions>& initial_state_ids)
    {
        m_last_active_state_ids = initial_state_ids;
    }

    template <class Event>
    const std::array<int, NumberOfRegions>& on_entry(Event const& )
    {
        return m_last_active_state_ids;
    }

    void on_exit(const std::array<int, NumberOfRegions>& active_state_ids)
    {
        m_last_active_state_ids = active_state_ids;
    }

    // the history policy keeps all deferred events until next reentry
    template <class Event>
    bool process_deferred_events(Event const&)const
    {
        return true;
    }

private:
    // Allow access to private members for serialization.
    template<typename T, int N>
    friend void serialize(T&, history_impl<front::always_shallow_history, N>&);

    std::array<int, NumberOfRegions> m_last_active_state_ids;
};

template <typename... Events, int NumberOfRegions>
class history_impl<front::shallow_history<Events...>, NumberOfRegions>
{
    using events_mp11 = mp11::mp_list<Events...>;

public:
    void reset_active_state_ids(const std::array<int, NumberOfRegions>& initial_state_ids)
    {
        m_initial_state_ids = initial_state_ids;
        m_last_active_state_ids = initial_state_ids;
    }

    template <class Event>
    const std::array<int, NumberOfRegions>& on_entry(Event const&)
    {
        if constexpr (mp11::mp_contains<events_mp11,Event>::value)
        {
            return m_last_active_state_ids;
        }
        return m_initial_state_ids;
    }

    void on_exit(const std::array<int, NumberOfRegions>& active_state_ids)
    {
        m_last_active_state_ids = active_state_ids;
    }

    // the history policy keeps deferred events until next reentry if coming from our history event
    template <class Event>
    bool process_deferred_events(Event const&) const
    {
        return mp11::mp_contains<events_mp11,Event>::value;
    }

  private:
    // Allow access to private members for serialization.
    template<typename T, typename... Es, int N>
    friend void serialize(T&, history_impl<front::shallow_history<Es...>, N>&);

    std::array<int, NumberOfRegions> m_initial_state_ids;
    std::array<int, NumberOfRegions> m_last_active_state_ids;
};

} // detail

} // boost::msm::backmp11

#endif // BOOST_MSM_BACKMP11_HISTORY_IMPL_H
