#include <iostream>
#include "socks_server.hpp"

int main(int argc, char **argv)
{
	boost::asio::io_service io_service;

	socks::socks_server s(io_service, 6543);

	io_service.run();
	return 0;
}
