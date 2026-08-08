# Copyright  (c) 2016-2025 Antony Polukhin
# Copyright  (c) 2025-2026 Fedor Osetrov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...4.0)

project(lexical_cast_subdir_test LANGUAGES CXX)

set(deps

# Library dependencies
config
container
core
throw_exception

# Test dependencies
algorithm
array
align
assert
atomic
bind
compat
concept_check
container_hash
conversion
date_time
describe
detail
exception
filesystem
function
function_types
functional
fusion
integer
intrusive
io
iterator
move
mp11
mpl
numeric/conversion
optional
predef
preprocessor
range
regex
scope
smart_ptr
static_assert
system
test
tokenizer
tuple
type_index
type_traits
typeof
unordered
utility
variant
variant2
winapi
)

foreach(dep IN LISTS deps)
    add_subdirectory(../../../${dep} boostorg/${dep})
endforeach()

enable_testing()
add_subdirectory(../../ boostorg/lexical_cast)

if (BOOST_USE_MODULES)
    add_executable(boost_lexical_cast_module_usage ../modules/usage_sample.cpp)
    target_link_libraries(boost_lexical_cast_module_usage PRIVATE Boost::lexical_cast)
    add_test(NAME boost_lexical_cast_module_usage COMMAND boost_lexical_cast_module_usage)
endif()

