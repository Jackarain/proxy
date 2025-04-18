// Copyright Antony Polukhin, 2013-2025.

// Distributed under the Boost Software License, Version 1.0.
// (See the accompanying file LICENSE_1_0.txt
// or a copy at <http://www.boost.org/LICENSE_1_0.txt>.)

#include <boost/config.hpp>
#ifdef BOOST_MSVC
# pragma warning(disable: 4512) // generic_stringize.cpp(37) : warning C4512: 'stringize_functor' : assignment operator could not be generated
#endif

//[lexical_cast_stringize
/*`
    In this example we'll make a `stringize` method that accepts a sequence, converts
    each element of the sequence into string and appends that string to the result.

    Example is based on the example from the [@http://www.packtpub.com/boost-cplusplus-application-development-cookbook/book Boost C++ Application Development Cookbook]
    by Antony Polukhin, ISBN 9781849514880. Russian translation: [@https://dmkpress.com/catalog/computer/programming/c/978-5-97060-868-5/ ISBN: 9785970608685].

    Step 1: Making a functor that converts any type to a string and remembers result:
*/

#include <boost/lexical_cast.hpp>

struct stringize_functor {
private:
    std::string& result;

public:
    explicit stringize_functor(std::string& res)
        : result(res)
    {}

    template <class T>
    void operator()(const T& v) const {
        result += boost::lexical_cast<std::string>(v);
    }
};

//` Step 2: Applying `stringize_functor` to each element in sequence:
#include <boost/fusion/include/for_each.hpp>
template <class Sequence>
std::string stringize(const Sequence& seq) {
    std::string result;
    boost::fusion::for_each(seq, stringize_functor(result));
    return result;
}

//` Step 3: Using the `stringize` with different types:
#include <boost/fusion/adapted/std_tuple.hpp>
#include <boost/fusion/adapted/std_pair.hpp>

int main() {
    std::tuple<char, int, char, int> decim('-', 10, 'e', 5);
    if (stringize(decim) != "-10e5") {
        return 1;
    }

    std::pair<int, std::string> value_and_type(270, "Kelvin");
    if (stringize(value_and_type) != "270Kelvin") {
        return 2;
    }

    return 0;
}

//] [/lexical_cast_stringize]



