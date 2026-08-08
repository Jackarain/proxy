// Copyright 2026 Christian Granzin
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

#include "DimSwitch.hpp"

// Include headers for a simple text archive format.
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/msm/backmp11/serialization/boost_serialization.hpp>

// Add a serialize free function to support Boost.Serialization for DimSwitch.
// We can integrate Boost.Serialization with this mechanism or by
// adding a serialize member function to the state machine.
namespace boost::serialization
{

template <typename Archive>
void serialize(Archive& archive, DimSwitch& state_machine,
               const unsigned int /*version*/)
{
    backmp11::reflect(
        state_machine,
        backmp11::serialization::boost_serialization_serializer<Archive>{
            archive});
}

} // namespace boost::serialization

namespace
{

[[maybe_unused]] void boost_serialization_example()
{
    DimSwitch dim_switch;

    // Do something with the state machine.
    dim_switch.start();
    dim_switch.process_event(TurnOn{});
    dim_switch.process_event(Dim{75});

    // Serialize the state machine.
    std::ostringstream ostream;
    boost::archive::text_oarchive oarchive{ostream};
    oarchive << dim_switch;

    // Deserialize the archive into a new state machine.
    std::istringstream istream{ostream.str()};
    boost::archive::text_iarchive iarchive{istream};
    DimSwitch dim_switch_2;
    iarchive >> dim_switch_2;

    // We have the same state machine as before.
    std::cout << "dim_switch_2.brightness == 75 : "
              << (dim_switch_2.brightness == 75) << std::endl;
}

} // namespace
