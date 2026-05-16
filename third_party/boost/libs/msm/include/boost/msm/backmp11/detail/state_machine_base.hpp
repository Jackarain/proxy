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

#ifndef BOOST_MSM_BACKMP11_DETAIL_STATE_MACHINE_BASE_HPP
#define BOOST_MSM_BACKMP11_DETAIL_STATE_MACHINE_BASE_HPP

#include <array>
#include <cstdint>
#include <exception>
#include <utility>

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/mp11.hpp>

#include <boost/msm/active_state_switching_policies.hpp>
#include <boost/msm/row_tags.hpp>
#include <boost/msm/backmp11/detail/basic_polymorphic.hpp>
#include <boost/msm/backmp11/detail/favor_runtime_speed.hpp>
#include <boost/msm/backmp11/detail/history_impl.hpp>
#include <boost/msm/backmp11/detail/state_visitor.hpp>
#include <boost/msm/backmp11/detail/transition_table.hpp>
#include <boost/msm/backmp11/common_types.hpp>
#include <boost/msm/backmp11/state_machine_config.hpp>

namespace boost::msm::backmp11::detail
{

// Wrapper for not modifying T during copy and move operations.
template <typename T>
class non_propagating
{
  public:
    non_propagating() = default;

    explicit non_propagating(const T& value) : m_value(value)
    {
    }

    non_propagating& operator=(const non_propagating&)
    {
        return *this;
    }

    non_propagating(non_propagating&&)
    {
    }

    non_propagating& operator=(non_propagating&&)
    {
        return *this;
    }

    T& operator*()
    {
        return m_value;
    }
    const T& operator*() const
    {
        return m_value;
    }

  private:
    T m_value;
};

template <class FrontEnd, class Config, class Derived>
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

    // Event that describes the SM is starting.
    // Used when the front-end does not define an initial_event.
    struct starting {};
    // Event that describes the SM is stopping.
    // Used when the front-end does not define a final_event.
    struct stopping {};

    // Wrapper for an exit pseudostate,
    // which upper SMs can use to connect to it.
    template <class ExitPseudostate>
    struct exit_pt : public ExitPseudostate
    {
        // tags
        struct internal
        {
            using tag = exit_pseudostate_be_tag;
        };
        using state = ExitPseudostate;
        using owner = derived_t;
        using event = typename ExitPseudostate::event;
        using forward_fn_t = void (*)(void* /*root_sm*/, const void* /*event*/);

        template <typename RootSm>
        void init()
        {
            m_forward_fn = &call_enqueue_event<RootSm, event>;
        }

        // forward event to the root sm.
        template <class ForwardEvent>
        void forward_event(void* root_sm, const ForwardEvent& forward_event)
        {
            static_assert(
                std::is_convertible_v<ForwardEvent, event>,
                "ForwardEvent must be convertible to exit pseudostate's event");
            // Call if handler set.
            // If not, this state is simply a terminate state.
            if (m_forward_fn)
            {
                m_forward_fn(root_sm, &forward_event);
            }
        }

      private:
        template <typename RootSm, typename Event>
        static void call_enqueue_event(void* root_sm, const void* event)
        {
            static_cast<RootSm*>(root_sm)->enqueue_event(
                *static_cast<const Event*>(event));
        }

        forward_fn_t m_forward_fn{};
    };

    // Wrapper for an entry pseudostate,
    // which upper SMs can use to connect to it.
    template <class EntryPseudostate>
    struct entry_pt : public EntryPseudostate
    {
        // tags
        struct internal
        {
            using tag = entry_pseudostate_be_tag;
        };

        using state = EntryPseudostate;
        using owner = derived_t;
    };

    // Wrapper for a direct entry,
    // which upper SMs can use to connect to it.
    template <class State>
    struct direct : public State
    {
        // tags
        struct internal
        {
            using tag = explicit_entry_be_tag;
        };
        using state = State;
        using owner = derived_t;
    };

    struct internal
    {
        using tag = state_machine_tag;

        using initial_states = to_mp_list_t<typename front_end_t::initial_state>;
        static constexpr int nr_regions = mp11::mp_size<initial_states>::value;

        using state_set = generate_state_set<state_machine_base>;
        using submachines = mp11::mp_copy_if<state_set, is_composite>;
    };

