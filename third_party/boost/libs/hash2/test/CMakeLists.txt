# Copyright 2018, 2019, 2021, 2022 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

include(BoostTestJamfile OPTIONAL RESULT_VARIABLE HAVE_BOOST_TEST)

if(HAVE_BOOST_TEST)

boost_test_jamfile(FILE Jamfile
  LINK_LIBRARIES Boost::hash2 Boost::core Boost::array Boost::unordered Boost::utility Boost::endian)

endif()
