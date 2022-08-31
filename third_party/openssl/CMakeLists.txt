cmake_minimum_required(VERSION 3.0)

project(openssl)

option(SHARED_OPENSSL "build shared openssl lib" OFF)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(crypto EXCLUDE_FROM_ALL)
add_subdirectory(ssl EXCLUDE_FROM_ALL)

set(OPENSSL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)

set(OPENSSL_LIBRARIES crypto ssl PARENT_SCOPE)
set(OPENSSL_VERSION "1.0.1j" PARENT_SCOPE)
set(OPENSSL_FOUND TRUE PARENT_SCOPE)

set(LIB_EAY_LIBRARY crypto PARENT_SCOPE)
set(SSL_EAY_LIBRARY ssl PARENT_SCOPE)

set(OPENSSL_SSL_LIBRARY ssl PARENT_SCOPE)
set(OPENSSL_CRYPTO_LIBRARY crypto PARENT_SCOPE)
