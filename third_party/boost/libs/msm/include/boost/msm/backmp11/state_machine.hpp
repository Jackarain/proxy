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

#ifndef BOOST_MSM_BACKMP11_STATE_MACHINE_H
#define BOOST_MSM_BACKMP11_STATE_MACHINE_H

#include <array>
#include <exception>
#include <functional>
#include <list>
#include <utility>

#include <boost/core/no_exceptions_support.hpp>
#include <boost/core/ignore_unused.hpp>

#include <boost/mp11.hpp>

#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/identity.hpp>
#include <boost/mpl/is_sequence.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/mpl/and.hpp>

#include <boost/assert.hpp>
#include <boost/ref.hpp>
#include <boost/type_traits/remove_pointer.hpp>
#include <boost/type_traits/add_reference.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/type_traits/is_convertible.hpp>

#include <boost/msm/active_state_switching_policies.hpp>
#include <boost/msm/row_tags.hpp>
#include <boost/msm/backmp11/detail/history_impl.hpp>
#include <boost/msm/backmp11/common_types.hpp>
#include <boost/msm/backmp11/detail/favor_runtime_speed.hpp>
#include <boost/msm/backmp11/state_machine_config.hpp>

namespace boost { namespace msm { namespace backmp11
{

// Check whether a state is a composite state.
using detail::is_composite;

namespace detail
{

constexpr bool is_valid(visit_mode mode) {
    constexpr uint8_t state_mask = 0b011;
    const uint8_t state_bits = static_cast<uint8_t>(mode) & state_mask;
    return state_bits == 0b001 || state_bits == 0b010;
}

template <
    class FrontEnd,
    class Config,
    class Derived
>
class state_machine_base : public FrontEnd
{
    static_assert(
        is_composite<FrontEnd>::value,
        "FrontEnd must be a composite state");
    static_assert(
        is_config<Config>::value,
        "Config must be an instance of state machine config");

  public:
    using config_t = Config;
    using root_sm_t = typename config_t::root_sm;
    using context_t = typename config_t::context;
    using front_end_t = FrontEnd;
    using derived_t = Derived;
    using events_queue_t = typename config_t::template
        queue_container<std::function<process_result()>>;

    // Event that describes the SM is starting.
    // Used when the front-end does not define an initial_event.
    struct starting {};
    // Event that describes the SM is stopping.
    // Used when the front-end does not define a final_event.
    struct stopping {};

    template <class ExitPoint>
    struct exit_pt : public ExitPoint
    {
        // tags
        typedef ExitPoint           wrapped_exit;
        typedef int                 pseudo_exit;
        typedef derived_t           owner;
        typedef int                 no_automatic_create;
        typedef typename
            ExitPoint::event        Event;
        typedef std::function<process_result (Event const&)>
                                    forward_function;

        // forward event to the higher-level FSM
        template <class ForwardEvent>
        void forward_event(ForwardEvent const& incomingEvent)
        {
            // use helper to forward or not
            ForwardHelper< ::boost::is_convertible<ForwardEvent,Event>::value>::helper(incomingEvent,m_forward);
        }
        void set_forward_fct(forward_function fct)
        {
            m_forward = fct;
        }
        exit_pt():m_forward(){}
        // by assignments, we keep our forwarding functor unchanged as our containing SM did not change
        template <class RHS>
        exit_pt(RHS&):m_forward(){}
        exit_pt<ExitPoint>& operator= (const exit_pt<ExitPoint>& )
        {
            return *this;
        }

        private:
        forward_function          m_forward;

        // using partial specialization instead of enable_if because of VC8 bug
        template <bool OwnEvent, int Dummy=0>
        struct ForwardHelper
        {
            template <class ForwardEvent>
            static void helper(ForwardEvent const& ,forward_function& )
            {
                // Not our event, assert
                BOOST_ASSERT(false);
            }
        };
        template <int Dummy>
        struct ForwardHelper<true,Dummy>
        {
            template <class ForwardEvent>
            static void helper(ForwardEvent const& incomingEvent,forward_function& forward_fct)
            {
                // call if handler set, if not, this state is simply a terminate state
                if (forward_fct)
                    forward_fct(incomingEvent);
            }
        };
    };

    template <class EntryPoint>
    struct entry_pt : public EntryPoint
    {
        // tags
        typedef EntryPoint          wrapped_entry;
        typedef int                 pseudo_entry;
        typedef derived_t           owner;
        typedef int                 no_automatic_create;
    };
    template <class EntryPoint>
    struct direct : public EntryPoint
    {
        // tags
        typedef EntryPoint          wrapped_entry;
        typedef int                 explicit_entry_state;
        typedef derived_t           owner;
        typedef int                 no_automatic_create;
    };

    struct internal
    {
        using tag = back_end_tag;

        using initial_states = to_mp_list_t<typename front_end_t::initial_state>;
        static constexpr int nr_regions = mp11::mp_size<initial_states>::value;

        template <class State, typename Enable = void>
        struct make_entry
        {
            using type = State;
        };
        template <class State>
        struct make_entry<State, std::enable_if_t<has_pseudo_entry<State>::value>>
        {
            using type = entry_pt<State>;
        };
        template <class State>
        struct make_entry<State, std::enable_if_t<has_direct_entry<State>::value>>
        {
            using type = direct<State>;
        };

        template <class State, typename Enable = void>
        struct make_exit
        {
            using type = State;
        };
        template <class State>
        struct make_exit<State, std::enable_if_t<has_pseudo_exit<State>::value>>
        {
            using type = exit_pt<State>;
        };


        template<typename Row, bool HasGuard, typename Event, typename Source, typename Target>
        static bool call_guard_or_true(state_machine_base& sm, const Event& event, Source& source, Target& target)
        {
            if constexpr (HasGuard)
            {
                return Row::guard_call(sm.get_fsm_argument(), event, source, target, sm.m_states);
            }
            else
            {
                return true;
            }
        }
        template<typename Row, bool HasAction, typename Event, typename Source, typename Target>
        static process_result call_action_or_true(state_machine_base& sm, const Event& event, Source& source, Target& target)
        {
            if constexpr (HasAction)
            {
                return Row::action_call(sm.get_fsm_argument(), event, source, target, sm.m_states);
            }
            else
            {
                return process_result::HANDLED_TRUE;
            }
        }

        // Template used to create transitions from rows in the transition table
        // (normal transitions).
        template<typename Row, bool HasAction, bool HasGuard>
        struct Transition
        {
            typedef typename Row::Evt transition_event;
            typedef typename make_entry<typename Row::Source>::type T1;
            // if the source is an exit pseudo state, then
            // current_state_type becomes the result of get_owner
            // meaning the containing SM from which the exit occurs
            typedef typename ::boost::mpl::eval_if<
                    typename has_pseudo_exit<T1>::type,
                    get_owner<T1,derived_t>,
                    ::boost::mpl::identity<typename Row::Source> >::type current_state_type;

