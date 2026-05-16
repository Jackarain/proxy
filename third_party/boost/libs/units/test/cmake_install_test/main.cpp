//  Copyright 2026 Peter Dimov
//  Distributed under the Boost Software License, Version 1.0.
//  http://www.boost.org/LICENSE_1_0.txt

#include <boost/units/systems/si.hpp>
#include <boost/units/systems/si/io.hpp>
#include <iostream>

using namespace boost::units;
using namespace boost::units::si;

int main()
{
    quantity<force> F = 2.0 * newton;
    quantity<length> dx = 2.0 * meter;
    quantity<energy> E = F * dx;

    std::cout

        << "F  = " << F << std::endl
        << "dx = " << dx << std::endl
        << "E  = " << E << std::endl;
}
