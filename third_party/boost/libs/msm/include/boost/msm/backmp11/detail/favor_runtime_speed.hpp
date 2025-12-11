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

#ifndef BOOST_MSM_BACKMP11_FAVOR_RUNTIME_SPEED_H
#define BOOST_MSM_BACKMP11_FAVOR_RUNTIME_SPEED_H

#include <boost/msm/backmp11/detail/metafunctions.hpp>
#include <boost/msm/backmp11/detail/dispatch_table.hpp>
#include <boost/msm/backmp11/event_traits.hpp>

#include <boost/mpl/advance.hpp>
#include <boost/mpl/begin.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/mpl/empty.hpp>
#include <boost/mpl/erase.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/front.hpp>
#include <boost/mpl/identity.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/pair.hpp>
#include <boost/mpl/pop_front.hpp>


namespace boost { namespace msm { namespace backmp11
{

struct favor_runtime_speed {};

namespace detail
{

template <typename Policy>
struct compile_policy_impl;
template <>
struct compile_policy_impl<favor_runtime_speed>
{
    using add_forwarding_rows = mp11::mp_true;

    template <typename Event>
    static constexpr bool is_completion_event(const Event&)
    {
        return has_completion_event<Event>::value;
    }

    template <typename StateMachine, typename Event>
    static bool is_end_interrupt_event(StateMachine& sm, const Event&)
    {
        return sm.template is_flag_active<EndInterruptFlag<Event>>();
    }

    template <typename StateMachine, typename Event>
    static process_result process_event_internal(StateMachine& sm, const Event& event, EventSource source)
    {
        return sm.process_event_internal_impl(event, source);
    }

    template <typename Event, typename StateMachine>
    static bool is_event_deferred(StateMachine& sm)
    {
        bool result = false;
        auto visitor = [&result](auto& state)
        {
            using State = std::decay_t<decltype(state)>;
            result |= has_state_deferred_event<State, Event>::value;
        };
        sm.template visit<visit_mode::active_non_recursive>(visitor);
        return result;
    }
    template <typename StateMachine, typename Event>
    static bool is_event_deferred(StateMachine& sm, const Event&)
    {
        return is_event_deferred<Event>(sm);
    }

    template <typename StateMachine, typename Event>
    static void defer_event(StateMachine& sm, Event const& event)
    {
        if constexpr (is_kleene_event<Event>::value)
        {
            using event_list = typename StateMachine::event_set_mp11;
            bool found =
                for_each_until<mp11::mp_transform<mp11::mp_identity, event_list>>(
                    [&sm, &event](auto event_identity)
                    {
                        using KnownEvent = typename decltype(event_identity)::type;
                        if (event.type() == typeid(KnownEvent))
                        {
                            do_defer_event(sm, *any_cast<KnownEvent>(&event));
                            return true;
                        }
                        return false;
                    }
            );
            if (!found)
            {
                for (const auto state_id : sm.get_active_state_ids())
                {
                    sm.no_transition(event, sm.get_fsm_argument(), state_id);
                }
            }
        }
        else
        {
            do_defer_event(sm, event);
        }
    }

    template <typename StateMachine, class Event>
    static void do_defer_event(StateMachine& sm, const Event& event)
    {
        auto& deferred_events = sm.get_deferred_events();
        deferred_events.queue.push_back(
            {
                [&sm, event]()
                {
                    return process_event_internal(
                        sm,
                        event,
                        EventSource::EVENT_SOURCE_DEFERRED);
                },
                [&sm]()
                {
                    return is_event_deferred<Event>(sm);
                },
                deferred_events.cur_seq_cnt
            }
        );
    }

    struct cell_initializer
    {
        static void init(generic_cell* entries, const generic_init_cell_value* array, size_t size)
        {
            for (size_t i=0; i<size; i++)
            {
                const auto& item = array[i];
                entries[item.index] = item.address;
            }
        }
    };

