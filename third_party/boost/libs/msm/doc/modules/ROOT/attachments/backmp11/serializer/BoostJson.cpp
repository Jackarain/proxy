// Copyright 2026 Christian Granzin
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include "DimSwitch.hpp"

#include <iostream>

#include <boost/core/typeinfo.hpp>

#define BOOST_JSON_NO_LIB
#include <boost/json/src.hpp>
#include <boost/msm/backmp11/serialization/boost_json.hpp>

namespace
{

// Helper for convenience:
// Convert all state ids to a human-readable JSON array
// to understand which states the ids refer to.
std::string state_names_to_boost_json_string(const DimSwitch& sm)
{
    boost::json::array json;
    sm.template visit<backmp11::visit_mode::all_states>(
        [&json](auto& state)
        {
            using State = std::decay_t<decltype(state)>;
            const auto demangled = boost::core::demangled_name(typeid(State));
            const auto short_name = demangled.substr(demangled.rfind(':') + 1);
            json.push_back(boost::json::string{short_name});
        });
    return boost::json::serialize(json);
}

std::string to_boost_json_string(const DimSwitch& dim_switch)
{
    const auto json = boost::json::value_from(dim_switch);
    return boost::json::serialize(json);
}

[[maybe_unused]] void boost_json_example()
{
    DimSwitch dim_switch;

    // Prints:
    // ["Off","On"]
    std::cout << state_names_to_boost_json_string(dim_switch) << std::endl;
    
    // The initial state is Off (state id 0).
    dim_switch.start();
    // Prints:
    // {"front_end":{"brightness":0},"states":{"1":{"times_pressed":0}},"active_state_ids":[0],"machine_state":1}
    std::cout << to_boost_json_string(dim_switch) << std::endl;

    // Turn On (state id 1) and set brightness to 75.
    dim_switch.process_event(TurnOn{});
    dim_switch.process_event(Dim{75});
    // Prints:
    // {"front_end":{"brightness":75},"states":{"1":{"times_pressed":1}},"active_state_ids":[1],"machine_state":1}
    std::cout << to_boost_json_string(dim_switch) << std::endl;

    // Deserialize the json into a new state machine.
    const auto json = boost::json::parse(to_boost_json_string(dim_switch));
    const auto dim_switch_2 = boost::json::value_to<DimSwitch>(json);

    // Prints:
    // {"front_end":{"brightness":75},"states":{"1":{"times_pressed":1}},"active_state_ids":[1],"machine_state":1}
    std::cout << to_boost_json_string(dim_switch_2) << std::endl;
}

} // namespace
