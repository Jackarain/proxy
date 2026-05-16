// Copyright 2023 Peter Dimov
// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <cstddef>

void f1();
void f2();

int main()
{
    f1();
    f2();
}

void test_odr_use( int const* )
{
}

void test_odr_use ( std::size_t const* )
{
}
