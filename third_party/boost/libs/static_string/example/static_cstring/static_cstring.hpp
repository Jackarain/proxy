//
// Copyright (c) 2025 Gennaro Prota (gennaro dot prota at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/static_string
//

#ifndef BOOST_STATIC_STRING_STATIC_CSTRING_HPP
#define BOOST_STATIC_STRING_STATIC_CSTRING_HPP

#include <boost/static_string/config.hpp>
#include <algorithm>
#include <climits>
#include <compare>
#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>
#include <stdexcept>
#include <type_traits>

namespace boost {
namespace static_strings {

namespace detail {

// Primary template: No remaining-capacity trick; uses traits::length() for length.
template<std::size_t N, typename CharT, typename Traits, bool UseRemaining>
class static_cstring_base
{
public:
    using traits_type = Traits;
    using value_type = CharT;
    using size_type = std::size_t;

    value_type data_[N + 1]{};

    constexpr size_type get_size() const noexcept
    {
        return traits_type::length(data_);
    }

    constexpr void set_size(size_type sz) noexcept
    {
        data_[sz] = value_type{};
    }
};

// Specialization for N <= UCHAR_MAX: Uses remaining-capacity trick.
template<std::size_t N, typename CharT, typename Traits>
class static_cstring_base<N, CharT, Traits, true>
{
public:
    using traits_type = Traits;
    using value_type = CharT;
    using size_type = std::size_t;

    value_type data_[N + 1]{};

    constexpr size_type get_size() const noexcept
    {
        return N - static_cast<unsigned char>(data_[N]);
    }

