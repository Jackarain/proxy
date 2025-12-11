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

#ifndef BOOST_MSM_BACKMP11_FAVOR_COMPILE_TIME_H
#define BOOST_MSM_BACKMP11_FAVOR_COMPILE_TIME_H

#include <deque>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>

#include <boost/msm/front/completion_event.hpp>
#include <boost/msm/backmp11/detail/metafunctions.hpp>
#include <boost/msm/backmp11/detail/dispatch_table.hpp>
#include <boost/msm/backmp11/event_traits.hpp>

#define BOOST_MSM_BACKMP11_GENERATE_STATE_MACHINE(smname)                      \
    template<>                                                                  \
    const smname::sm_dispatch_table& smname::sm_dispatch_table::instance()      \
    {                                                                           \
        static dispatch_table table;                                            \
        return table;                                                           \
    }

namespace boost { namespace msm { namespace backmp11
{

struct favor_compile_time
{
    // TODO fix adapter and remove this.
    using compile_policy = int;
};

namespace detail
{

template <typename Policy>
struct compile_policy_impl;
template <>
struct compile_policy_impl<favor_compile_time>
{
    using add_forwarding_rows = mp11::mp_false;

    static bool is_completion_event(const any_event& event)
    {
        return (event.type() == typeid(front::none));
    }

    template<typename Statemachine>
    static bool is_end_interrupt_event(Statemachine& sm, const any_event& event)
    {
        static end_interrupt_event_helper helper{sm};
        return helper.is_end_interrupt_event(event);
    }

    template <typename StateMachine, typename Event>
    static HandledEnum process_event_internal(StateMachine& sm, const Event& event, EventSource source)
    {
        return sm.process_event_internal_impl(any_event(event), source);
    }
    template <typename StateMachine>
    static HandledEnum process_event_internal(StateMachine& sm, const any_event& event, EventSource source)
    {
        return sm.process_event_internal_impl(event, source);
    }

    template <typename State>
    static const std::unordered_set<std::type_index>& get_deferred_event_type_indices()
    {
        static std::unordered_set<std::type_index> type_indices = []()
        {
            std::unordered_set<std::type_index> indices;
            using deferred_events = to_mp_list_t<typename State::deferred_events>;
            using deferred_event_identities = mp11::mp_transform<mp11::mp_identity, deferred_events>;
            mp11::mp_for_each<deferred_event_identities>(
                [&indices](auto event_identity)
                {
                    using Event = typename decltype(event_identity)::type;
                    indices.emplace(to_type_index<Event>());
                }
            );
            return indices;
        }();
        return type_indices;
    }

    template <typename StateMachine>
    static bool is_event_deferred(StateMachine& sm, std::type_index type_index)
    {
        bool result = false;
        auto visitor = [&result, &type_index](auto& state) {
            using State = std::decay_t<decltype(state)>;
            auto& set = get_deferred_event_type_indices<State>();
            result |= (set.find(type_index) != set.end());
        };
        sm.template visit<visit_mode::active_non_recursive>(visitor);
        return result;
    }
    template <typename StateMachine>
    static bool is_event_deferred(StateMachine& sm, const any_event& event)
    {
        return is_event_deferred(sm, event.type());
    }

    template <typename StateMachine>
    static void defer_event(StateMachine& sm, any_event const& event)
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
                [&sm, type_index = std::type_index{event.type()}]()
                {
                    return is_event_deferred(sm, type_index);
                },
                deferred_events.cur_seq_cnt
            }
        );
    }

    template<typename Stt>
    struct get_real_rows
    {
        template<typename Transition>
        using is_real_row = mp11::mp_not<typename has_not_real_row_tag<Transition>::type>;
        typedef mp11::mp_copy_if<Stt, is_real_row> type;
    };

    // Convert an event to a type index.
    template<class Event>
    static std::type_index to_type_index()
    {
        return std::type_index{typeid(Event)};
    }

    // Helper class to manage end interrupt events.
    class end_interrupt_event_helper
    {
    public:
        template<class StateMachine>
        end_interrupt_event_helper(const StateMachine& sm)
        {
            mp11::mp_for_each<mp11::mp_transform<mp11::mp_identity, typename StateMachine::event_set_mp11>>(
                [this, &sm](auto event_identity)
                {
                    using Event = typename decltype(event_identity)::type;
                    using Flag = EndInterruptFlag<Event>;
                    m_is_flag_active_functions[to_type_index<Event>()] =
                        [&sm](){return sm.template is_flag_active<Flag>();};
                });
        }

        bool is_end_interrupt_event(const any_event& event) const
        {
            auto it = m_is_flag_active_functions.find(event.type());
            if (it != m_is_flag_active_functions.end())
            {
                return (it->second)();
            }
            return false;
        }

    private:
        using map = std::unordered_map<std::type_index, std::function<bool()>>;
        map m_is_flag_active_functions;
    };

