#
# Copyright (c) 2024 Dirk Stolle
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
foreach(_name
  path_spec)
  set(_test t_core_io_${_name})
  set(_target test_core_io_${_name})

  add_executable(${_target} "")
  target_sources(${_target} PRIVATE ${_name}.cpp)
  target_link_libraries(${_target}
    PRIVATE
      gil_compile_options
      gil_include_directories
      gil_dependencies)
  add_test(NAME ${_test} COMMAND ${_target})

  unset(_name)
  unset(_target)
  unset(_test)
endforeach()