    // returns a mp11::mp_bool<true> if State has Event as deferred event
    template <class State, class Event>
    using has_state_deferred_event = mp11::mp_contains<
        to_mp_list_t<typename State::deferred_events>,
        Event
        >;

    template<typename Fsm, typename State>
    struct table_index
    {
        using type = mp11::mp_if<
            mp11::mp_not<is_same<State, Fsm>>,
            mp11::mp_size_t<Fsm::template get_state_id<State>() + 1>,
            mp11::mp_size_t<0>
            >;
    };
    template<typename Fsm, typename State>
    using get_table_index = typename table_index<Fsm, State>::type;

    // Generates a singleton runtime lookup table that maps current state
    // to a function that makes the SM take its transition on the given
    // Event type.
    template<class Fsm>
    class dispatch_table
    {
        using Stt = typename Fsm::complete_table;
    public:
        // Dispatch function for a specific event.
        template<class Event>
        using cell = HandledEnum (*)(Fsm&, int,int,Event const&);

        // Dispatch an event.
        template<class Event>
        static HandledEnum dispatch(Fsm& fsm, int region_id, int state_id, const Event& event)
        {
            return event_dispatch_table<Event>::instance().entries[state_id+1](fsm, region_id, state_id, event);
        }

        // Dispatch an event to the FSM's internal table.
        template<class Event>
        static HandledEnum dispatch_internal(Fsm& fsm, int region_id, int state_id, const Event& event)
        {
            return event_dispatch_table<Event>::instance().entries[0](fsm, region_id, state_id, event);
        }

    private:
        // Compute the maximum state value in the sm so we know how big
        // to make the tables
        typedef typename generate_state_set<Stt>::state_set state_set;
        BOOST_STATIC_CONSTANT(int, max_state = (mp11::mp_size<state_set>::value));

        // Dispatch table for a specific event.
        template<class Event>
        class event_dispatch_table
        {
        public:
            using event_cell = cell<Event>;

            // The singleton instance.
            static const event_dispatch_table& instance() {
                static event_dispatch_table table;
                return table;
            }

        private:
            // A function object for use with mp11::mp_for_each that stuffs transitions into cells.
            class row_init_helper
            {
            public:
                row_init_helper(event_cell* entries)
                    : m_entries(entries) {}

                template<typename Row>
                typename ::boost::disable_if<typename is_kleene_event<typename Row::transition_event>::type, void>::type
                    operator()(Row)
                {
                    m_entries[get_table_index<Fsm, typename Row::current_state_type>::value] =
                        &Row::execute;
                }

                template<typename Row>
                typename ::boost::enable_if<typename is_kleene_event<typename Row::transition_event>::type, void>::type
                    operator()(Row)
                {
                    m_entries[get_table_index<Fsm, typename Row::current_state_type>::value] =
                        &convert_event_and_forward<Row>::execute;
                }

            private:
                event_cell* m_entries;
            };

            static process_result execute_no_transition(Fsm&, int, int, const Event&)
            {
                return process_result::HANDLED_FALSE;
            }

            // initialize the dispatch table for a given Event and Fsm
            event_dispatch_table()
            {
                // Initialize cells for no transition
                for (size_t i=0;i<max_state+1; i++)
                {
                    entries[i] = &execute_no_transition;
                }

                // build chaining rows for rows coming from the same state and the current event
                // first we build a map of sequence for every source
                // in reverse order so that the frow's are handled first (UML priority)
                typedef mp11::mp_fold<
                    mp11::mp_copy_if<
                        to_mp_list_t<Stt>,
                        event_filter_predicate
                        >,
                    mp11::mp_list<>,
                    map_updater
                    > map_of_row_seq;
                // and then build chaining rows for all source states having more than 1 row
                typedef mp11::mp_transform<
                    row_chainer,
                    map_of_row_seq
                    > chained_rows;

                // Go back and fill in cells for matching transitions.
// MSVC crashes when using get_init_cells.
#if !defined(_MSC_VER)
                typedef mp11::mp_transform<
                    preprocess_row,
                    chained_rows
                    > chained_and_preprocessed_rows;
                event_cell_initializer::init(
                    reinterpret_cast<generic_cell*>(entries),
                    get_init_cells<event_cell, chained_and_preprocessed_rows>(),
                    mp11::mp_size<chained_and_preprocessed_rows>::value
                    );
#else
                mp11::mp_for_each<chained_rows>(row_init_helper{entries});
#endif
            }
            
