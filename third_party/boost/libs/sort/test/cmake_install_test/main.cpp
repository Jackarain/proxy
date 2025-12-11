#include <boost/sort/spreadsort/integer_sort.hpp>

int main()
{
    int v[] = { 1, 2, 3, 0 };
    boost::sort::spreadsort::integer_sort( v + 0, v + 4 );
    return v[ 0 ];
}
