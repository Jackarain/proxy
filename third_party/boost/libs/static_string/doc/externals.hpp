#pragma once

namespace boost {

/// !EXTERNAL!
///
/// @see https://www.boost.org/doc/libs/release/libs/utility/doc/html/utility/utilities/string_view.html
template <class T, class Traits>
struct basic_string_view {};

} // namespace boost

namespace std {

/// !EXTERNAL!
///
/// @see https://en.cppreference.com/w/cpp/iterator/reverse_iterator
template <class T>
struct reverse_iterator {};

/// !EXTERNAL!
///
/// @see https://en.cppreference.com/w/cpp/types/size_t
struct size_t {};

/// !EXTERNAL!
///
/// @see https://en.cppreference.com/w/cpp/types/integer
struct uint64_t {};

/// !EXTERNAL!
///
/// @see https://en.cppreference.com/w/cpp/types/integer
struct int64_t {};

/// !EXTERNAL!
///
/// @see https://en.cppreference.com/w/cpp/types/nullptr_t
struct nullptr_t {};

/// !EXTERNAL!
///
/// @see https://en.cppreference.com/w/cpp/types/ptrdiff_t
struct ptrdiff_t {};

/// !EXTERNAL!
///
/// @see https://en.cppreference.com/w/cpp/utility/initializer_list
template <class T>
struct initializer_list {};

/// !EXTERNAL!
///
/// @see https://en.cppreference.com/w/cpp/io/basic_ostream
template <class T, class Traits>
struct basic_ostream {};

/// !EXTERNAL!
///
/// @see https://en.cppreference.com/w/cpp/types/numeric_limits
template <class T>
struct numeric_limits {};

} // namespace std
