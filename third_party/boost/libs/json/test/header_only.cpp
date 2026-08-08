#include <boost/json/src.hpp>

void
with_array()
{
    boost::json::value jv;
    jv.emplace_array();
}

void
with_object()
{
    boost::json::value jv;
    jv.emplace_object();
}

void
with_string()
{
    boost::json::value jv;
    jv.emplace_string();
}

int main(int, char **)
{
    // it's important these are separate functions, otherwise the warnings this
    // file is supposed to produce do not manifest
    with_array();
    with_object();
    with_string();
    return 0;
}
