//  Copyright (c) 2020-2025 Andrey Semashev
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_ATOMIC_TESTS_ALIGNED_OBJECT_HPP_INCLUDED_
#define BOOST_ATOMIC_TESTS_ALIGNED_OBJECT_HPP_INCLUDED_

#include <cstddef>
#include <cstdint>
#include <new>

//! A wrapper that creates an object that has at least the specified alignment
template< typename T, std::size_t Alignment >
class aligned_object
{
private:
    T* m_p;
    unsigned char m_storage[Alignment + sizeof(T)];

public:
    aligned_object()
    {
        m_p = new (get_aligned_storage()) T;
    }

    explicit aligned_object(T const& value)
    {
        m_p = new (get_aligned_storage()) T(value);
    }

    aligned_object(aligned_object const&) = delete;
    aligned_object& operator= (aligned_object const&) = delete;

    ~aligned_object() noexcept
    {
        m_p->~T();
    }

    T& get() const noexcept
    {
        return *m_p;
    }

private:
    unsigned char* get_aligned_storage()
    {
#if defined(UINTPTR_MAX)
        using uintptr_type = std::uintptr_t;
#else
        using uintptr_type = std::size_t;
#endif
        unsigned char* p = m_storage;
        uintptr_type misalignment = ((uintptr_type)p) & (Alignment - 1u);
        if (misalignment > 0)
            p += Alignment - misalignment;
        return p;
    }
};

#endif // BOOST_ATOMIC_TESTS_ALIGNED_OBJECT_HPP_INCLUDED_