    using states_t = mp11::mp_rename<typename internal::state_set, std::tuple>;

  protected:
    using processable_event = basic_polymorphic<event_occurrence>;
    template <typename T>
    using event_container = typename config_t::template event_container<T>;
    using event_container_t = event_container<processable_event>;

    struct event_pool_t
    {
        event_container_t events;
        uint16_t cur_seq_cnt{};
    };

    using event_pool_member = optional_instance<
        event_pool_t,
        !std::is_same_v<event_container<void>, no_event_container<void>>>;

    template <bool C = event_pool_member::value,
              typename = std::enable_if_t<C>>
    event_pool_t& get_event_pool()
    {
        return m_optional_members.template get<event_pool_member>();
    }

    template <bool C = event_pool_member::value,
              typename = std::enable_if_t<C>>
    const event_pool_t& get_event_pool() const
    {
        return m_optional_members.template get<event_pool_member>();
    }

  private:
    using state_set = typename internal::state_set;
    static constexpr int nr_regions = internal::nr_regions;
    using active_state_ids_t = std::array<int, nr_regions>;
    using initial_state_identities = mp11::mp_transform<mp11::mp_identity, typename internal::initial_states>;
    using compile_policy = typename config_t::compile_policy;
    using compile_policy_impl = detail::compile_policy_impl<compile_policy>;

    template<typename T>
    using get_active_state_switch_policy = typename T::active_state_switch_policy;
    using active_state_switching =
        mp11::mp_eval_or<active_state_switch_after_entry,
                         get_active_state_switch_policy, front_end_t>;

    template <class, class, class>
    friend class state_machine_base;

    template <typename StateMachine>
    friend struct transition_table_impl;

    template <typename Policy, typename>
    friend struct detail::compile_policy_impl;

    template <typename, typename, visit_mode, bool,
              template <typename> typename...>
    friend class state_visitor_impl;
    template <typename, bool, template <typename> typename...>
    friend class state_visitor_base_impl;
    template <typename, typename, template <typename> typename...>
    friend class event_deferral_visitor;
    template <typename>
    friend class init_state_visitor;

    template <typename Event>
    friend class deferred_event;

    // Allow access to private members for serialization.
    // WARNING:
    // No guarantee is given on the private member layout.
    // Future changes may break existing serializer implementations.
    template<typename T, typename A0, typename A1, typename A2>
    friend void serialize(T&, state_machine_base<A0, A1, A2>&);

    template <typename T>
    using get_initial_event = typename T::initial_event;
    using fsm_initial_event =
        mp11::mp_eval_or<starting, get_initial_event, front_end_t>;

    template <typename T>
    using get_final_event = typename T::final_event;
    using fsm_final_event =
        mp11::mp_eval_or<stopping, get_final_event, front_end_t>;
    
    using state_map = generate_state_map<state_set>;
    using history_impl = detail::history_impl<typename front_end_t::history, nr_regions>;

    using context_member =
        optional_instance<context_t*,
                          !std::is_same_v<context_t, no_context> &&
                              (std::is_same_v<root_sm_t, no_root_sm> ||
                               std::is_same_v<root_sm_t, derived_t>)>;