    struct chain_row
    {
        template<typename Fsm>
        HandledEnum operator()(Fsm& fsm, int region, int state, any_event const& evt) const
        {
            typedef HandledEnum (*real_cell)(Fsm&, int, int, any_event const&);
            HandledEnum res = HandledEnum::HANDLED_FALSE;
            typename std::deque<generic_cell>::const_iterator it = one_state.begin();
            while (it != one_state.end() && (res != HandledEnum::HANDLED_TRUE && res != HandledEnum::HANDLED_DEFERRED ))
            {
                auto fnc = reinterpret_cast<real_cell>(*it);
                HandledEnum handled = (*fnc)(fsm,region,state,evt);
                // reject is considered as erasing an error (HANDLED_FALSE)
                if ((HandledEnum::HANDLED_FALSE==handled) && (HandledEnum::HANDLED_GUARD_REJECT==res) )
                    res = HandledEnum::HANDLED_GUARD_REJECT;
                else
                    res = handled;
                ++it;
            }
            return res;
        }
        // Use a deque with a generic type to avoid unnecessary template instantiations.
        std::deque<generic_cell> one_state;
    };

    // Generates a singleton runtime lookup table that maps current state
    // to a function that makes the SM take its transition on the given
    // Event type.
    template<class Fsm>
    class dispatch_table
    {
        using Stt = typename Fsm::complete_table;
    public:
        // Dispatch an event.
        static HandledEnum dispatch(Fsm& fsm, int region_id, int state_id, const any_event& event)
        {
            return instance().m_state_dispatch_tables[state_id+1].dispatch(fsm, region_id, state_id, event);
        }

        // Dispatch an event to the FSM's internal table.
        static HandledEnum dispatch_internal(Fsm& fsm, int region_id, int state_id, const any_event& event)
        {
            return instance().m_state_dispatch_tables[0].dispatch(fsm, region_id, state_id, event);
        }

    private:
        // Adapter for calling a row's execute function.
        template<typename Event, typename Row>
        static HandledEnum convert_and_execute(Fsm& fsm, int region_id, int state_id, const any_event& event)
        {
            return Row::execute(fsm, region_id, state_id, *any_cast<Event>(&event));
        }

        // Dispatch table for one state.
        class state_dispatch_table
        {
        public:
            // Initialize the submachine call for the given state.
            template<typename State>
            void init_call_submachine()
            {
                m_call_submachine = [](Fsm& fsm, const any_event& evt)
                {
                    return (fsm.template get_state<State&>()).process_event_internal(evt);
                };
            }

            template<typename Event>
            chain_row& get_chain_row()
            {
                return m_entries[to_type_index<Event>()];
            }

            // Dispatch an event.
            HandledEnum dispatch(Fsm& fsm, int region_id, int state_id, const any_event& event) const
            {
                HandledEnum handled = HandledEnum::HANDLED_FALSE;
                if (m_call_submachine)
                {
                    handled = m_call_submachine(fsm, event);
                    if (handled)
                    {
                        return handled;
                    }
                }
                auto it = m_entries.find(event.type());
                if (it != m_entries.end())
                {
                    handled = (it->second)(fsm, region_id, state_id, event);
                }
                return handled;
            }

        private:
            std::unordered_map<std::type_index, chain_row> m_entries;
            // Special functor if the state is a composite
            std::function<HandledEnum(Fsm&, const any_event&)> m_call_submachine;
        };

        dispatch_table()
        {
            // Execute row-specific initializations.
            mp11::mp_for_each<typename get_real_rows<Stt>::type>(
                [this](auto row)
                {
                    using Row = decltype(row);
                    using Event = typename Row::transition_event;
                    using State = typename Row::current_state_type;
                    static constexpr int state_id = Fsm::template get_state_id<State>();
                    auto& chain_row = m_state_dispatch_tables[state_id + 1].template get_chain_row<Event>();
                    chain_row.one_state.push_front(reinterpret_cast<generic_cell>(&convert_and_execute<Event, Row>));
                });

            // Execute state-specific initializations.
            using submachine_states = mp11::mp_copy_if<state_set, has_back_end_tag>;
            mp11::mp_for_each<mp11::mp_transform<mp11::mp_identity, submachine_states>>(
                [this](auto state_identity)
                {
                    using SubmachineState = typename decltype(state_identity)::type;
                    static constexpr int state_id = Fsm::template get_state_id<SubmachineState>();
                    m_state_dispatch_tables[state_id + 1].template init_call_submachine<SubmachineState>();
                });
        }

        // The singleton instance.
        static const dispatch_table& instance();

        // Compute the maximum state value in the sm so we know how big
        // to make the table
        typedef typename generate_state_set<Stt>::state_set state_set;
        BOOST_STATIC_CONSTANT(int, max_state = (mp11::mp_size<state_set>::value));
        state_dispatch_table m_state_dispatch_tables[max_state+1];
    };
};

#ifndef BOOST_MSM_BACKMP11_MANUAL_GENERATION

template<class Fsm>
const typename compile_policy_impl<favor_compile_time>::template dispatch_table<Fsm>& 
compile_policy_impl<favor_compile_time>::dispatch_table<Fsm>::instance()
{
    static dispatch_table table;
    return table;
}

#endif


} // detail

}}} // boost::msm::backmp11

#endif //BOOST_MSM_BACKMP11_FAVOR_COMPILE_TIME_H
