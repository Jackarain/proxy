# Copyright 2018, 2019 Peter Dimov
# Copyright 2025 James E. King III
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

include(BoostTestJamfile OPTIONAL RESULT_VARIABLE HAVE_BOOST_TEST)

if(HAVE_BOOST_TEST)

boost_test_jamfile(FILE Jamfile.v2 LINK_LIBRARIES Boost::numeric_interval Boost::core Boost::logic)

endif()