            // class used to build a chain (or sequence) of transitions for a given event and start state
            // (like an UML diamond). Allows transition conflicts.
            template< typename Seq,typename AnEvent,typename State >
            struct chain_row
            {
                typedef State   current_state_type;
                typedef AnEvent transition_event;

                // helper for building a disable/enable_if-controlled execute function
                struct execute_helper
                {
                    template <class Sequence>
                    static
                    HandledEnum
                    execute(Fsm& , int, int, Event const& , ::boost::mpl::true_ const & )
                    {
                        // if at least one guard rejected, this will be ignored, otherwise will generate an error
                        return HandledEnum::HANDLED_FALSE;
                    }

                    template <class Sequence>
                    static
                    HandledEnum
                    execute(Fsm& fsm, int region_index , int state, Event const& evt,
                            ::boost::mpl::false_ const & )
                    {
                        // try the first guard
                        typedef typename ::boost::mpl::front<Sequence>::type first_row;
                        HandledEnum res = first_row::execute(fsm,region_index,state,evt);
                        if (HandledEnum::HANDLED_TRUE!=res && HandledEnum::HANDLED_DEFERRED!=res)
                        {
                            // if the first rejected, move on to the next one
                            HandledEnum sub_res = 
                                execute<typename ::boost::mpl::pop_front<Sequence>::type>(fsm,region_index,state,evt,
                                    ::boost::mpl::bool_<
                                        ::boost::mpl::empty<typename ::boost::mpl::pop_front<Sequence>::type>::type::value>());
                            // if at least one guards rejects, the event will not generate a call to no_transition
                            if ((HandledEnum::HANDLED_FALSE==sub_res) && (HandledEnum::HANDLED_GUARD_REJECT==res) )
                                return HandledEnum::HANDLED_GUARD_REJECT;
                            else
                                return sub_res;
                        }
                        return res;
                    }
                };
                // Take the transition action and return the next state.
                static HandledEnum execute(Fsm& fsm, int region_index, int state, Event const& evt)
                {
                    // forward to helper
                    return execute_helper::template execute<Seq>(fsm,region_index,state,evt,
                        ::boost::mpl::bool_< ::boost::mpl::empty<Seq>::type::value>());
                }
            };
            // nullary metafunction whose only job is to prevent early evaluation of _1
            template< typename Entry > 
            struct make_chain_row_from_map_entry
            { 
                // if we have more than one frow with the same state as source, remove the ones extra
                // note: we know the frow's are located at the beginning so we remove at the beginning (number of frows - 1) elements
                enum { number_frows = boost::mp11::mp_count_if<typename Entry::second, has_is_frow>::value };

                //erases the first NumberToDelete rows
                template<class Sequence, int NumberToDelete>
                struct erase_first_rows
                {
                    typedef typename ::boost::mpl::erase<
                        typename Entry::second,
                        typename ::boost::mpl::begin<Sequence>::type,
                        typename ::boost::mpl::advance<
                                typename ::boost::mpl::begin<Sequence>::type, 
                                ::boost::mpl::int_<NumberToDelete> >::type
                    >::type type;
                };
                // if we have more than 1 frow with this event (not allowed), delete the spare
                typedef typename ::boost::mpl::eval_if<
                    typename ::boost::mpl::bool_< number_frows >= 2 >::type,
                    erase_first_rows<typename Entry::second,number_frows-1>,
                    ::boost::mpl::identity<typename Entry::second>
                >::type filtered_stt;

