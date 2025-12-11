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

#ifndef BOOST_MSM_BACKMP11_METAFUNCTIONS_H
#define BOOST_MSM_BACKMP11_METAFUNCTIONS_H

#include <boost/mp11.hpp>
#include <boost/mp11/mpl_list.hpp>

#include <boost/mpl/copy.hpp>
#include <boost/mpl/is_sequence.hpp>
#include <boost/mpl/front.hpp>

#include <boost/type_traits/is_same.hpp>
#include <boost/utility/enable_if.hpp>

#include <boost/msm/row_tags.hpp>

#include <boost/msm/backmp11/common_types.hpp>
#include <boost/msm/back/traits.hpp>
#include <boost/msm/front/detail/common_states.hpp>


namespace boost { namespace msm { namespace backmp11
{

namespace detail
{

constexpr bool has_flag(visit_mode value, visit_mode flag)
{
    return (static_cast<int>(value) & static_cast<int>(flag)) != 0;
}

struct back_end_tag {};

template <typename T>
using has_back_end_tag = std::is_same<typename T::internal::tag, back_end_tag>;

template <class T>
using is_back_end = has_back_end_tag<T>;

template <typename T>
using is_composite = mp11::mp_or<
    std::is_same<typename T::internal::tag, msm::front::detail::composite_state_tag>,
    has_back_end_tag<T>
    >;

// Call a functor on all elements of List, until the functor returns true.
template <typename List, typename Func>
constexpr bool for_each_until(Func&& func)
{
    bool condition = false;

    boost::mp11::mp_for_each<List>(
        [&func, &condition](auto&& item)
        {
            if (!condition)
            {
                condition = func(std::forward<decltype(item)>(item));
            }
        }
    );
    return condition;
}

// Wrapper for an instance of a type, which might not be present.
template<typename T, bool C>
struct optional_instance;
template <typename T>
struct optional_instance<T, true>
{
    using type = T;
    type instance;
    static constexpr bool value = true;
};
template<typename T>
struct optional_instance<T, false>
{
    using type = T;
    static constexpr bool value = false;
};

// Helper to convert a single type or MPL sequence to Mp11
template<typename T, typename Enable = void>
struct to_mp_list
{
    typedef typename mpl::copy<T, mpl::back_inserter<mp11::mp_list<>>>::type type;
};
template<typename T>
struct to_mp_list<T, std::enable_if_t<!mpl::is_sequence<T>::value>>
{
    using type = mp11::mp_list<T>;
};
template<typename ...T>
struct to_mp_list<mp11::mp_list<T...>>
{
    typedef mp11::mp_list<T...> type;
};
template<typename ...T>
using to_mp_list_t = typename to_mp_list<T...>::type;

template <class stt>
struct generate_state_set;

// iterates through a transition table to generate an ordered state set
// first the source states, transition up to down
// then the target states, up to down
template <class Stt>
struct generate_state_set
{
    typedef to_mp_list_t<Stt> stt;
    // first add the source states
    template <typename V, typename T>
    using set_push_source_state =
        mp11::mp_set_push_back<V, typename T::current_state_type>;
    using source_state_set =
        mp11::mp_fold<stt, mp11::mp_list<>, set_push_source_state>;
    // then add the target states
    template <typename V, typename T>
    using set_push_target_state =
        mp11::mp_set_push_back<V, typename T::next_state_type>;
    using state_set =
        mp11::mp_fold<stt, source_state_set, set_push_target_state>;
};

// extends a state set to a map with key=state and value=id
template <class StateSet>
struct generate_state_map
{
    typedef StateSet state_set;
    typedef mp11::mp_iota<mp11::mp_size<state_set>> indices;
    typedef mp11::mp_transform_q<
        mp11::mp_bind<mp11::mp_list, mp11::_1, mp11::_2>,
        state_set,
        indices
        > type;
};

// returns the id of a given state
template <class StateMap, class State>
struct get_state_id
{
    typedef mp11::mp_second<mp11::mp_map_find<
        StateMap,
        State
        >> type;
    
    static constexpr typename type::value_type value = type::value;
};

// iterates through the transition table and generate a set containing all the events
template <class stt>
struct generate_event_set
{
    typedef to_mp_list_t<stt> stt_mp11;
    template<typename V, typename T>
    using event_set_pusher = mp11::mp_set_push_back<
        V,
        typename T::transition_event
        >;
    typedef mp11::mp_fold<
        to_mp_list_t<stt>,
        mp11::mp_list<>,
        event_set_pusher
        > event_set_mp11;
};

// extends an event set to a map with key=event and value=id
template <class stt>
struct generate_event_map
{
    typedef typename generate_event_set<stt>::event_set_mp11 event_set;
    typedef mp11::mp_iota<mp11::mp_size<event_set>> indices;
    typedef mp11::mp_transform_q<
        mp11::mp_bind<mp11::mp_list, mp11::_1, mp11::_2>,
        event_set,
        indices
        > type;
};

// returns the id of a given event
template <class stt,class Event>
struct get_event_id
{
    typedef mp11::mp_second<mp11::mp_map_find<
        typename generate_event_map<stt>::type,
        Event
        >> type;
    enum {value = type::value};
};

template <class State>
using has_state_deferred_events = mp11::mp_not<
    mp11::mp_empty<to_mp_list_t<typename State::deferred_events>>
    >;

// Template used to create dummy entries for initial states not found in the stt.
template< typename T1 >
struct not_a_row
{
    typedef int not_real_row_tag;
    struct dummy_event 
    {
    };
    typedef T1                  current_state_type;
    typedef T1                  next_state_type;
    typedef dummy_event         transition_event;
};

// used for states created with explicit_creation
// if the state is an explicit entry, we reach for the wrapped state
// otherwise, this returns the state itself
template <class State>
struct get_wrapped_state 
{
    template <typename T>
    using get_wrapped_entry = typename T::wrapped_entry;
    using type = mp11::mp_eval_or<State, get_wrapped_entry, State>;
};

// returns the transition table of a Composite state
template <class Derived>
struct get_transition_table
{
    typedef typename Derived::internal::template create_real_stt<typename Derived::front_end_t>::type Stt;
    // get the state set
    typedef typename generate_state_set<Stt>::state_set states;
    // iterate through the initial states and add them in the stt if not already there
    typedef typename Derived::internal::initial_states initial_states;
    template<typename V, typename T>
    using states_pusher = mp11::mp_if_c<
        mp11::mp_set_contains<states, T>::value,
        V,
        mp11::mp_push_back<
            V,
            not_a_row<typename get_wrapped_state<T>::type>
            >
        >;
    typedef typename mp11::mp_fold<
        to_mp_list_t<initial_states>,
        to_mp_list_t<Stt>,
        states_pusher
        > with_init;

