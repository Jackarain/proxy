// Copyright 2026 Christian Granzin
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include "DimSwitch.hpp"

#include <iostream>

#include <boost/core/typeinfo.hpp>

// nlohmann/json.
#include <boost/msm/backmp11/serialization/nlohmann_json.hpp>

namespace
{

// Helper for convenience:
// Convert all state ids to a human-readable JSON array
// to understand which states the ids refer to.
std::string state_names_to_nlohmann_json_string(const DimSwitch& sm)
{
    nlohmann::json json;
    sm.template visit<backmp11::visit_mode::all_states>(
        [&sm, &json](auto& state)
        {
            using State = std::decay_t<decltype(state)>;
            const auto demangled = boost::core::demangled_name(typeid(State));
            const auto short_name = demangled.substr(demangled.rfind(':') + 1);
            json[sm.template get_state_id<State>()] = short_name;
        });
    return json.dump(4);
}

std::string to_nlohmann_json_string(const DimSwitch& dim_switch)
{
    const nlohmann::json json = dim_switch;
    return json.dump(4);
}

[[maybe_unused]] void nlohmann_json_example()
{
    DimSwitch dim_switch;

    // Prints:
    // [
    //     "Off",
    //     "On"
    // ]
    std::cout << state_names_to_nlohmann_json_string(dim_switch) << std::endl;
    
    // The initial state is Off (state id 0).
    dim_switch.start();
    // Prints:
    // {
    //     "active_state_ids": [
    //         0
    //     ],
    //     "front_end": {
    //         "brightness": 0
    //     },
    //     "machine_state": 1,
    //     "states": {
    //         "1": {
    //             "times_pressed": 0
    //         }
    //     }
    // }
    std::cout << to_nlohmann_json_string(dim_switch) << std::endl;

    // Turn On (state id 1) and set brightness to 75.
    dim_switch.process_event(TurnOn{});
    dim_switch.process_event(Dim{75});
    // Prints:
    // {
    //     "active_state_ids": [
    //         1
    //     ],
    //     "front_end": {
    //         "brightness": 75
    //     },
    //     "machine_state": 1,
    //     "states": {
    //         "1": {
    //             "times_pressed": 1
    //         }
    //     }
    // }
    std::cout << to_nlohmann_json_string(dim_switch) << std::endl;
    // Deserialize the json into a new state machine.
    const nlohmann::json json =
        nlohmann::json::parse(to_nlohmann_json_string(dim_switch));
    auto dim_switch_2 = json.get<DimSwitch>();

    // Prints:
    // {
    //     "active_state_ids": [
    //         1
    //     ],
    //     "front_end": {
    //         "brightness": 75
    //     },
    //     "machine_state": 1,
    //     "states": {
    //         "1": {
    //             "times_pressed": 1
    //         }
    //     }
    // }
    std::cout << to_nlohmann_json_string(dim_switch_2) << std::endl;
}

} // namespace