            typedef typename make_exit<typename Row::Target>::type T2;
            // if Target is a sequence, then we have a fork and expect a sequence of explicit_entry
            // else if Target is an explicit_entry, next_state_type becomes the result of get_owner
            // meaning the containing SM if the row is "outside" the containing SM or else the explicit_entry state itself
            typedef typename ::boost::mpl::eval_if<
                typename ::boost::mpl::is_sequence<T2>::type,
                get_fork_owner<T2,derived_t>,
                ::boost::mpl::eval_if<
                        typename has_no_automatic_create<T2>::type,
                        get_owner<T2,derived_t>,
                        ::boost::mpl::identity<T2> >
            >::type next_state_type;

            // Take the transition action and return the next state.
            static process_result execute(state_machine_base& sm, int region_index, int state, transition_event const& event)
            {
                BOOST_STATIC_CONSTANT(int, current_state = (get_state_id<current_state_type>()));
                BOOST_STATIC_CONSTANT(int, next_state = (get_state_id<next_state_type>()));
                boost::ignore_unused(state); // Avoid warnings if BOOST_ASSERT expands to nothing.
                BOOST_ASSERT(state == (current_state));
                // if T1 is an exit pseudo state, then take the transition only if the pseudo exit state is active
                if (has_pseudo_exit<T1>::type::value &&
                    !sm.is_exit_state_active<T1, get_owner<T1,derived_t>>())
                {
                    return process_result::HANDLED_FALSE;
                }

                auto& source = sm.get_state<current_state_type>();
                auto& target = sm.get_state<next_state_type>();

                if (!call_guard_or_true<Row, HasGuard>(sm, event, source, target))
                {
                    // guard rejected the event, we stay in the current one
                    return process_result::HANDLED_GUARD_REJECT;
                }
                sm.m_active_state_ids[region_index] = active_state_switching::after_guard(current_state,next_state);

                // first call the exit method of the current state
                source.on_exit(event, sm.get_fsm_argument());
                sm.m_active_state_ids[region_index] = active_state_switching::after_exit(current_state,next_state);

                // then call the action method
                process_result res = call_action_or_true<Row, HasAction>(sm, event, source, target);
                sm.m_active_state_ids[region_index] = active_state_switching::after_action(current_state,next_state);

                // and finally the entry method of the new current state
                convert_event_and_execute_entry<T2>(target,event,sm);
                sm.m_active_state_ids[region_index] = active_state_switching::after_entry(current_state,next_state);

                return res;
            }
        };

        // Template used to create transitions from rows in the transition table
        // (internal transitions).
        template<typename Row, bool HasAction, bool HasGuard, typename State = typename Row::Source>
        struct InternalTransition
        {
            typedef typename Row::Evt transition_event;
            typedef State current_state_type;
            typedef current_state_type next_state_type;

            // Take the transition action and return the next state.
            static process_result execute(state_machine_base& sm, int , int state, transition_event const& event)
            {

                BOOST_STATIC_CONSTANT(int, current_state = (get_state_id<current_state_type>()));
                boost::ignore_unused(state, current_state); // Avoid warnings if BOOST_ASSERT expands to nothing.
                BOOST_ASSERT(state == (current_state));

                auto& source = sm.get_state<current_state_type>();
                auto& target = source;

                if (!call_guard_or_true<Row, HasGuard>(sm, event, source, target))
                {
                    // guard rejected the event, we stay in the current one
                    return process_result::HANDLED_GUARD_REJECT;
                }

                // call the action method
                return call_action_or_true<Row, HasAction>(sm, event, source, target);
            }
        };

        template <class Tag, class Row,class State>
        struct create_backend_stt;
        template <class Row,class State>
        struct create_backend_stt<g_row_tag,Row,State>
        {
            using type = Transition<Row, false, true>;
        };
        template <class Row,class State>
        struct create_backend_stt<a_row_tag,Row,State>
        {
            using type = Transition<Row, true, false>;
        };
        template <class Row,class State>
        struct create_backend_stt<_row_tag,Row,State>
        {
            using type = Transition<Row, false, false>;
        };
        template <class Row,class State>
        struct create_backend_stt<row_tag,Row,State>
        {
            using type = Transition<Row, true, true>;
        };
        template <class Row,class State>
        struct create_backend_stt<g_irow_tag,Row,State>
        {
            using type = InternalTransition<Row, false, true>;
        };
        template <class Row,class State>
        struct create_backend_stt<a_irow_tag,Row,State>
        {
            using type = InternalTransition<Row, true, false>;
        };
        template <class Row,class State>
        struct create_backend_stt<irow_tag,Row,State>
        {
            using type = InternalTransition<Row, true, true>;
        };
        template <class Row,class State>
        struct create_backend_stt<_irow_tag,Row,State>
        {
            using type = InternalTransition<Row, false, false>;
        };
        template <class Row,class State>
        struct create_backend_stt<sm_a_i_row_tag,Row,State>
        {
            using type = InternalTransition<Row, true, false, State>;
        };
        template <class Row,class State>
        struct create_backend_stt<sm_g_i_row_tag,Row,State>
        {
            using type = InternalTransition<Row, false, true, State>;
        };
        template <class Row,class State>
        struct create_backend_stt<sm_i_row_tag,Row,State>
        {
            using type = InternalTransition<Row, true, true, State>;
        };
        template <class Row,class State>
        struct create_backend_stt<sm__i_row_tag,Row,State>
        {
            using type = InternalTransition<Row, false, false, State>;
        };
        template <class Row,class State=void>
        struct transition_cell
        {
            using type = typename create_backend_stt<typename Row::row_type_tag,Row,State>::type;
        };

        // add to the stt the initial states which could be missing (if not being involved in a transition)
        template <class TFrontEnd, class stt_simulated = typename TFrontEnd::transition_table>
        struct create_real_stt
        {
            template<typename T>
            using get_transition_cell = typename transition_cell<T, TFrontEnd>::type;
            typedef typename boost::mp11::mp_transform<
                get_transition_cell,
                to_mp_list_t<stt_simulated>
            > type;
        };

        using stt = typename get_transition_table<state_machine_base>::type;
        using state_set = typename generate_state_set<stt>::state_set;
    };

    typedef mp11::mp_rename<typename internal::state_set, std::tuple> states_t;

  private:
    using stt = typename internal::stt;
    using state_set = typename internal::state_set;
    static constexpr int nr_regions = internal::nr_regions;
    using active_state_ids_t = std::array<int, nr_regions>;
    using initial_states_identity = mp11::mp_transform<mp11::mp_identity, typename internal::initial_states>;
    using compile_policy = typename config_t::compile_policy;
    using compile_policy_impl = detail::compile_policy_impl<compile_policy>;
    template<class Row, class State>
    using transition_cell = typename internal::template transition_cell<Row, State>;
    template<class TFrontEnd, class stt_simulated>
    using create_real_stt = typename internal::template create_real_stt<TFrontEnd, stt_simulated>;
    

