//  Copyright (c) 2020-2025 Andrey Semashev
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_ATOMIC_TEST_ATOMIC_WRAPPER_HPP_INCLUDED_
#define BOOST_ATOMIC_TEST_ATOMIC_WRAPPER_HPP_INCLUDED_

#include <boost/atomic/atomic.hpp>
#include <boost/atomic/atomic_ref.hpp>
#include <boost/atomic/ipc_atomic.hpp>
#include <boost/atomic/ipc_atomic_ref.hpp>
#include "aligned_object.hpp"

//! Wrapper type for atomic template
template< typename T >
struct atomic_wrapper
{
    using atomic_type = boost::atomic< T >;
    using atomic_reference_type = atomic_type&;

    atomic_type a;

    atomic_wrapper() = default;
    explicit atomic_wrapper(T const& value) : a(value) {}
};

//! Wrapper type for atomic_ref template
template< typename T >
struct atomic_ref_wrapper
{
    using atomic_type = boost::atomic_ref< T >;
    using atomic_reference_type = atomic_type const&;

    aligned_object< T, atomic_type::required_alignment > object;
    const atomic_type a;

    atomic_ref_wrapper() : a(object.get()) {}
    explicit atomic_ref_wrapper(T const& value) : object(value), a(object.get()) {}
};

//! Wrapper type for ipc_atomic template
template< typename T >
struct ipc_atomic_wrapper
{
    using atomic_type = boost::ipc_atomic< T >;
    using atomic_reference_type = atomic_type&;

    atomic_type a;

    ipc_atomic_wrapper() = default;
    explicit ipc_atomic_wrapper(T const& value) : a(value) {}
};

//! Wrapper type for atomic_ref template
template< typename T >
struct ipc_atomic_ref_wrapper
{
    using atomic_type = boost::ipc_atomic_ref< T >;
    using atomic_reference_type = atomic_type const&;

    aligned_object< T, atomic_type::required_alignment > object;
    const atomic_type a;

    ipc_atomic_ref_wrapper() : a(object.get()) {}
    explicit ipc_atomic_ref_wrapper(T const& value) : object(value), a(object.get()) {}
};

#endif // BOOST_ATOMIC_TEST_ATOMIC_WRAPPER_HPP_INCLUDED_