    // Visit states with a compile-time filter (reduces template instantiations).
    // Kept private for now, because the API of this method is not stable yet
    // and this optimization is likely not needed to be available in the public API.
    template <visit_mode Mode, template <typename> typename... Predicates, typename Visitor>
    void visit_if(Visitor&& visitor)
    {
        using state_visitor =
            state_visitor<derived_t, Visitor, Mode, Predicates...>;
        state_visitor::visit(self(), visitor);
    }
    template <visit_mode Mode, template <typename> typename... Predicates, typename Visitor>
    void visit_if(Visitor&& visitor) const
    {
        using state_visitor =
            state_visitor<const derived_t, Visitor, Mode, Predicates...>;
        state_visitor::visit(self(), visitor);
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
            *m_root_sm = this;
            using visitor_t = init_state_visitor<derived_t>;
            visitor_t visitor{self()};
            visit_if<visit_mode::all_recursive, 
                     visitor_t::template predicate>(visitor);
        }
        reset_active_state_ids();
    }

    // Construct with a context and
    // forward further constructor arguments to the front-end.
    template <bool C = context_member::value,
              typename = std::enable_if_t<C>,
              typename... Args>
    state_machine_base(context_t& context, Args&&... args)
        : state_machine_base(std::forward<Args>(args)...)
    {
        m_optional_members.template get<context_member>() = &context;
        if constexpr (std::is_same_v<root_sm_t, no_root_sm>)
        {
            visit_if<visit_mode::all_recursive, has_state_machine_tag>(
                [&context](auto& state_machine) {
                    state_machine.m_optional_members
                        .template get<context_member>() = &context;
                });
        }
    }

    // Copy constructor.
    state_machine_base(state_machine_base const& rhs) : state_machine_base()
    {
        *this = rhs;
    }

    // Copy assignment operator.
    state_machine_base& operator=(state_machine_base const& rhs) = default;

    // Move constructor.
    state_machine_base(state_machine_base&& rhs) : state_machine_base()
    {
        *this = std::move(rhs);
    }

    // Move assignment operator.
    state_machine_base& operator=(state_machine_base&& rhs) = default;

    // Start the state machine (calls entry of the initial state(s)).
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

    // Start the state machine
    // (calls entry of the initial state(s) with initial_event).
    template <class Event>
    void start(Event const& initial_event)
    {
        if (!m_running)
        {
            on_entry(initial_event, get_fsm_argument());
        }
    }

    // Stop the state machine (calls exit of the current state(s)).
    void stop()
    {
        stop(fsm_final_event{});
    }

    // Stop the state machine
    // (calls exit of the current state(s) with final_event).
    template <class Event>
    void stop(Event const& final_event)
    {
        if (m_running)
        {
            on_exit(final_event, get_fsm_argument());
            m_running = false;
        }
    }

    // Main function to process events.
    template<class Event>
    process_result process_event(Event const& event)
    {
        return process_event_internal(
            compile_policy_impl::normalize_event(event),
            process_info::direct_call);
    }

    // Try to process pending event occurrences in the event pool,
    // with an optional limit for the max no. of events that shall be processed.
    // Returns the no. of processed events.
    template <bool C = event_pool_member::value,
              typename = std::enable_if_t<C>>
    inline size_t process_event_pool(size_t max_events = SIZE_MAX)
    {
        if (get_event_pool().events.empty() || m_event_processing)
        {
            return 0;
        }
        return do_process_event_pool(max_events);
    }

    // Enqueues an event in the event pool for later processing.
    // If the state machine is already processing, the event will be processed
    // after the current event completes.
    template <class Event,
              bool C = event_pool_member::value,
              typename = std::enable_if_t<C>>
    void enqueue_event(Event const& event)
    {
        compile_policy_impl::defer_event(
            *this, compile_policy_impl::normalize_event(event), false);
    }

    // Process all queued events.
    template <bool C = event_pool_member::value,
              typename = std::enable_if_t<C>>
    [[deprecated ("Use process_event_pool() instead")]]
    void process_queued_events()
    {
        process_event_pool();
    }

    // Process a single queued event.
    template <bool C = event_pool_member::value,
              typename = std::enable_if_t<C>>
    [[deprecated ("Use process_event_pool(1) instead")]]
    void process_single_queued_event()
    {
        process_event_pool(1);
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
        return *static_cast<root_sm_t*>(*m_root_sm);
    }
    // Get the root sm.
    template <typename T = root_sm_t, 
              typename = std::enable_if_t<!std::is_same_v<T, no_root_sm>>>
    const root_sm_t& get_root_sm() const
    {
        return *static_cast<const root_sm_t*>(*m_root_sm);
    }

    // Return the id of a state in the sm.
    template<typename State>
    static constexpr int get_state_id(const State&)
    {
        static_assert(
            mp11::mp_map_contains<state_map, State>::value,
            "The state must be contained in the state machine");
        return detail::get_state_id<state_map, State>::value;
    }
    // Return the id of a state in the sm.
    template<typename State>
    static constexpr int get_state_id()
    {
        static_assert(
            mp11::mp_map_contains<state_map, State>::value,
            "The state must be contained in the state machine");
        return detail::get_state_id<state_map, State>::value;
    }

    // True if the sm is used in another sm.
    bool is_contained() const
    {
        return (static_cast<const void*>(this) != *m_root_sm);
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

    // Visit the states (only active states, recursive).
    template <typename Visitor>
    void visit(Visitor&& visitor)
    {
        visit<visit_mode::active_recursive>(std::forward<Visitor>(visitor));
    }

    // Visit the states (only active states, recursive).
    template <typename Visitor>
    void visit(Visitor&& visitor) const
    {
        visit<visit_mode::active_recursive>(std::forward<Visitor>(visitor));
    }

    // Visit the states.
    // How to traverse is selected with visit_mode.
    template <visit_mode Mode, typename Visitor>
    void visit(Visitor&& visitor)
    {
        visit_if<Mode>(std::forward<Visitor>(visitor));
    }

    // Visit the states.
    // How to traverse is selected with visit_mode.
    template <visit_mode Mode, typename Visitor>
    void visit(Visitor&& visitor) const
    {
        visit_if<Mode>(std::forward<Visitor>(visitor));
    }

    // Check whether a state is currently active.
    template <typename State>
    bool is_state_active() const
    {
        using visitor_t = is_state_active_visitor<State>;
        visitor_t visitor;
        visit_if<visit_mode::active_recursive,
                 visitor_t::template predicate>(visitor);
        return visitor.result();
    }

    // Check if a flag is active, using the BinaryOp as folding function.
    template <typename Flag, typename BinaryOp = flag_or>
    bool is_flag_active() const
    {
        using visitor_t = is_flag_active_visitor<Flag, BinaryOp>;
        visitor_t visitor;
        visit_if<visit_mode::active_recursive,
                 visitor_t::template predicate>(visitor);
        return visitor.result();
    }

    // Puts the event into the event pool for later processing.
    // If the deferral takes place while the state machine is processing,
    // the event will be evaluated for dispatch from the next processing cycle.
    template <
        class Event,
        bool C = event_pool_member::value,
        typename = std::enable_if_t<C>>
    void defer_event(Event const& event)
    {
        compile_policy_impl::defer_event(
            *this, compile_policy_impl::normalize_event(event), m_event_processing);
    }

  protected:
    static_assert(std::is_same_v<typename config_t::fsm_parameter, local_transition_owner> ||
                    (std::is_same_v<typename config_t::fsm_parameter, typename config_t::root_sm> &&
                     !std::is_same_v<typename config_t::root_sm, no_root_sm>),
                  "fsm_parameter must be local_transition_owner or root_sm"
                 );
    using fsm_parameter_t = mp11::mp_if_c<
        std::is_same_v<typename config_t::fsm_parameter, local_transition_owner>,
        derived_t,
        typename config_t::root_sm>;

    const fsm_parameter_t& get_fsm_argument() const
    {
        if constexpr (std::is_same_v<typename config_t::fsm_parameter,
                                     local_transition_owner>)
        {
            return self();
        }
        else
        {
            return get_root_sm();
        }
    }

    fsm_parameter_t& get_fsm_argument()
    {
        return const_cast<fsm_parameter_t&>
            (static_cast<const state_machine_base&>(*this).get_fsm_argument());
    }

    template <typename Event>
    bool is_event_deferred(const Event& event) const
    {
        return compile_policy_impl::is_event_deferred(self(), event);
    }

    // Repetition of the front-end's method definition
    // required due to above signature.
    template <typename Event, typename Fsm>
    bool is_event_deferred(const Event& event, Fsm& fsm) const
    {
        return static_cast<const front_end_t*>(this)->is_event_deferred(event,
                                                                        fsm);
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
       mp11::mp_for_each<initial_state_identities>(
       [this, &index](auto state_identity)
       {
           using State = typename decltype(state_identity)::type;
           m_active_state_ids[index++] = get_state_id<State>();
       });
       m_history.reset_active_state_ids(m_active_state_ids);
    }

    // Main function used internally to process events.
    // Explicitly not inline, because code size can significantly increase if
    // this method is inlined in all existing process_info variants.
    template <class Event>
    BOOST_NOINLINE process_result process_event_internal(Event const& event,
                                                         process_info info)
    {
        // If the state machine has terminate or interrupt flags, check them.
        if constexpr (mp11::mp_any_of<state_set, is_state_blocking>::value)
        {
            // If the state machine is terminated, do not handle any event.
            if (is_flag_active<TerminateFlag>())
            {
                return process_result::HANDLED_TRUE;
            }

            // If the state machine is interrupted, do not handle any event
            // unless the event is the end interrupt event.
            if (is_flag_active<InterruptedFlag>() &&
                !is_end_interrupt_event(event))
            {
                return process_result::HANDLED_TRUE;
            }
        }

        if constexpr (event_pool_member::value)
        {
            if (info != process_info::event_pool)
            {
                // If we are already processing or the event is deferred in the
                // active state configuration, process it later.
                // Skip the deferral check in submachine calls, since the
                // parent has already checked and dispatched the event.
                if (m_event_processing ||
                    (info != process_info::submachine_call &&
                     compile_policy_impl::is_event_deferred(self(), event)))
                {
                    compile_policy_impl::defer_event(self(), event, false);
                    return process_result::HANDLED_DEFERRED;
                }

                // Ensure we consider an event
                // that was action-deferred in the last sequence.
                get_event_pool().cur_seq_cnt += 1;
            }
        }
        else
        {
            BOOST_ASSERT_MSG(!m_event_processing,
                             "An event pool must be available to call "
                             "process_event while processing an event");
        }

        // Process the event.
        m_event_processing = true;
        process_result result;
#ifndef BOOST_NO_EXCEPTIONS
        if constexpr (has_no_exception_thrown<front_end_t>::value)
        {
            result = do_process_event(event, info);
        }
        else
        {
            try
            {
                result = do_process_event(event, info);
            }
            catch (std::exception& e)
            {
                // give a chance to the concrete state machine to handle
                this->exception_caught(event, get_fsm_argument(), e);
                result = process_result::HANDLED_FALSE;
            }
        }
#else
        result = do_process_event(event, info);
#endif
        m_event_processing = false;

        // After handling, look if we have more to process in the event pool
        // (but only if we're not already processing from it).
        if constexpr (event_pool_member::value)
        {
            if (info != process_info::event_pool)
            {
                process_event_pool();
            }
        }

        return result;
    }

  private:
    // Core logic for event processing without exceptions, queues, etc.
    template<class Event>
    process_result do_process_event(Event const& event, process_info info)
    {
        using dispatch_table =
            typename compile_policy_impl::template dispatch_table<derived_t,
                                                                  Event>;
        process_result result = process_result::HANDLED_FALSE;
        // Dispatch the event to every region.
        for (int region_id = 0; region_id < nr_regions; region_id++)
        {
            result |= dispatch_table::dispatch(self(), region_id, event);
        }
        // Dispatch the event to the SM-internal table if it hasn't been consumed yet.
        if (!(result & handled_true_or_deferred))
        {
            result |= dispatch_table::internal_dispatch(self(), event);
        }

        // If the event has not been handled and we have orthogonal zones, then
        // generate an error on every active state.
        // For events coming from upper machines, do not handle
        // but let the upper sm handle the error.
        if (!result && !(info == process_info::submachine_call))
        {
            for (const auto state_id: m_active_state_ids)
            {
                this->no_transition(event, get_fsm_argument(), state_id);
            }
        }
        return result;
    }

    // MSCV Bug:
    // Compile error if this class is named completion_event.
    template <typename State>
    class completion_event_occurrence : public event_occurrence
    {
        // Merge each list of transitions into a chain if needed.
        template <typename Transitions>
        struct merge_transitions_impl;
        template <typename Transition>
        struct merge_transitions_impl<mp11::mp_list<Transition>>
        {
            using type = Transition;
        };
        template <typename... Transitions>
        struct merge_transitions_impl<mp11::mp_list<Transitions...>>
        {
            using list = mp11::mp_list<Transitions...>;
            using completion_event =
                typename mp11::mp_first<list>::transition_event;
            using type =
                transition_chain<derived_t, State, list, completion_event>;
        };
        template <typename Transitions>
        using merge_transitions =
            typename merge_transitions_impl<Transitions>::type;
        using completion_transitions =
            detail::completion_transitions<derived_t, State>;
        using completion_transition = merge_transitions<completion_transitions>;

      public:
        completion_event_occurrence(int region_id)
            : event_occurrence(&try_process), m_region_id(region_id)
        {
        }

        static std::optional<process_result> try_process(event_occurrence& self, void* sm, uint16_t /*seq_cnt*/)
        {
            return static_cast<completion_event_occurrence*>(&self)
                ->try_process_impl(*reinterpret_cast<derived_t*>(sm));
        }

      private:
            std::optional<process_result> try_process_impl(derived_t& sm)
            {
                mark_for_deletion();
                return sm.template
                    process_completion_transition<completion_transition>(m_region_id);
            }

        int m_region_id;
    };

    template <typename Transition>
    process_result process_completion_transition(int region_id)
    {
        // If the state machine has terminate or interrupt flags, check them.
        if constexpr (mp11::mp_any_of<state_set, is_state_blocking>::value)
        {
            // If the state machine is interrupted or terminated, do not handle any event.
            if (is_flag_active<TerminateFlag>() || is_flag_active<InterruptedFlag>())
            {
                return process_result::HANDLED_TRUE;
            }
        }

        // Process the event.
        using completion_event = typename Transition::transition_event;
        completion_event event{};
        m_event_processing = true;
        process_result result;
#ifndef BOOST_NO_EXCEPTIONS
        if constexpr (has_no_exception_thrown<front_end_t>::value)
        {
            result = Transition::execute(self(), region_id, event);
        }
        else
        {
            try
            {
                result = Transition::execute(self(), region_id, event);
            }
            catch (std::exception& e)
            {
                // give a chance to the concrete state machine to handle
                this->exception_caught(event, get_fsm_argument(), e);
            }
        }
#else
        result = Transition::execute(self(), region_id, event);
#endif
        m_event_processing = false;
        return result;
    }

    // Core logic for event pool processing,
    // there must be at least one event in the pool.
    // Explicitly not inline, because code size can significantly increase if
    // this method's content is inlined in all entries and process_event calls.
    template <bool C = event_pool_member::value,
              typename = std::enable_if_t<C>>
    BOOST_NOINLINE size_t do_process_event_pool(size_t max_events = SIZE_MAX)
    {
        event_pool_t& event_pool = get_event_pool();
        auto it = event_pool.events.begin();
        size_t processed_events = 0;
        do
        {
            event_occurrence& event = **it;
            // The event was already processed.
            if (event.marked_for_deletion())
            {
                it = event_pool.events.erase(it);
                continue;
            }

            std::optional<process_result> result =
                event.try_process(self(), event_pool.cur_seq_cnt);
            // The event has not been dispatched.
            if (!result.has_value())
            {
                it++;
                continue;
            }

            // Consider anything except "only deferred" to be a processed event.
            if (*result != process_result::HANDLED_DEFERRED)
            {
                processed_events++;
                if (processed_events == max_events)
                {
                    break;
                }
            }

            // Start from the beginning, we might be able to process
            // events that were deferred before.
            it = event_pool.events.begin();
            // Consider newly deferred events only if
            // the event was not deferred at the same time
            // (required to prevent infinitely processing the same event,
            // if it was handled and at the same time action-deferred
            // in orthogonal regions).
            if (!(*result & process_result::HANDLED_DEFERRED))
            {
                event_pool.cur_seq_cnt += 1;
            }
        } while (it != event_pool.events.end());
        return processed_events;
    }

    template <class Event>
    void do_defer_event(const Event& event, bool next_rtc_seq)
    {
        auto& event_pool = get_event_pool();
        const uint16_t seq_cnt = next_rtc_seq ? event_pool.cur_seq_cnt
                                              : event_pool.cur_seq_cnt - 1;
        event_pool.events.push_back(processable_event::make(
            deferred_event<Event>{self(), event, seq_cnt}));
    }

    template <class Event, class Fsm>
    void preprocess_entry(Event const& event, Fsm& fsm)
    {
        m_running = true;
        m_event_processing = true;

        // Call on_entry on this SM first.
        static_cast<front_end_t*>(this)->on_entry(event, fsm);
    }

    void postprocess_entry()
    {
        m_event_processing = false;

        // After handling, look if we have more to process in the event pool.
        if constexpr (event_pool_member::value)
        {
            process_event_pool();
        }
    }

    template <typename Event>
    class state_entry_visitor
    {
      public:
        state_entry_visitor(derived_t& self, const Event& event)
            : m_self(self), m_event(event)
        {
        }

        template <typename State>
        void operator()(State& state)
        {
            state.on_entry(m_event, m_self.get_fsm_argument());
            m_self.template on_state_entry_completed<State>(m_region_id++);
        }

      private:
        derived_t& m_self;
        const Event& m_event;
        int m_region_id{};
    };


    template <class Event, class Fsm>
    void on_entry(Event const& event, Fsm& fsm)
    {
        preprocess_entry(event, fsm);

        // First set all active state ids...
        m_active_state_ids = m_history.on_entry(event);
        // ... then execute each state entry.
        state_entry_visitor<Event> visitor{self(), event};
        if constexpr (std::is_same_v<typename front_end_t::history,
                                     front::no_history>)
        {
            mp11::mp_for_each<initial_state_identities>(
                [this, &visitor](auto state_identity)
                {
                    using State = typename decltype(state_identity)::type;
                    auto& state = this->get_state<State>();
                    visitor(state);
                });
        }
        else
        {
            visit<visit_mode::active_non_recursive>(visitor);
        }

        postprocess_entry();
    }

    template <class TargetStates, class Event, class Fsm>
    void on_explicit_entry(Event const& event, Fsm& fsm)
    {
        preprocess_entry(event, fsm);

        using state_identities =
            mp11::mp_transform<mp11::mp_identity, TargetStates>;
        static constexpr bool all_regions_defined =
            mp11::mp_size<state_identities>::value == nr_regions;

        // First set all active state ids...
        if constexpr (!all_regions_defined)
        {
            m_active_state_ids = m_history.on_entry(event);
        }
        mp11::mp_for_each<state_identities>(            
            [this](auto state_identity)
            {
                using State = typename decltype(state_identity)::type;
                static constexpr int region_id = State::zone_index;
                static_assert(region_id >= 0 && region_id < nr_regions);
                m_active_state_ids[region_id] = get_state_id<State>();
            }
        );
        // ... then execute each state entry.
        state_entry_visitor<Event> visitor{self(), event};
        if constexpr (all_regions_defined)
        {
            mp11::mp_for_each<state_identities>(
                [this, &visitor](auto state_identity)
                {
                    using State = typename decltype(state_identity)::type;
                    auto& state = this->get_state<State>();
                    visitor(state);
                });
        }
        else
        {
            visit<visit_mode::active_non_recursive>(visitor);
        }

        postprocess_entry();
    }

    template <class TargetStates, class Event, class Fsm>
    void on_pseudo_entry(Event const& event, Fsm& fsm)
    {
        on_explicit_entry<TargetStates>(event, fsm);

        // Execute the second part of the compound transition.
        process_event(event);
    }

    template <typename State>
    void on_state_entry_completed(int region_id)
    {
        // Exclude composite states from completion transitions,
        // these should fire when all their regions reach a final state
        // (and final states do not exist yet).
        if constexpr(
            !is_composite<State>::value &&
            has_completion_transitions<derived_t, State>::value)
        {
            auto& event_pool = get_event_pool();
            // Process completion transitions BEFORE any other event in the
            // pool (UML Standard 2.3 15.3.14).
            event_pool.events.push_front(
                processable_event::make(
                    completion_event_occurrence<State>{region_id}));
        }
    }

    template <class Event, class Fsm>
    void on_exit(Event const& event, Fsm& fsm)
    {
        // First exit the substates.
        visit<visit_mode::active_non_recursive>(
            [this, &event](auto& state)
            {
                state.on_exit(event, get_fsm_argument());
            }
        );
        // Then call our own exit.
        (static_cast<front_end_t*>(this))->on_exit(event, fsm);
        // Give the history a chance to handle this (or not).
        m_history.on_exit(this->m_active_state_ids);
        // History decides what happens with the event pool.
        if (m_history.clear_event_pool(event))
        {
            if constexpr (event_pool_member::value)
            {
                get_event_pool().events.clear();
            }
        }
    }

    derived_t& self()
    {
        return *static_cast<derived_t*>(this);
    }

    const derived_t& self() const
    {
        return *static_cast<const derived_t*>(this);
    }

    struct optional_members :
        event_pool_member,
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

    active_state_ids_t     m_active_state_ids;
    optional_members       m_optional_members;
    history_impl           m_history{};
    bool                   m_event_processing{false};
    non_propagating<void*> m_root_sm{nullptr};
    states_t               m_states{};
    bool                   m_running{false};
};

} // boost::msm::backmp11::detail

#endif // BOOST_MSM_BACKMP11_DETAIL_STATE_MACHINE_BASE_HPP