    constexpr void set_size(size_type sz) noexcept
    {
        data_[sz] = value_type{};
        data_[N] = static_cast<value_type>(N - sz);
    }
};

} // namespace detail

template<std::size_t N, typename CharT = char, typename Traits = std::char_traits<CharT>>
class basic_static_cstring
    : public detail::static_cstring_base<N, CharT, Traits, (N <= UCHAR_MAX)>
{
public:
    using base = detail::static_cstring_base<N, CharT, Traits, (N <= UCHAR_MAX)>;
    using base::data_;
    using base::get_size;
    using base::set_size;

    // Member types
    using traits_type     = Traits;
    using value_type      = CharT;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference       = value_type&;
    using const_reference = const value_type&;
    using pointer         = value_type*;
    using const_pointer   = const value_type*;
    using iterator        = pointer;
    using const_iterator  = const_pointer;

    static constexpr size_type npos = static_cast<size_type>(-1);
    static constexpr size_type static_capacity = N;

    // Constructors.
    constexpr basic_static_cstring() noexcept
    {
        set_size(0);
    }

    constexpr basic_static_cstring(const CharT* s)
    {
        assign(s);
    }

    constexpr basic_static_cstring(const CharT* s, size_type count)
    {
        assign(s, count);
    }

    constexpr basic_static_cstring(size_type count, CharT ch)
    {
        assign(count, ch);
    }

    template<std::size_t M>
    constexpr basic_static_cstring(const CharT (&arr)[M])
    {
        static_assert(M <= N + 1, "Array too big for static_cstring");
        assign(arr);
    }

    constexpr size_type size() const noexcept
    {
        return get_size();
    }

    constexpr size_type length() const noexcept
    {
        return size();
    }

    constexpr bool empty() const noexcept
    {
        return data_[0] == value_type{};
    }

    static constexpr size_type max_size() noexcept
    {
        return N;
    }

    static constexpr size_type capacity() noexcept
    {
        return N;
    }

    // Element access.
    constexpr reference operator[](size_type pos) noexcept
    {
        return data_[pos];
    }

    constexpr const_reference operator[](size_type pos) const noexcept
    {
        return data_[pos];
    }

    constexpr reference at(size_type pos)
    {
        if (pos >= size())
        {
            throw std::out_of_range("static_cstring::at");
        }
        return data_[pos];
    }

    constexpr const_reference at(size_type pos) const
    {
        if (pos >= size())
        {
            throw std::out_of_range("static_cstring::at");
        }
        return data_[pos];
    }

    constexpr reference front() noexcept
    {
        BOOST_STATIC_STRING_ASSERT(!empty());
        return data_[0];
    }

    constexpr const_reference front() const noexcept
    {
        BOOST_STATIC_STRING_ASSERT(!empty());
        return data_[0];
    }

    constexpr reference back() noexcept
    {
        BOOST_STATIC_STRING_ASSERT(!empty());
        return data_[size() - 1];
    }

    constexpr const_reference back() const noexcept
    {
        BOOST_STATIC_STRING_ASSERT(!empty());
        return data_[size() - 1];
    }

    constexpr pointer data() noexcept
    {
        return data_;
    }

    constexpr const_pointer data() const noexcept
    {
        return data_;
    }

    constexpr const_pointer c_str() const noexcept
    {
        return data_;
    }

    // Iterators.
    constexpr iterator begin() noexcept
    {
        return data_;
    }

    constexpr const_iterator begin() const noexcept
    {
        return data_;
    }

    constexpr const_iterator cbegin() const noexcept
    {
        return data_;
    }

    constexpr iterator end() noexcept
    {
        return data_ + size();
    }

    constexpr const_iterator end() const noexcept
    {
        return data_ + size();
    }

    constexpr const_iterator cend() const noexcept
    {
        return data_ + size();
    }

    // Modifiers.
    constexpr void clear() noexcept
    {
        set_size(0);
    }

    constexpr basic_static_cstring& assign(const CharT* s)
    {
        return assign(s, traits_type::length(s));
    }

    constexpr basic_static_cstring& assign(const CharT* s, size_type count)
    {
        if (count > N)
        {
            throw std::length_error("static_cstring::assign");
        }
        traits_type::copy(data_, s, count);
        set_size(count);
        return *this;
    }

    constexpr basic_static_cstring& assign(size_type count, CharT ch)
    {
        if (count > N)
        {
            throw std::length_error("static_cstring::assign");
        }
        traits_type::assign(data_, count, ch);
        set_size(count);
        return *this;
    }

    constexpr basic_static_cstring& operator=(const CharT* s)
    {
        return assign(s);
    }

    constexpr void push_back(CharT ch)
    {
        const size_type sz = size();
        if (sz >= N)
        {
            throw std::length_error("static_cstring::push_back");
        }
        data_[sz] = ch;
        set_size(sz + 1);
    }

    constexpr void pop_back() noexcept
    {
        BOOST_STATIC_STRING_ASSERT(!empty());
        set_size(size() - 1);
    }

    constexpr basic_static_cstring& append(const CharT* s)
    {
        return append(s, traits_type::length(s));
    }

    constexpr basic_static_cstring& append(const CharT* s, size_type count)
    {
        const size_type sz = size();
        if (sz + count > N)
        {
            throw std::length_error("static_cstring::append");
        }
        traits_type::copy(data_ + sz, s, count);
        set_size(sz + count);
        return *this;
    }

    constexpr basic_static_cstring& append(size_type count, CharT ch)
    {
        const size_type sz = size();
        if (sz + count > N)
        {
            throw std::length_error("static_cstring::append");
        }
        traits_type::assign(data_ + sz, count, ch);
        set_size(sz + count);
        return *this;
    }

    constexpr basic_static_cstring& operator+=(const CharT* s)
    {
        return append(s);
    }

    constexpr basic_static_cstring& operator+=(CharT ch)
    {
        push_back(ch);
        return *this;
    }

    // Comparisons.
    constexpr int compare(const basic_static_cstring& other) const noexcept
    {
        const size_type lhs_sz = size();
        const size_type rhs_sz = other.size();
        const int result = traits_type::compare(data_, other.data_, (std::min)(lhs_sz, rhs_sz));

        return result != 0
                 ? result
                 : lhs_sz < rhs_sz
                 ? -1
                 : lhs_sz > rhs_sz
                 ? 1
                 : 0;
    }

    constexpr int compare(const CharT* s) const noexcept
    {
        const size_type lhs_sz = size();
        const size_type rhs_sz = traits_type::length(s);
        const int result = traits_type::compare(data_, s, (std::min)(lhs_sz, rhs_sz));

        return result != 0
                 ? result
                 : lhs_sz < rhs_sz
                 ? -1
                 : lhs_sz > rhs_sz
                 ? 1
                 : 0;
    }

    // Conversions.
    constexpr operator std::basic_string_view<CharT, Traits>() const noexcept
    {
        return {data_, size()};
    }

    std::basic_string<CharT, Traits> str() const
    {
        return {data_, size()};
    }

    // Swap.
    constexpr void swap(basic_static_cstring& other) noexcept
    {
        basic_static_cstring tmp = *this;
        *this = other;
        other = tmp;
    }

    constexpr bool operator==(const basic_static_cstring& other) const noexcept
    {
        const size_type sz = size();
        return sz == other.size()
            && traits_type::compare(data_, other.data_, sz) == 0;
    }

    constexpr std::strong_ordering
    operator<=>(const basic_static_cstring& other) const noexcept
    {
        return compare(other) <=> 0;
    }
};

#if defined(BOOST_STATIC_STRING_USE_DEDUCT)

// Deduction guide.
template<std::size_t N, typename CharT>
basic_static_cstring(const CharT(&)[N]) -> basic_static_cstring<N - 1, CharT>;

#endif

// Comparison with const CharT*.
template<std::size_t N, typename CharT, typename Traits>
constexpr bool operator==(const basic_static_cstring<N, CharT, Traits>& lhs,
                          const CharT* rhs) noexcept
{
    return lhs.compare(rhs) == 0;
}

template<std::size_t N, typename CharT, typename Traits>
constexpr bool operator==(const CharT* lhs,
                          const basic_static_cstring<N, CharT, Traits>& rhs) noexcept
{
    return rhs.compare(lhs) == 0;
}

// Stream output.
template<std::size_t N, typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,
                                              const basic_static_cstring<N, CharT, Traits>& str)
{
    return os << str.c_str();
}

// Alias templates.
template<std::size_t N>
using static_cstring = basic_static_cstring<N, char>;

template<std::size_t N>
using static_wcstring = basic_static_cstring<N, wchar_t>;

}
}

#endif