    template<typename T>
    using get_active_state_switch_policy = typename T::active_state_switch_policy;
    using active_state_switching =
        boost::mp11::mp_eval_or<active_state_switch_after_entry,
                                get_active_state_switch_policy, front_end_t>;

    typedef bool (*flag_handler)(state_machine_base const &);

    // all state machines are friend with each other to allow embedding any of them in another fsm
    template <class, class, class>
    friend class state_machine_base;

    template <typename Policy>
    friend struct detail::compile_policy_impl;

    // Allow access to private members for serialization.
    // WARNING:
    // No guarantee is given on the private member layout.
    // Future changes may break existing serializer implementations.
    template<typename T, typename A0, typename A1, typename A2>
    friend void serialize(T&, state_machine_base<A0, A1, A2>&);

    template <typename T>
    using get_initial_event = typename T::initial_event;
    using fsm_initial_event =
        boost::mp11::mp_eval_or<starting, get_initial_event, front_end_t>;

    template <typename T>
    using get_final_event = typename T::final_event;
    using fsm_final_event =
        boost::mp11::mp_eval_or<stopping, get_final_event, front_end_t>;

    // Template used to form forwarding rows in the transition table for every row of a composite SM
    template <typename T1, class Evt>
    struct frow
    {
        typedef T1                  current_state_type;
        typedef T1                  next_state_type;
        typedef Evt                 transition_event;
        // tag to find out if a row is a forwarding row
        typedef int                 is_frow;

        // Take the transition action and return the next state.
        static process_result execute(state_machine_base& sm, int region_index, int , transition_event const& event)
        {
            // false as second parameter because this event is forwarded from outer fsm
            process_result res =
                (sm.get_state<current_state_type>()).process_event_internal(event);
            sm.m_active_state_ids[region_index]=get_state_id<T1>();
            return res;
        }
        // helper metafunctions used by dispatch table and give the frow a new event
        // (used to avoid double entries in a table because of base events)
        template <class NewEvent>
        struct replace_event
        {
            typedef frow<T1,NewEvent> type;
        };
    };

    template <class Table,class Intermediate,class State>
    struct add_forwarding_row_helper
    {
        typedef typename generate_event_set<Table>::event_set_mp11 all_events;

        template<typename T>
        using frow_state_type = frow<State, T>;
        typedef mp11::mp_append<
            to_mp_list_t<Intermediate>,
            mp11::mp_transform<frow_state_type, all_events>
            > type;
    };
    // gets the transition table from a composite and make from it a forwarding row
    template <class State,bool IsComposite>
    struct get_internal_transition_table
    {
        // first get the table of a composite
        typedef typename recursive_get_transition_table<State>::type original_table;

        // we now look for the events the composite has in its internal transitions
        // the internal ones are searched recursively in sub-sub... states
        // we go recursively because our states can also have internal tables or substates etc.
        typedef typename recursive_get_internal_transition_table<State, true>::type recursive_istt;
        template<typename T>
        using get_transition_cell = typename transition_cell<T, State>::type;
        typedef boost::mp11::mp_transform<
            get_transition_cell,
            to_mp_list_t<recursive_istt>
        > recursive_istt_with_tag;

        typedef boost::mp11::mp_append<original_table, recursive_istt_with_tag> table_with_all_events;

        // and add for every event a forwarding row
        typedef typename ::boost::mpl::eval_if<
                typename compile_policy_impl::add_forwarding_rows,
                add_forwarding_row_helper<table_with_all_events,mp11::mp_list<>,State>,
                ::boost::mpl::identity< mp11::mp_list<> >
        >::type type;
    };
    template <class State>
    struct get_internal_transition_table<State, false>
    {
        typedef typename create_real_stt<State, typename State::internal_transition_table >::type type;
    };
    
    typedef typename generate_state_map<state_set>::type state_map_mp11;
    typedef typename generate_event_set<stt>::event_set_mp11 event_set_mp11;
    typedef history_impl<typename front_end_t::history, nr_regions> concrete_history;
    typedef typename generate_event_set<
        typename create_real_stt<front_end_t, typename front_end_t::internal_transition_table >::type
    >::event_set_mp11 processable_events_internal_table;

    // extends the transition table with rows from composite states
    template <class Composite>
    struct extend_table
    {
        // add the init states
        //typedef typename get_transition_table<Composite>::type stt;
        typedef typename Composite::stt Stt;

        // add the internal events defined in the internal_transition_table
        // Note: these are added first because they must have a lesser prio
        // than the deeper transitions in the sub regions
        // table made of a stt + internal transitions of composite
        template<typename T>
        using get_transition_cell = typename transition_cell<T, Composite>::type;
        typedef typename boost::mp11::mp_transform<
            get_transition_cell,
            to_mp_list_t<typename Composite::internal_transition_table>
        > internal_stt;

        typedef boost::mp11::mp_append<
            to_mp_list_t<Stt>,
            internal_stt
        > stt_plus_internal;

        // for every state, add its transition table (if any)
        // transformed as frow
        template<typename V, typename State>
        using F = boost::mp11::mp_append<
            V,
            typename get_internal_transition_table<State, is_composite<State>::value>::type
            >;
        typedef boost::mp11::mp_fold<
            state_set,
            stt_plus_internal,
            F
        > type;
    };
    // extend the table with tables from composite states
    typedef typename extend_table<state_machine_base>::type complete_table;
    // define the dispatch table used for event dispatch
    using sm_dispatch_table = typename compile_policy_impl::template dispatch_table<state_machine_base>;

    struct deferred_event_t
    {
        std::function<process_result()> process_event;
        std::function<bool()> is_event_deferred;
        // Deferred events are added with a correlation sequence that helps to
        // identify when an event was added.
        // Newly deferred events will not be considered for procesing
        // within the same sequence.
        size_t seq_cnt;
    };
    using deferred_events_queue_t = std::list<deferred_event_t>;

    struct deferred_events_t
    {
        deferred_events_queue_t queue;
        size_t cur_seq_cnt;
    };
    using has_any_deferred_event =
        mp11::mp_any_of<state_set, has_state_deferred_events>;
    using deferred_events_member =
        optional_instance<deferred_events_t,
                          has_any_deferred_event::value ||
                              has_activate_deferred_events<front_end_t>::value>;
    using events_queue_member =
        optional_instance<events_queue_t,
                          !has_no_message_queue<front_end_t>::value>;
    using context_member =
        optional_instance<context_t*,
                          !std::is_same_v<context_t, no_context> &&
                              (std::is_same_v<root_sm_t, no_root_sm> ||
                               std::is_same_v<root_sm_t, derived_t>)>;

