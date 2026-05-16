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

#include "boost/msm/back/state_machine.hpp"
#include "boost/msm/backmp11/state_machine_config.hpp"
#include "boost/msm/backmp11/favor_compile_time.hpp"
#include "boost/msm/backmp11/state_machine.hpp"
#include <boost/msm/back/queue_container_deque.hpp>
#define BOOST_PARAMETER_CAN_USE_MP11
#include <boost/parameter.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/array.hpp>
#include "Backmp11.hpp"

namespace boost::msm::backmp11
{

using back::queue_container_deque;

template <
    class A1,
    class A2,
    class A3,
    class A4
>
struct state_machine_config_adapter : state_machine_config
{
    typedef ::boost::parameter::parameters<
      ::boost::parameter::optional<
            ::boost::parameter::deduced< back::tag::history_policy>, has_history_policy< ::boost::mpl::_ >
        >
    , ::boost::parameter::optional<
            ::boost::parameter::deduced< back::tag::compile_policy>, has_compile_policy< ::boost::mpl::_ >
        >
    , ::boost::parameter::optional<
            ::boost::parameter::deduced< back::tag::fsm_check_policy>, has_fsm_check< ::boost::mpl::_ >
        >
    , ::boost::parameter::optional<
            ::boost::parameter::deduced< back::tag::queue_container_policy>,
            has_queue_container_policy< ::boost::mpl::_ >
        >
    > config_signature;

    typedef typename
        config_signature::bind<A1,A2,A3,A4>::type
        config_args;

    typedef typename ::boost::parameter::binding<
        config_args, back::tag::compile_policy, favor_runtime_speed >::type    CompilePolicy;
    using compile_policy = mp11::mp_if_c<
        std::is_same_v<CompilePolicy, favor_runtime_speed>,
        favor_runtime_speed,
        favor_compile_time>;
    

    typedef typename ::boost::parameter::binding<
        config_args, back::tag::queue_container_policy,
        queue_container_deque >::type                                    QueueContainerPolicy;
    template<typename T>
    using queue_container = typename QueueContainerPolicy::template In<T>::type;
};

template <class A0,
          class A1 = parameter::void_,
          class A2 = parameter::void_,
          class A3 = parameter::void_,
          class A4 = parameter::void_>
class state_machine_adapter
    : public state_machine<A0, state_machine_config_adapter<A1, A2, A3, A4>, state_machine_adapter<A0, A1, A2, A3, A4>>
{
    using Base = state_machine<A0, state_machine_config_adapter<A1, A2, A3, A4>, state_machine_adapter<A0, A1, A2, A3, A4>>;
  public:
    using Base::Base;

    // The new API returns a const std::array<...>&.
    const int* current_state() const
    {
        return &this->get_active_state_ids()[0];
    }

    // The history can be accessed like this,
    // but it has to be configured in the front-end.
    auto& get_history()
    {
        return this->m_history;
    }

    auto& get_message_queue()
    {
        return this->get_event_pool().events;
    }

    size_t get_message_queue_size() const
    {
        return this->get_event_pool().events.size();
    }

    void execute_queued_events()
    {
        this->process_event_pool();
    }

    void execute_single_queued_event()
    {
        this->process_event_pool(1);
    }

    auto& get_deferred_queue()
    {
        return this->get_event_pool().events;
    }

    void clear_deferred_queue()
    {
        this->get_event_pool().events.clear();
    }

    // No adapter.
    // Superseded by the visitor API.
    // void visit_current_states(...) {...}

    // No adapter.
    // States can be set with `get_state<...>() = ...` or the visitor API.
    // void set_states(...) {...}

    // No adapter.
    // Could be implemented with the visitor API.
    // auto get_state_by_id(int id) {...}
};

template <class Archive>
struct serialize_state
{
    serialize_state(Archive& ar):ar_(ar){}

    template<typename T>
    typename ::boost::enable_if<
        typename ::boost::mpl::or_<
            typename has_do_serialize<T>::type,
            typename detail::has_state_machine_tag<typename T::internal>::type
        >::type
        ,void
    >::type
    operator()(T& t) const
    {
        ar_ & t;
    }
    template<typename T>
    typename ::boost::disable_if<
        typename ::boost::mpl::or_<
            typename has_do_serialize<T>::type,
            typename detail::has_state_machine_tag<typename T::internal>::type
        >::type
        ,void
    >::type
    operator()(T&) const
    {
        // no state to serialize
    }
    Archive& ar_;
};

namespace detail
{

template <class Archive, class FrontEnd, class Config, class Derived>
void serialize(Archive& ar,
               state_machine_base<FrontEnd, Config, Derived>& sm)
{
    (serialize_state<Archive>(ar))(boost::serialization::base_object<FrontEnd>(sm));
    ar & sm.m_active_state_ids;
    ar & sm.m_history;
    ar & sm.m_event_processing;
    mp11::tuple_for_each(sm.m_states, serialize_state<Archive>(ar));
}

template<typename T, int N>
void serialize(T& ar, detail::history_impl<front::no_history, N>& history)
{
    ar & history.m_initial_state_ids;
}

template<typename T, typename... Es, int N>
void serialize(T& ar, detail::history_impl<front::shallow_history<Es...>, N>& history)
{
    ar & history.m_initial_state_ids;
    ar & history.m_last_active_state_ids;
}

}

} // boost::msm::backmp11

namespace boost::serialization
{

template <class Archive, class A0, class A1, class A2, class A3, class A4>
void serialize(Archive& ar,
               msm::backmp11::state_machine_adapter<A0, A1, A2, A3, A4>& sm,
               const unsigned int /*version*/)
{
    msm::backmp11::detail::serialize(ar, sm);
}

template<typename T, int N>
void serialize(T& ar,
               msm::backmp11::detail::history_impl<msm::front::no_history, N>& history,
               const unsigned int /*version*/)
{
    msm::backmp11::detail::serialize(ar, history);
}

template<typename T, typename... Es, int N>
void serialize(T& ar,
               msm::backmp11::detail::history_impl<msm::front::shallow_history<Es...>, N>& history,
               const unsigned int /*version*/)
{
    msm::backmp11::detail::serialize(ar, history);
}

} // boost::serialization
