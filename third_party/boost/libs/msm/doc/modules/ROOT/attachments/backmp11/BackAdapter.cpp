// Copyright 2026 Christian Granzin
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
#include "boost/msm/backmp11/state_machine.hpp"
#define BOOST_PARAMETER_CAN_USE_MP11
#include <boost/parameter.hpp>
#include <boost/serialization/array.hpp>

namespace
{

// Serializer for Boost.Serialization.
// Replicates the serialization implementation of `back::state_machine`.
template <typename Archive>
class back_serializer
{
  public:
    back_serializer(Archive& archive) : m_archive(archive) {}

    template <typename FrontEnd>
    void visit_front_end(FrontEnd&& front_end)
    {
        if constexpr (has_do_serialize<std::decay_t<FrontEnd>>::value)
        {
            m_archive & front_end;
        }
    }

    template <typename FrontEnd, typename Reflect>
    void visit_front_end(FrontEnd&& /*front_end*/, Reflect&& reflect)
    {
        reflect();
    }

    template <typename Member>
    void visit_member(const char* /*key*/, Member&& member)
    {
        m_archive & member;
    }

    template <typename State>
    void visit_state(size_t /*state_id*/, State&& state)
    {
        if constexpr (has_do_serialize<std::decay_t<State>>::value)
        {
            m_archive & state;
        }
    }

    template <typename State, typename Reflect>
    void visit_state(size_t /*state_id*/, State&& /*state*/, Reflect&& reflect)
    {
        reflect();
    }

  private:
    Archive& m_archive;
};

// Config adapter for `back::state_machine`.
// The adapter only contains the compile policy and the queue container policy,
// the other template arguments are not supported.
template <typename CompilePolicy, typename QueueContainerPolicy>
struct back_config_adapter : boost::msm::backmp11::state_machine_config
{
    using compile_policy = boost::mp11::mp_if_c<
        std::is_same_v<CompilePolicy,
                       boost::msm::backmp11::favor_runtime_speed>,
        boost::msm::backmp11::favor_runtime_speed,
        boost::msm::backmp11::favor_compile_time>;

    template <typename T>
    using event_pool_container =
        typename QueueContainerPolicy::template In<T>::type;
    using event_pool = boost::msm::backmp11::event_pool<event_pool_container>;
};

template <class A1, class A2, class A3, class A4>
struct get_back_config_adapter_impl
{
    template <typename... Spec>
    using parameters = boost::parameter::parameters<Spec...>;
    template <typename Tag,
              typename Predicate = ::boost::parameter::aux::use_default>
    using optional = boost::parameter::optional<Tag, Predicate>;

    using config_signature = boost::parameter::parameters<
        boost::parameter::optional<
            boost::parameter::deduced<boost::msm::back::tag::history_policy>,
            has_history_policy<boost::mpl::_>>,
        boost::parameter::optional<
            boost::parameter::deduced<boost::msm::back::tag::compile_policy>,
            has_compile_policy<boost::mpl::_>>,
        boost::parameter::optional<
            boost::parameter::deduced<boost::msm::back::tag::fsm_check_policy>,
            has_fsm_check<boost::mpl::_>>,
        boost::parameter::optional<
            boost::parameter::deduced<
                boost::msm::back::tag::queue_container_policy>,
            has_queue_container_policy<boost::mpl::_>>>;

    using config_args = typename config_signature::bind<A1, A2, A3, A4>::type;

    using CompilePolicy = typename boost::parameter::binding<
        config_args, boost::msm::back::tag::compile_policy,
        boost::msm::backmp11::favor_runtime_speed>::type;

    using QueueContainerPolicy = typename boost::parameter::binding<
        config_args, boost::msm::back::tag::queue_container_policy,
        boost::msm::back::queue_container_deque>::type;

    using type =
        back_config_adapter<CompilePolicy, QueueContainerPolicy>;
};
template <class A1, class A2, class A3, class A4>
using get_back_config_adapter =
    typename get_back_config_adapter_impl<A1, A2, A3, A4>::type;

// State machine adapter for a `back::state_machine`.
// Provides a `backmp11::state_machine` implementation adapted to
// replicate the API of `back::state_machine`.
template <class A0,
          class A1 = boost::parameter::void_,
          class A2 = boost::parameter::void_,
          class A3 = boost::parameter::void_,
          class A4 = boost::parameter::void_>
class back_adapter : public boost::msm::backmp11::state_machine<
                         A0, get_back_config_adapter<A1, A2, A3, A4>,
                         back_adapter<A0, A1, A2, A3, A4>>
{
    using Base = boost::msm::backmp11::state_machine<
        A0, get_back_config_adapter<A1, A2, A3, A4>,
        back_adapter<A0, A1, A2, A3, A4>>;

  public:
    using Base::Base;

    void start()
    {
        try
        {
            Base::start();
        }
        catch (std::exception& e)
        {
            this->exception_caught(boost::msm::backmp11::starting{}, *this, e);
        }
    }

    void stop()
    {
        try
        {
            Base::stop();
        }
        catch (std::exception& e)
        {
            this->exception_caught(boost::msm::backmp11::stopping{}, *this, e);
        }
    }

    template <typename Event>
    boost::msm::back::HandledEnum process_event(const Event& event)
    {
        if (this->template is_flag_active<boost::msm::InterruptedFlag>() &&
            !this->is_end_interrupt_event(event))
        {
            return boost::msm::back::HANDLED_TRUE;
        }

        if (this->get_machine_state() ==
            boost::msm::backmp11::machine_state::processing)
        {
            this->enqueue_event(event);
            return boost::msm::back::HANDLED_DEFERRED;
        }
        else
        {
            try
            {
                return static_cast<boost::msm::back::HandledEnum>(
                    Base::process_event(event));
            }
            catch (std::exception& e)
            {
                this->exception_caught(event, *this, e);
                return boost::msm::back::HANDLED_FALSE;
            }
        }
    }

    // The new API returns a const std::array<...>&.
    const uint16_t* current_state() const
    {
        return &this->get_active_state_ids()[0];
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

    template <class Archive>
    void serialize(Archive& ar, const unsigned int /*version*/)
    {
        boost::msm::backmp11::reflect(*this, back_serializer<Archive>{ar});
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

} // namespace