                typedef chain_row<filtered_stt,Event,
                    typename Entry::first > type;
            }; 
            // helper for lazy evaluation in eval_if of change_frow_event
            template <class Transition,class NewEvent>
            struct replace_event
            {
                typedef typename Transition::template replace_event<NewEvent>::type type;
            };
            // changes the event type for a frow to the event we are dispatching
            // this helps ensure that an event does not get processed more than once because of frows and base events.
            template <class FrowTransition>
            struct change_frow_event
            {
                typedef typename ::boost::mp11::mp_if_c<
                    has_is_frow<FrowTransition>::type::value,
                    replace_event<FrowTransition,Event>,
                    boost::mp11::mp_identity<FrowTransition>
                >::type type;
            };

            template <class Row>
            struct convert_event_and_forward
            {
                static HandledEnum execute(Fsm& fsm, int region_index, int state, Event const& evt)
                {
                    typename Row::transition_event forwarded(evt);
                    return Row::execute(fsm,region_index,state,forwarded);
                }
            };

            using event_init_cell_value = init_cell_value<event_cell>;

            template<size_t v1, event_cell v2>
            using init_cell_constant = init_cell_constant<v1, event_cell, v2>;

            template<event_cell v>
            using cell_constant = std::integral_constant<event_cell, v>;

            using event_cell_initializer = cell_initializer;

            // Helpers for row processing
            // First operation (fold)
            template <typename T>
            using event_filter_predicate = mp11::mp_and<
                mp11::mp_not<has_not_real_row_tag<T>>,
                mp11::mp_or<
                    std::is_base_of<typename T::transition_event, Event>,
                    typename is_kleene_event<typename T::transition_event>::type
                    >
                >;
            template <typename M, typename Key, typename Value>
            using push_map_value = mp11::mp_push_front<
                mp11::mp_second<mp11::mp_map_find<M, Key>>,
                Value>;
            template<typename M, typename T>
            using map_updater = mp11::mp_map_replace<
                M,
                mp11::mp_list<
                    typename T::current_state_type,
                    mp11::mp_eval_if_c<
                        !mp11::mp_map_contains<M, typename T::current_state_type>::value,
                        // first row on this source state, make a list with 1 element
                        mp11::mp_list<typename change_frow_event<T>::type>,
                        // list already exists, add the row
                        push_map_value,
                        M,
                        typename T::current_state_type,
                        typename change_frow_event<T>::type
                        >
                    >
                >;
            // Second operation (transform)
            template<typename T>
            using to_mpl_map_entry = mpl::pair<
                mp11::mp_first<T>,
                mp11::mp_second<T>
                >;
            template<typename T>
            using row_chainer = mp11::mp_if_c<
                (mp11::mp_size<to_mp_list_t<mp11::mp_second<T>>>::value > 1),
                // we need row chaining
                typename make_chain_row_from_map_entry<to_mpl_map_entry<T>>::type,
                // just one row, no chaining, we rebuild the row like it was before
                mp11::mp_front<mp11::mp_second<T>>
                >;
            template<typename Row>
            using preprocess_row_helper = cell_constant<&Row::execute>;
            template<typename Row>
            using preprocess_row = init_cell_constant<
                // Offset into the entries array
                get_table_index<Fsm, typename Row::current_state_type>::value,
                // Address of the execute function
                mp11::mp_eval_if_c<
                    is_kleene_event<typename Row::transition_event>::type::value,
                    cell_constant<
                        &convert_event_and_forward<Row>::execute
                        >,
                    preprocess_row_helper,
                    Row
                    >::value
                >;

        // data members
        public:
            // max_state+1, because 0 is reserved for this fsm (internal transitions)
            event_cell entries[max_state+1];
        };
    };
};

} // detail

}}} // boost::msm::backmp11


#endif //BOOST_MSM_BACKMP11_FAVOR_RUNTIME_SPEED_H