    template <bool C = deferred_events_member::value,
              typename = std::enable_if_t<C>>
    deferred_events_t& get_deferred_events()
    {
        return m_optional_members.template get<deferred_events_member>();
    }

    template <bool C = deferred_events_member::value,
              typename = std::enable_if_t<C>>
    const deferred_events_t& get_deferred_events() const
    {
        return m_optional_members.template get<deferred_events_member>();
    }

    template <class Event>
    bool is_event_deferred(const Event& event) const
    {
        return compile_policy_impl::is_event_deferred(
            *const_cast<state_machine_base*>(this), event);
    }

    // Visit states with a compile-time filter (reduces template instantiations).
    template <template <typename> typename Predicate, visit_mode Mode, typename Visitor>
    void visit_if(Visitor&& visitor)
    {
        // TODO:
        // Filter needs to be passed to visit to reduce template instantiations.
        visit<Mode>(
            [&visitor](auto& state)
            {
                using State = std::decay_t<decltype(state)>;
                if constexpr (Predicate<State>())
                {
                    std::invoke(std::forward<Visitor>(visitor), state);
                }
            });
    }
    
  public:
    // Construct and forward constructor arguments to the front-end.
    template <typename... Args>
    state_machine_base(Args&&... args)
        : front_end_t(std::forward<Args>(args)...)
    {
        static_assert(
            std::is_base_of_v<state_machine_base, derived_t>,
            "Derived must inherit from state_machine");
        if constexpr (!std::is_same_v<context_t, no_context>)
        {
            static_assert(
                std::is_constructible_v<derived_t, context_t&>,
                "Derived must inherit the base class constructors");
        }
        if constexpr (std::is_same_v<root_sm_t, no_root_sm> ||
                      std::is_same_v<root_sm_t, derived_t>)
        {
            // create states
            init(*static_cast<derived_t*>(this));
        }
        reset_active_state_ids();
    }

    // Construct with a context and forward further constructor arguments to the front-end.
    template <bool C = context_member::value,
              typename = std::enable_if_t<C>,
              typename... Args>
    state_machine_base(context_t& context, Args&&... args)
        : state_machine_base(std::forward<Args>(args)...)
        {
            m_optional_members.template get<context_member>() = &context;
            if constexpr (std::is_same_v<root_sm_t, no_root_sm>)
            {
                constexpr visit_mode mode = visit_mode::all_states | visit_mode::recursive;
                visit_if<is_back_end, mode>(
                    [&context](auto &state_machine)
                    {
                        state_machine.m_optional_members.template get<context_member>() = &context;
                    });
            }
        }
    
    // Copy constructor.
    state_machine_base(state_machine_base const& rhs)
        : front_end_t(rhs)
    {
        if constexpr (std::is_same_v<root_sm_t, no_root_sm> ||
                      std::is_same_v<root_sm_t, derived_t>)
        {
            // create states
            init(*static_cast<derived_t*>(this));
        }
        // Copy all members except the root sm pointer.
        m_active_state_ids = rhs.m_active_state_ids;
        m_optional_members = rhs.m_optional_members;
        m_history = rhs.m_history;
        m_event_processing = rhs.m_event_processing;
        m_states = rhs.m_states;
        m_running = rhs.m_running;
    }

    // Copy assignment operator.
    state_machine_base& operator= (state_machine_base const& rhs)
    {
        if (this != &rhs)
        {
           front_end_t::operator=(rhs);
            // Copy all members except the root sm pointer.
            m_active_state_ids = rhs.m_active_state_ids;
            m_optional_members = rhs.m_optional_members;
            m_history = rhs.m_history;
            m_event_processing = rhs.m_event_processing;
            m_states = rhs.m_states;
            m_running = rhs.m_running;
        }
       return *this;
    }

    // Start the state machine (calls entry of the initial state).
    void start()
    {
        // Assert for a case where root sm was not set up correctly
        // after construction.
        if constexpr (!std::is_same_v<typename Config::root_sm, no_root_sm>)
        {
            BOOST_ASSERT_MSG(&(this->get_root_sm()),
            "Root sm must be passed as Derived and configured as root_sm");
        }
        start(fsm_initial_event{});
    }

    // Start the state machine (calls entry of the initial state with initial_event to on_entry's).
    template <class Event>
    void start(Event const& initial_event)
    {
        if (!m_running)
        {
            internal_start<Event, fsm_parameter_t, true>(initial_event, get_fsm_argument());
        }
    }

    // stop the state machine (calls exit of the current state)
    void stop()
    {
        stop(fsm_final_event{});
    }

    // stop the state machine (calls exit of the current state passing finalEvent to on_exit's)
    template <class Event>
    void stop(Event const& final_event)
    {
        if (m_running)
        {
            on_exit(final_event, get_fsm_argument());
            m_running = false;
        }
    }

    // Check whether a state is currently active.
  public:
    template <typename State>
    bool is_state_active() const
    {
        bool found = false;
        const_cast<state_machine_base*>(this)->visit(
            [&found](const auto& state)
            {
                using StateToCheck = std::decay_t<decltype(state)>;
                found |= std::is_same_v<State, StateToCheck>;
            });
        return found;
    }

    // Main function to process events.
    template<class Event>
    process_result process_event(Event const& event)
    {
        return process_event_internal(event, EventSource::EVENT_SOURCE_DIRECT);
    }

    // Enqueues an event in the message queue.
    // Call process_queued_events to process all queued events.
    // Be careful if you do this during event processing, the event will be processed immediately
    // and not kept in the queue.
    template <class Event,
              bool C = events_queue_member::value,
              typename = std::enable_if_t<C>>
    void enqueue_event(Event const& event)
    {
        get_events_queue().push_back(
            [this, event]
            {
                return process_event_internal(
                    event,
                    EventSource::EVENT_SOURCE_DIRECT |
                    EventSource::EVENT_SOURCE_MSG_QUEUE);
            }
        );
    }

    // Process all queued events.
    template <bool C = events_queue_member::value,
              typename = std::enable_if_t<C>>
    void process_queued_events()
    {
        while(!get_events_queue().empty())
        {
            process_single_queued_event();
        }
    }

    // Process a single queued event.
    template <bool C = events_queue_member::value,
              typename = std::enable_if_t<C>>
    void process_single_queued_event()
    {
        auto to_call = get_events_queue().front();
        get_events_queue().pop_front();
        to_call();
    }

    // Get the context of the state machine.
    template <bool C = !std::is_same_v<context_t, no_context>,
              typename = std::enable_if_t<C>>
    context_t& get_context()
    {
        if constexpr (context_member::value)
        {
            return *m_optional_members.template get<context_member>();
        }
        else
        {
            return get_root_sm().get_context();
        }
    }

    // Get the context of the state machine.
    template <bool C = !std::is_same_v<context_t, no_context>,
              typename = std::enable_if_t<C>>
    const context_t& get_context() const
    {
        if constexpr (context_member::value)
        {
            return *m_optional_members.template get<context_member>();
        }
        else
        {
            return get_root_sm().get_context();
        }
    }

