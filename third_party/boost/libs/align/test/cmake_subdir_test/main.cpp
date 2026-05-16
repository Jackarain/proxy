// Copyright 2026 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/align.hpp>

int main()
{
    void* p = 0;
    (void)p;
    return boost::alignment::is_aligned( &p, boost::alignment::alignment_of<void*>::value )? 0: 1;
}
