/*
 *             Copyright Andrey Semashev 2025.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */

// The test verifies that the compiler supports unions with non-trivial default constructors.

class test_class
{
public:
    test_class() {}
};

union test_union
{
    unsigned char as_bytes[sizeof(test_class)];
    test_class as_class;

    test_union() {}
};

int main(int, char*[])
{
    test_union tu;
    tu.as_class = test_class();

    return 0;
}