    // Get the events queued for later processing.
    template <bool C = events_queue_member::value,
              typename = std::enable_if_t<C>>
    events_queue_t& get_events_queue()
    {
        return m_optional_members.template get<events_queue_member>();
    }

    // Get the events queued for later processing.
    template <bool C = events_queue_member::value,
              typename = std::enable_if_t<C>>
    const events_queue_t& get_events_queue() const
    {
       return m_optional_members.template get<events_queue_member>();
    }

    // Get the deferred events queued for later processing.
    template <bool C = deferred_events_member::value,
              typename = std::enable_if_t<C>>
    deferred_events_queue_t& get_deferred_events_queue()
    {
        return get_deferred_events().queue;
    }

    // Get the deferred events queued for later processing.
    template <bool C = deferred_events_member::value,
              typename = std::enable_if_t<C>>
    const deferred_events_queue_t& get_deferred_events_queue() const
    {
        return get_deferred_events().queue;
    }

    // Getter that returns the currently active state ids of the FSM.
    const active_state_ids_t& get_active_state_ids() const
    {
        return m_active_state_ids;
    }

    // Get the root sm.
    template <typename T = root_sm_t, 
              typename = std::enable_if_t<!std::is_same_v<T, no_root_sm>>>
    root_sm_t& get_root_sm()
    {
        return *static_cast<root_sm_t*>(m_root_sm);
    }
    // Get the root sm.
    template <typename T = root_sm_t, 
              typename = std::enable_if_t<!std::is_same_v<T, no_root_sm>>>
    const root_sm_t& get_root_sm() const
    {
        return *static_cast<const root_sm_t*>(m_root_sm);
    }

    // Return the id of a state in the sm.
    template<typename State>
    static constexpr int get_state_id(const State&)
    {
        static_assert(mp11::mp_map_contains<state_map_mp11, State>::value);
        return detail::get_state_id<state_map_mp11, State>::type::value;
    }
    // Return the id of a state in the sm.
    template<typename State>
    static constexpr int get_state_id()
    {
        static_assert(mp11::mp_map_contains<state_map_mp11, State>::value);
        return detail::get_state_id<state_map_mp11, State>::type::value;
    }

    // True if the sm is used in another sm.
    bool is_contained() const
    {
        return (static_cast<const void*>(this) != m_root_sm);
    }

    // Get a state.
    template <class State>
    State& get_state()
    {
        return std::get<std::remove_reference_t<State>>(m_states);
    }
    // Get a state.
    template <class State>
    const State& get_state() const
    {
        return std::get<std::remove_reference_t<State>>(m_states);
    }

    // checks if a flag is active using the BinaryOp as folding function
    template <class Flag,class BinaryOp>
    bool is_flag_active() const
    {
        flag_handler* flags_entries = get_entries_for_flag<Flag>();
        bool res = (*flags_entries[ m_active_state_ids[0] ])(*this);
        for (int i = 1; i < nr_regions ; ++i)
        {
            res = BinaryOp() (res,(*flags_entries[ m_active_state_ids[i] ])(*this));
        }
        return res;
    }
    // checks if a flag is active using no binary op if 1 region, or OR if > 1 regions
    template <class Flag>
    bool is_flag_active() const
    {
        return FlagHelper<Flag,(nr_regions>1)>::helper(*this,get_entries_for_flag<Flag>());
    }

    // Visit the states (only active states, recursive).
    template <typename Visitor>
    constexpr void visit(Visitor&& visitor)
    {
        visit<visit_mode::active_recursive>(std::forward<Visitor>(visitor));
    }

    // Visit the states.
    // How to traverse is selected with visit_mode.
    template <visit_mode Mode, typename Visitor>
    constexpr void visit(Visitor&& visitor)
    {
        static_assert(
            is_valid(Mode),
            "Mode must specify one of active_states or all_states");
        constexpr bool recursive = has_flag(Mode, visit_mode::recursive);
        if constexpr (has_flag(Mode, visit_mode::active_states))
        {
            if (m_running)
            {
                for (const int state_id : m_active_state_ids)
                {
                    using table = visitor_dispatch_table<Visitor, recursive>;
                    table::dispatch(*this, state_id, std::forward<Visitor>(visitor));
                }
            }
        }
        // all states
        else
        {
            mp11::tuple_for_each(m_states,
                [&visitor](auto& state)
                {
                    std::invoke(std::forward<Visitor>(visitor), state);

                    using State = std::decay_t<decltype(state)>;
                    // recursive needs to be repeated in this lambda,
                    // MSVC does not recognize the constexpr correctly.
                    constexpr bool recursive = has_flag(Mode, visit_mode::recursive);
                    if constexpr (has_back_end_tag<State>::value && recursive)
                    {
                        state.template visit<Mode>(std::forward<Visitor>(visitor));
                    }
                }
            );
        }   
    }

    // Puts the given event into the deferred events queue.
    template <
        class Event,
        bool C = deferred_events_member::value,
        typename = std::enable_if_t<C>>
    void defer_event(Event const& event)
    {
        compile_policy_impl::defer_event(*this, event);
    }

  protected:
    static_assert(std::is_same_v<typename config_t::fsm_parameter, transition_owner> ||
                    (std::is_same_v<typename config_t::fsm_parameter, typename config_t::root_sm> &&
                     !std::is_same_v<typename config_t::root_sm, no_root_sm>),
                  "fsm_parameter must be transition_owner or root_sm"
                 );
    using fsm_parameter_t = mp11::mp_if_c<
        std::is_same_v<typename config_t::fsm_parameter, transition_owner>,
        derived_t,
        typename config_t::root_sm>;

    fsm_parameter_t& get_fsm_argument()
    {
        if constexpr (std::is_same_v<typename config_t::fsm_parameter,
                                     transition_owner>)
        {
            return *static_cast<derived_t*>(this);
        }
        else
        {
            return get_root_sm();
        }
    }

    // Checks if an event is an end interrupt event.
    template <typename Event>
    bool is_end_interrupt_event(const Event& event) const
    {
        return compile_policy_impl::is_end_interrupt_event(*this, event);
    }

    // Helpers used to reset the state machine.
    void reset_active_state_ids()
    {
       size_t index = 0;
       mp11::mp_for_each<initial_states_identity>(
       [this, &index](auto state_identity)
       {
           using State = typename decltype(state_identity)::type;
           m_active_state_ids[index++] = get_state_id<State>();
       });
       m_history.reset_active_state_ids(m_active_state_ids);
    }
    
