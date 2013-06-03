#include <iostream>
#include "socks_server.hpp"

int main(int argc, char **argv)
{
	if (argc  != 2)
	{
		std::cerr << "usage: " << argv[0] << " <port>" << std::endl;
		return -1;
	}

	try {

		boost::asio::io_service io_service;

		socks::socks_server s(io_service, atoi(argv[1]));

		io_service.run();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}
