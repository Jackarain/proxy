//  Copyright 2026 Peter Dimov
//  Distributed under the Boost Software License, Version 1.0.
//  http://www.boost.org/LICENSE_1_0.txt

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/moment.hpp>
#include <iostream>

using namespace boost::accumulators;

int main()
{
    // Define an accumulator set for calculating the mean and the
    // 2nd moment ...
    accumulator_set<double, stats<tag::mean, tag::moment<2> > > acc;

    // push in some data ...
    acc(1.2);
    acc(2.3);
    acc(3.4);
    acc(4.5);

    // Display the results ...
    std::cout << "Mean:   " << mean(acc) << std::endl;
    std::cout << "Moment: " << moment<2>(acc) << std::endl;
}