    // handling of deferred events
    void try_process_deferred_events()
    {
        if constexpr (deferred_events_member::value)
        {
            deferred_events_t& deferred_events = get_deferred_events();
            if (deferred_events.queue.empty())
            {
                return;
            }

            active_state_ids_t active_state_ids = m_active_state_ids;
            // Iteratively process all of the events within the deferred
            // queue up to (but not including) newly deferred events.
            auto it = deferred_events.queue.begin();
            do
            {
                if (deferred_events.cur_seq_cnt == it->seq_cnt)
                {
                    return;
                }
                if (it->is_event_deferred())
                {
                    it = std::next(it);
                }
                else
                {
                    deferred_event_t deferred_event = std::move(*it);
                    it = deferred_events.queue.erase(it);
                    const process_result result = deferred_event.process_event();

                    if ((result & process_result::HANDLED_TRUE) &&
                        (active_state_ids != m_active_state_ids))
                    {
                        // The active state configuration has changed.
                        // Start from the beginning, we might be able
                        // to process events that stayed in the queue before.
                        active_state_ids = m_active_state_ids;
                        it = deferred_events.queue.begin();
                    }
                }
            } while (it != deferred_events.queue.end());
        }
    }

    // handling of eventless transitions
    void try_process_completion_event(EventSource source, bool handled)
    {
        using first_completion_event = mp11::mp_find_if<event_set_mp11, has_completion_event>;
        // if none is found in the SM, nothing to do
        if constexpr (first_completion_event::value != mp11::mp_size<event_set_mp11>::value)
        {
            if (handled)
            {
                process_event_internal(
                    mp11::mp_at<event_set_mp11, first_completion_event>{},
                    source | EventSource::EVENT_SOURCE_DIRECT);
            }
        }
    }

    // Handling of enqueued events.
    void try_process_queued_events()
    {
        if constexpr (events_queue_member::value)
        {
            process_queued_events();
        }
    }

    // Main function used internally to make transitions
    // Can only be called for internally (for example in an action method) generated events.
    template<class Event>
    process_result process_event_internal(Event const& event,
                           EventSource source = EventSource::EVENT_SOURCE_DEFAULT)
    {
        // The compile policy decides whether the event needs to be wrapped or not.
        // After wrapping it should call back process_event_internal_impl.
        return compile_policy_impl::process_event_internal(*this, event, source);
    }

    template<class Event>
    process_result process_event_internal_impl(Event const& event, EventSource source)
    {
        // If the state machine has terminate or interrupt flags, check them.
        if constexpr (mp11::mp_any_of<state_set, is_state_blocking_t>::value)
        {
            // If the state machine is terminated, do not handle any event.
            if (is_flag_active<TerminateFlag>())
            {
                return process_result::HANDLED_TRUE;
            }
            // If the state machine is interrupted, do not handle any event
            // unless the event is the end interrupt event.
            if (is_flag_active<InterruptedFlag>() && !is_end_interrupt_event(event))
            {
                return process_result::HANDLED_TRUE;
            }
        }

        // If we have an event queue and are already processing events,
        // enqueue it for later processing.
        if constexpr (events_queue_member::value)
        {
            if (m_event_processing)
            {
                enqueue_event(event);
                return process_result::HANDLED_TRUE;
            }
        }

        // If deferred events are configured and the event is to be deferred
        // in the active state configuration, then defer it for later processing.
        if constexpr (has_any_deferred_event::value)
        {
            if (is_event_deferred(event))
            {
                compile_policy_impl::defer_event(*this, event);
                return process_result::HANDLED_DEFERRED;
            }
        }

        // Process the event.
        m_event_processing = true;
        process_result handled;
        const bool is_direct_call = source & EventSource::EVENT_SOURCE_DIRECT;
        if constexpr (has_no_exception_thrown<front_end_t>::value)
        {
            handled = do_process_event(event, is_direct_call);
        }
        else
        {
            // when compiling without exception support there is no formal parameter "e" in the catch handler.
            // Declaring a local variable here does not hurt and will be "used" to make the code in the handler
            // compilable although the code will never be executed.
            std::exception e;
            BOOST_TRY
            {
                handled = do_process_event(event, is_direct_call);
            }
            BOOST_CATCH (std::exception& e)
            {
                // give a chance to the concrete state machine to handle
                this->exception_caught(event, get_fsm_argument(), e);
                handled = process_result::HANDLED_FALSE;
            }
            BOOST_CATCH_END
        }

        // at this point we allow the next transition be executed without enqueing
        // so that completion events and deferred events execute now (if any)
        m_event_processing = false;

        // Process completion transitions BEFORE any other event in the
        // pool (UML Standard 2.3 15.3.14)
        try_process_completion_event(source, (handled & process_result::HANDLED_TRUE));

        // After handling, take care of the queued and deferred events.
        // Default:
        // Handle deferred events queue with higher prio than events queue.
        if constexpr (!has_event_queue_before_deferred_queue<front_end_t>::value)
        {
            if (!(EventSource::EVENT_SOURCE_DEFERRED & source))
            {
                try_process_deferred_events();

                // Handle any new events generated into the queue, but only if
                // we're not already processing from the message queue.
                if (!(EventSource::EVENT_SOURCE_MSG_QUEUE & source))
                {
                    try_process_queued_events();
                }
            }
        }
        // Non-default:
        // Handle events queue with higher prio than deferred events queue.
        else
        {
            if (!(EventSource::EVENT_SOURCE_MSG_QUEUE & source))
            {
                try_process_queued_events();
                if (!(EventSource::EVENT_SOURCE_DEFERRED & source))
                {
                    try_process_deferred_events();
                }
            }
        }

        return handled;
    }

