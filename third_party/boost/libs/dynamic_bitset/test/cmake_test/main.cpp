#include <boost/dynamic_bitset.hpp>

int main()
{
    const boost::dynamic_bitset<> db{3, 4};

    return db[2] ? 0 : 1;
}
