// Copyright 2025 René Ferdinand Rivera Morel
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.txt or copy at
// https://www.bfgroup.xyz/b2/LICENSE.txt)

// tag::source[]
#include <contracts>
#include <iostream>

int f(const int x) pre(x != 0) post(r : r != x)
{
	return x + 1;
}

int main()
{
	std::cout << "Hello!\n";
	f(1);
	f(0);
}
// end::source[]