    // minimum event processing without exceptions, queues, etc.
    template<class Event>
    process_result do_process_event(Event const& event, bool is_direct_call)
    {
        if constexpr (deferred_events_member::value)
        {
            if (is_direct_call)
            {
                get_deferred_events().cur_seq_cnt += 1;
            }
        }

        process_result handled = process_result::HANDLED_FALSE;
        // Dispatch the event to every region.
        for (int region_id=0; region_id<nr_regions; region_id++)
        {
            handled = static_cast<process_result>(
                static_cast<int>(handled) |
                static_cast<int>(sm_dispatch_table::dispatch(*this, region_id, m_active_state_ids[region_id], event))
            );
        }
        // Process the event in the internal table of this fsm if the event is processable (present in the table).
        if constexpr (mp11::mp_set_contains<processable_events_internal_table,Event>::value)
        {
            handled = static_cast<process_result>(
                static_cast<int>(handled) |
                static_cast<int>(sm_dispatch_table::dispatch_internal(*this, 0, m_active_state_ids[0], event))
            );
        }

        // if the event has not been handled and we have orthogonal zones, then
        // generate an error on every active state
        // for state machine states contained in other state machines, do not handle
        // but let the containing sm handle the error, unless the event was generated in this fsm
        // (by calling process_event on this fsm object, is_direct_call == true)
        // completion events do not produce an error
        if ((!is_contained() || is_direct_call) && !handled && !compile_policy_impl::is_completion_event(event))
        {
            for (const auto state_id: m_active_state_ids)
            {
                this->no_transition(event, get_fsm_argument(), state_id);
            }
        }
        return handled;
    }

private:
    // helper for flag handling. Uses OR by default on orthogonal zones.
    template <class Flag,bool OrthogonalStates>
    struct FlagHelper
    {
        static bool helper(state_machine_base const& sm,flag_handler* )
        {
            // by default we use OR to accumulate the flags
            return sm.is_flag_active<Flag,std::logical_or<bool>>();
        }
    };
    template <class Flag>
    struct FlagHelper<Flag,false>
    {
        static bool helper(state_machine_base const& sm,flag_handler* flags_entries)
        {
            // just one active state, so we can call operator[] with 0
            return flags_entries[sm.get_active_state_ids()[0]](sm);
        }
    };
    // handling of flag
    // defines a true and false functions plus a forwarding one for composite states
    template <class State,class Flag>
    struct FlagHandler
    {
        static bool flag_true(state_machine_base const& )
        {
            return true;
        }
        static bool flag_false(state_machine_base const& )
        {
            return false;
        }
        static bool forward(state_machine_base const& fsm)
        {
            return fsm.template get_state<State>().template is_flag_active<Flag>();
        }
    };
    template <class Flag>
    struct init_flags
    {
    private:
        // helper function, helps hiding the forward function for non-state machines states.
        template <class T>
        void helper (flag_handler* an_entry,int offset, ::boost::mpl::true_ const &  )
        {
            // composite => forward
            an_entry[offset] = &FlagHandler<T,Flag>::forward;
        }
        template <class T>
        void helper (flag_handler* an_entry,int offset, ::boost::mpl::false_ const &  )
        {
            // default no flag
            an_entry[offset] = &FlagHandler<T,Flag>::flag_false;
        }
        // attributes
        flag_handler* entries;

    public:
        init_flags(flag_handler* entries_)
            : entries(entries_)
        {}

        // Flags initializer function object, used with for_each
        template <class State>
        void operator()( mp11::mp_identity<State> const& )
        {
            typedef typename get_flag_list<State>::type flags;
            typedef mp11::mp_contains<flags,Flag > found;

            BOOST_STATIC_CONSTANT(int, state_id = (get_state_id<State>()));
            if (found::type::value)
            {
                // the type defined the flag => true
                entries[state_id] = &FlagHandler<State,Flag>::flag_true;
            }
            else
            {
                // false or forward
                typedef typename ::boost::mpl::and_<
                            typename has_back_end_tag<State>::type,
                            typename ::boost::mpl::not_<
                                    typename has_non_forwarding_flag<Flag>::type>::type >::type composite_no_forward;

                helper<State>(entries,state_id,::boost::mpl::bool_<composite_no_forward::type::value>());
            }
        }
    };
    // maintains for every flag a static array containing the flag value for every state
    template <class Flag>
    flag_handler* get_entries_for_flag() const
    {
        BOOST_STATIC_CONSTANT(int, max_state = (mp11::mp_size<state_set>::value));

        static flag_handler flags_entries[max_state];
        // build a state list, but only once
        static flag_handler* flags_entries_ptr =
            (mp11::mp_for_each<mp11::mp_transform<mp11::mp_identity, state_set>>
                            (init_flags<Flag>(flags_entries)),
            flags_entries);
        return flags_entries_ptr;
    }

    template <class Event, class Fsm, bool InitialStart = false>
    void internal_start(Event const& event, Fsm& fsm)
    {
        m_running = true;
        
        // Call on_entry on this SM first.
        static_cast<front_end_t*>(this)->on_entry(event, fsm);

        // Then call on_entry on the states.
        if constexpr (InitialStart)
        {
            mp11::mp_for_each<initial_states_identity>(
                [this, &event, &fsm](auto state_identity)
                {
                    using State = typename decltype(state_identity)::type;
                    execute_entry(this->get_state<State>(), event, fsm);
                });
        }
        else 
        {
            // Visit active states non-recursively.
            visit(
                [&event, &fsm](auto& state)
                {
                    // TODO:
                    // Add filter to rule out impossible entry states.
                    execute_entry(state, event, fsm);
                });
        }
        
        // give a chance to handle an anonymous (eventless) transition
        try_process_completion_event(EventSource::EVENT_SOURCE_DEFAULT, true);
    }

    // helper to find out if a SM has an active exit state and is therefore waiting for exiting
    template <class State, class StateOwner>
    inline
    bool is_exit_state_active()
    {
        if constexpr (has_pseudo_exit<State>::value)
        {
            typedef typename StateOwner::type Owner;
            Owner& owner = get_state<Owner&>();
            const int state_id = owner.template get_state_id<State>();
            for (const auto active_state_id : owner.get_active_state_ids())
            {
                if (active_state_id == state_id)
                {
                    return true;
                }
            }
        }
        return false;
    }

     template <class State>
     struct find_region_id
     {
         template <int region,int Dummy=0>
         struct In
         {
             enum {region_index=region};
         };
        enum {region_index = In<State::zone_index>::region_index };
     };

    // entry for states machines which are themselves embedded in other state machines (composites)
    template <class Event, class Fsm>
    void on_entry(Event const& event, Fsm& fsm)
    {
        // block immediate handling of events
        m_event_processing = true;
        // by default we activate the history/init states, can be overwritten by direct events.
        m_active_state_ids = m_history.on_entry(event);

        // this variant is for the standard case, entry due to activation of the containing FSM
        if constexpr (!has_direct_entry<Event>::value)
        {
            internal_start(event, fsm);
        }
        // this variant is for the direct entry case
        else if constexpr (has_direct_entry<Event>::value)
        {
            // Set the new active state(s) first, this includes
            // a normal direct entry (or entries) and a pseudo entry.
            using entry_states = to_mp_list_t<typename Event::active_state>;
            mp11::mp_for_each<mp11::mp_transform<mp11::mp_identity, entry_states>>(
                [this](auto state_identity)
                {
                    using State = typename decltype(state_identity)::type::wrapped_entry;
                    static constexpr int region_index = find_region_id<State>::region_index;
                    static_assert(region_index >= 0 && region_index < nr_regions);
                    m_active_state_ids[region_index] = get_state_id<State>();
                }
            );
            internal_start(event.m_event, fsm);
            // in case of a pseudo entry process the transition in the zone of the newly active state
            // (entry pseudo states are, according to UML, a state connecting 1 transition outside to 1 inside
            if constexpr (has_pseudo_entry<typename Event::active_state>::value)
            {
                static_assert(!mpl::is_sequence<typename Event::active_state>::value);
                process_event(event.m_event);
            }
        }
        // handle messages which were generated and blocked in the init calls
        m_event_processing = false;
        // look for deferred events waiting
        try_process_deferred_events();
        try_process_queued_events();
    }
    template <class Event,class Fsm>
    void on_exit(Event const& event, Fsm& fsm)
    {
        // first recursively exit the sub machines
        // forward the event for handling by sub state machines
        visit(
            [&event, &fsm](auto& state)
            {
                // TODO:
                // Filter out impossible exit states.
                state.on_exit(event, fsm);
            }
        );
        // then call our own exit
        (static_cast<front_end_t*>(this))->on_exit(event,fsm);
        // give the history a chance to handle this (or not).
        m_history.on_exit(this->m_active_state_ids);
        // history decides what happens with deferred events
        if (!m_history.process_deferred_events(event))
        {
            if constexpr (deferred_events_member::value)
            {
                get_deferred_events_queue().clear();
            }
        }
    }

