# Copyright 2018, 2019, 2021, 2022 Peter Dimov
# Copyright 2025 Joaquin M Lopez Munoz
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

include(BoostTestJamfile OPTIONAL RESULT_VARIABLE HAVE_BOOST_TEST)

if(HAVE_BOOST_TEST)

boost_test_jamfile(FILE Jamfile.v2
  LINK_LIBRARIES Boost::bloom Boost::config Boost::core Boost::mp11)

endif()