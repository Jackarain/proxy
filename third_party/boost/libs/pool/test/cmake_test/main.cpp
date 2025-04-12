// this is the example from "Pool Interfaces" in the docs

#include <boost/pool/pool.hpp>

int main()
{
    boost::pool<> p(sizeof(int));
    for (int i = 0; i < 10000; ++i)
    {
        void * const t = p.malloc();
        // do something with it
        (t);
    }

    return 0;
}