    // calls entry or on_entry depending on the state type
    template <class State, class Event, class Fsm>
    static void execute_entry(State& state, Event const& event, Fsm& fsm)
    {
        // calls on_entry on the fsm then handles direct entries, fork, entry pseudo state
        if constexpr (has_back_end_tag<State>::value)
        {
            state.on_entry(event,fsm);
        }
        else if constexpr (has_pseudo_exit<State>::value)
        {
            // calls on_entry on the state then forward the event to the transition which should be defined inside the
            // contained fsm
            state.on_entry(event,fsm);
            state.forward_event(event);
        }
        else if constexpr (has_direct_entry<Event>::value)
        {
            state.on_entry(event.m_event, fsm);
        }
        else
        {
            state.on_entry(event, fsm);
        }
        
    }

    // helper allowing special handling of direct entries / fork
    template <class Target,class State,class Event>
    static void convert_event_and_execute_entry(State& state,Event const& event, state_machine_base& sm)
    {
        auto& fsm = sm.get_fsm_argument();
        if constexpr (has_explicit_entry_state<Target>::value || mpl::is_sequence<Target>::value)
        {
            // for the direct entry, pack the event in a wrapper so that we handle it differently during fsm entry
            execute_entry(state,direct_entry_event<Target,Event>(event),fsm);
        }
        else
        {
            // if the target is a normal state, do the standard entry handling
            execute_entry(state,event,fsm);
        }
    }

    template <typename State>
    using state_filter_predicate = mp11::mp_or<
        has_pseudo_exit<State>,
        has_back_end_tag<State>
        >;
    using states_to_init = mp11::mp_copy_if<
        states_t,
        state_filter_predicate>;
    
    // initializes the SM
    template <class TRootSm>
    void init(TRootSm& root_sm)
    {
        if constexpr (!std::is_same_v<root_sm_t, no_root_sm>)
        {
            static_assert(
                std::is_same_v<TRootSm, root_sm_t>,
                "The configured root_sm must match the used one"
            );
            static_assert(
                std::is_same_v<context_t, no_context> ||
                std::is_same_v<context_t, typename TRootSm::context_t>,
                "The configured context must match the root sm's one");
        }
        m_root_sm = static_cast<void*>(&root_sm);

        mp11::mp_for_each<mp11::mp_transform<mp11::mp_identity, states_to_init>>(
            [this, &root_sm](auto state_identity)
            {
                using State = typename decltype(state_identity)::type;
                auto& state = this->get_state<State>();
                
                if constexpr (has_pseudo_exit<State>::value)
                {
                    state.set_forward_fct(
                        [&root_sm](typename State::event const& event)
                        {
                            return root_sm.process_event(event);
                        }
                    );
                }

                if constexpr (is_back_end<State>::value)
                {
                    static_assert(
                        std::is_same_v<compile_policy, typename State::compile_policy>,
                        "All compile policies must be identical"
                    );
                    state.init(root_sm);
                }
            }
        );
    }

    // Dispatch table for calling invoking visitors with active states.
    template <typename Visitor, bool Recursive>
    class visitor_dispatch_table
    {
    public:
        visitor_dispatch_table()
        {
            using state_identities = mp11::mp_transform<mp11::mp_identity, state_set>;
            mp11::mp_for_each<state_identities>(
                [this](auto state_identity)
                {
                    using State = typename decltype(state_identity)::type;
                    m_cells[get_state_id<State>()] = &accept<State>;
                }
            );
        }

        template<typename State>
        static void accept(state_machine_base& sm, Visitor visitor)
        {
            auto& state = sm.template get_state<State>();
            visitor(state);

            if constexpr (has_back_end_tag<State>::value && Recursive)
            {
                state.template visit
                    <visit_mode::active_states | visit_mode::recursive>
                    (std::forward<Visitor>(visitor));
            }
        }

        static void dispatch(state_machine_base& sm, int index, Visitor visitor)
        {
            instance().m_cells[index](sm, visitor);
        }

    private:
        using cell_t = void (*)(state_machine_base&, Visitor);

        static visitor_dispatch_table& instance()
        {
            static visitor_dispatch_table instance;
            return instance;
        }

        cell_t m_cells[mp11::mp_size<state_set>::value];
    };

    struct optional_members :
        events_queue_member,
        deferred_events_member,
        context_member
    {
        template <typename T>
        typename T::type& get()
        {
            return static_cast<T*>(this)->instance;
        }
        template <typename T>
        const typename T::type& get() const
        {
            return static_cast<const T*>(this)->instance;
        }
    };

    // data members
    active_state_ids_t   m_active_state_ids;
    optional_members     m_optional_members;
    concrete_history     m_history{};
    bool                 m_event_processing{false};
    void*                m_root_sm{nullptr};
    states_t             m_states{};
    bool                 m_running{false};
};

} // detail

/**
 * @brief Back-end for state machines.
 *
 * Can take 1...3 parameters.
 * 
 * @tparam T0 (mandatory) : Front-end
 * @tparam T1 (optional)  : State machine config
 * @tparam T2 (optional)  : Derived class (required when inheriting from state_machine)
 */
template <typename ...T>
class state_machine;

template <class FrontEnd, class Config, class Derived>
class state_machine<FrontEnd, Config, Derived>
    : public detail::state_machine_base<FrontEnd, Config, Derived>
{
    using Base = detail::state_machine_base<FrontEnd, Config, Derived>;
  public:
    using Base::Base;
};

template <class FrontEnd, class Config>
class state_machine<FrontEnd, Config>
    : public detail::state_machine_base<FrontEnd, Config, state_machine<FrontEnd, Config>>
{
    using Base = detail::state_machine_base<FrontEnd, Config, state_machine<FrontEnd, Config>>;
  public:
    using Base::Base;
};

template <class FrontEnd>
class state_machine<FrontEnd>
    : public detail::state_machine_base<FrontEnd, default_state_machine_config, state_machine<FrontEnd>>
{
    using Base = detail::state_machine_base<FrontEnd, default_state_machine_config, state_machine<FrontEnd>>;
  public:
    using Base::Base;
};

}}} // boost::msm::backmp11

#endif //BOOST_MSM_BACKMP11_STATE_MACHINE_H