    // do the same for states marked as explicitly created
    template<typename T>
    using get_explicit_creation = to_mp_list_t<typename T::explicit_creation>;
    using fake_explicit_created = mp11::mp_eval_or<
            mp11::mp_list<>,
            get_explicit_creation,
            Derived
            >;
    //converts a "fake" (simulated in a state_machine_ description )state into one which will really get created
    template <class State>
    using convert_fake_state = mp11::mp_if_c<
        has_direct_entry<State>::value,
        typename Derived::template direct<State>,
        State
        >;
    using explicit_created = mp11::mp_transform<
        convert_fake_state,
        fake_explicit_created
        >;
    
    typedef typename mp11::mp_fold<
        to_mp_list_t<explicit_created>,
        with_init,
        states_pusher
        > type;
};
template<typename T>
using get_transition_table_t = typename get_transition_table<T>::type;

// recursively builds an internal table including those of substates, sub-substates etc.
// variant for submachines
template <class State, bool IsComposite>
struct recursive_get_internal_transition_table
{
    // get the composite's internal table
    typedef typename State::front_end_t::internal_transition_table composite_table;
    // and for every substate (state of submachine), recursively get the internal transition table
    using composite_states = typename State::internal::state_set;
    template<typename V, typename SubState>
    using append_recursive_internal_transition_table = mp11::mp_append<
        V,
        typename recursive_get_internal_transition_table<SubState, has_back_end_tag<SubState>::value>::type
        >;
    typedef typename mp11::mp_fold<
        composite_states,
        to_mp_list_t<composite_table>,
        append_recursive_internal_transition_table
        > type;
};
// stop iterating on leafs (simple states)
template <class State>
struct recursive_get_internal_transition_table<State, false>
{
    typedef to_mp_list_t<
        typename State::internal_transition_table
        > type;
};
// recursively get a transition table for a given composite state.
// returns the transition table for this state + the tables of all composite sub states recursively
template <class Composite>
struct recursive_get_transition_table
{
    // get the transition table of the state if it's a state machine
    typedef typename mp11::mp_eval_if_c<
        !has_back_end_tag<Composite>::value,
        mp11::mp_list<>,
        get_transition_table_t,
        Composite
        > org_table;

    typedef typename generate_state_set<org_table>::state_set states;

    // and for every substate, recursively get the transition table if it's a state machine
    template<typename V, typename T>
    using append_recursive_transition_table = mp11::mp_append<
        V,
        typename recursive_get_transition_table<T>::type
        >;
    typedef typename mp11::mp_fold<
        states,
        org_table,
        append_recursive_transition_table> type;
};

// event used internally for wrapping a direct entry
template <class State, class Event>
struct direct_entry_event
{
  public:
    typedef int direct_entry;
    typedef State active_state;
    typedef Event contained_event;

    direct_entry_event(Event const& event):m_event(event){}
    Event const& m_event;
};

//returns the owner of an explicit_entry state
//which is the containing SM if the transition originates from outside the containing SM
//or else the explicit_entry state itself
template <class State,class ContainingSM>
struct get_owner 
{
    using type = mp11::mp_if<
        mp11::mp_same<typename State::owner, ContainingSM>,
        State,
        typename State::owner
        >;
};


template <class Sequence, class ContainingSM>
struct get_fork_owner 
{
    typedef typename ::boost::mpl::front<Sequence>::type seq_front;
    typedef typename ::boost::mpl::if_<
                    typename ::boost::mpl::not_<
                        typename ::boost::is_same<typename seq_front::owner,ContainingSM>::type>::type,
                    typename seq_front::owner, 
                    seq_front >::type type;
};

// builds flags (add internal_flag_list and flag_list). internal_flag_list is used for terminate/interrupt states
template <class State>
struct get_flag_list
{
    typedef typename mp11::mp_append<
        to_mp_list_t<typename State::flag_list>,
        to_mp_list_t<typename State::internal_flag_list>
        > type;
};

template <class State>
struct is_state_blocking 
{
    template<typename T>
    using has_event_blocking_flag = typename has_event_blocking_flag<T>::type;
    typedef typename mp11::mp_any_of<
        typename get_flag_list<State>::type,
        has_event_blocking_flag
        > type;

};
template<typename T>
using is_state_blocking_t = typename is_state_blocking<T>::type;

} // detail

}}} // boost::msm::backmp11

#endif // BOOST_MSM_BACKMP11_METAFUNCTIONS_H
