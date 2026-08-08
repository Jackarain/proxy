// Copyright 2026 Christian Granzin
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MSM_NONSTANDALONE_TEST
#define BOOST_TEST_MODULE backmp11_serialization_test
#endif
#include <boost/test/unit_test.hpp>

#include "attachments/backmp11/serializer/BoostSerialization.cpp"
#include "attachments/backmp11/serializer/BoostJson.cpp"

#ifdef BOOST_MSM_TEST_NLOHMANN_JSON
#include "attachments/backmp11/serializer/NlohmannJson.cpp"
#endif

using namespace boost::msm;

namespace
{

BOOST_AUTO_TEST_CASE(boost_serialization)
{
    DimSwitch dim_switch;

    // The initial state is Off.
    dim_switch.start();
    BOOST_REQUIRE(dim_switch.is_state_active<Off>());

    // Turn On and set brightness to 75.
    dim_switch.process_event(TurnOn{});
    BOOST_REQUIRE(dim_switch.is_state_active<On>());
    BOOST_REQUIRE(dim_switch.get_state<On>().times_pressed == 1);
    dim_switch.process_event(Dim{75});
    BOOST_REQUIRE(dim_switch.brightness = 75);

    // Serialize the state machine.
    std::ostringstream ostream;
    boost::archive::text_oarchive oarchive{ostream};
    oarchive << dim_switch;

    // Deserialize the archive into a new state machine.
    std::istringstream istream{ostream.str()};
    boost::archive::text_iarchive iarchive{istream};
    DimSwitch dim_switch_2;
    iarchive >> dim_switch_2;

    // We have the same state as before.
    BOOST_REQUIRE(dim_switch_2.is_state_active<On>());
    BOOST_REQUIRE(dim_switch.get_state<On>().times_pressed == 1);
    BOOST_REQUIRE(dim_switch_2.brightness = 75);
}

BOOST_AUTO_TEST_CASE(boost_json)
{
    DimSwitch dim_switch;

    // Convert all state ids to a human-readable JSON array
    // to understand which states the ids refer to.
    auto state_names_json_string = state_names_to_boost_json_string(dim_switch);
    BOOST_REQUIRE(state_names_json_string == R"(["Off","On"])");

    // The initial state is Off (state id 0).
    dim_switch.start();
    BOOST_REQUIRE(dim_switch.is_state_active<Off>());

    BOOST_REQUIRE(to_boost_json_string(dim_switch) == \
R"({"front_end":{"brightness":0},"states":{"1":{"times_pressed":0}},"active_state_ids":[0],"machine_state":1})");

    // Turn On (state id 1) and set brightness to 75.
    dim_switch.process_event(TurnOn{});
    BOOST_REQUIRE(dim_switch.is_state_active<On>());
    BOOST_REQUIRE(dim_switch.get_state<On>().times_pressed == 1);
    dim_switch.process_event(Dim{75});
    BOOST_REQUIRE(dim_switch.brightness = 75);

    BOOST_REQUIRE(to_boost_json_string(dim_switch) == \
R"({"front_end":{"brightness":75},"states":{"1":{"times_pressed":1}},"active_state_ids":[1],"machine_state":1})");

    // Deserialize the json into a new state machine.
    const auto json = boost::json::parse(to_boost_json_string(dim_switch));
    // const auto json = boost::json::value_from(dim_switch);
    auto dim_switch_2 = boost::json::value_to<DimSwitch>(json);

    // We have the same state as before.
    BOOST_REQUIRE(dim_switch_2.is_state_active<On>());
    BOOST_REQUIRE(dim_switch.get_state<On>().times_pressed == 1);
    BOOST_REQUIRE(dim_switch_2.brightness = 75);
}

#ifdef BOOST_MSM_TEST_NLOHMANN_JSON

BOOST_AUTO_TEST_CASE(nlohmann_json)
{
    DimSwitch dim_switch;

    auto state_names_json = state_names_to_nlohmann_json_string(dim_switch);
    BOOST_REQUIRE(state_names_json == \
R"([
    "Off",
    "On"
])");

    // The initial state is Off (state id 0).
    dim_switch.start();
    BOOST_REQUIRE(dim_switch.is_state_active<Off>());

    BOOST_REQUIRE(to_nlohmann_json_string(dim_switch) == \
R"({
    "active_state_ids": [
        0
    ],
    "front_end": {
        "brightness": 0
    },
    "machine_state": 1,
    "states": {
        "1": {
            "times_pressed": 0
        }
    }
})");

    // Turn On (state id 1) and set brightness to 75.
    dim_switch.process_event(TurnOn{});
    BOOST_REQUIRE(dim_switch.is_state_active<On>());
    BOOST_REQUIRE(dim_switch.get_state<On>().times_pressed == 1);
    dim_switch.process_event(Dim{75});
    BOOST_REQUIRE(dim_switch.brightness = 75);

    BOOST_REQUIRE(to_nlohmann_json_string(dim_switch) == \
R"({
    "active_state_ids": [
        1
    ],
    "front_end": {
        "brightness": 75
    },
    "machine_state": 1,
    "states": {
        "1": {
            "times_pressed": 1
        }
    }
})");
    
    // Deserialize the json into a new state machine.
    const nlohmann::json json =
        nlohmann::json::parse(to_nlohmann_json_string(dim_switch));
    auto dim_switch_2 = json.get<DimSwitch>();

    // We have the same state as before.
    BOOST_REQUIRE(dim_switch_2.is_state_active<On>());
    BOOST_REQUIRE(dim_switch.get_state<On>().times_pressed == 1);
    BOOST_REQUIRE(dim_switch_2.brightness = 75);
}

#endif // BOOST_MSM_TEST_NLOHMANN_JSON

} // namespace
