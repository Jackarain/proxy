This directory contains an experimental implementation of `basic_static_cstring`, which differs from `basic_static_string` in the following ways:

|                     | `basic_static_cstring` | `basic_static_string` |
|---------------------|------------------------|-----------------------|
| Layout              | `sizeof == N + 1`      | Has size member       |
| Embedded NULs       | Not supported          | Supported             |
| Trivially copyable  | Yes                    | No                    |

Additionally, when `N <= UCHAR_MAX`, `basic_static_cstring` employs an optimization that avoids calling `std::strlen()` to compute the size.

This work stems from [boostorg/static_string#23](https://github.com/boostorg/static_string/issues/23).

If you believe `basic_static_cstring` should become part of the public API, please share your feedback on the Boost mailing list.