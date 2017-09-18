#include <iostream>
#include "socks_server.hpp"

#include <boost/program_options.hpp>
namespace po = boost::program_options;


int main(int argc, char **argv)
{
	try {
		unsigned short port;
		std::string address;

		po::options_description desc("Options");
		desc.add_options()
			("help,h", "Help message.")
			("version", "Current version.")
			("port", po::value<unsigned short>(&port)->default_value(1080), "Socks porxy port.")
			("address", po::value<std::string>(&address)->default_value("127.0.0.1"), "Socks listen bind address.")
		;

		// 解析命令行.
		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

		// 帮助输出.
		if (vm.count("help") || argc == 1)
		{
			std::cout << desc;
			return 0;
		}

		boost::asio::io_service io_service;
		socks::socks_server s(io_service, port, address);

		io_service.run();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}
