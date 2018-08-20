#include <iostream>
#include "socks_client.hpp"

#include <boost/smart_ptr.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;


using boost::asio::ip::tcp;

class client
{
public:
	client(boost::asio::io_service& io_service,
		const std::string& server, const std::string& path, const std::string& socks_server)
		: resolver_(io_service)
		, socket_(io_service)
		, server_(server)
	{
		// Form the request. We specify the "Connection: close" header so that the
		// server will close the socket after transmitting the response. This will
		// allow us to treat all data up until the EOF as the content.
		std::ostream request_stream(&request_);
		request_stream << "GET " << path << " HTTP/1.0\r\n";
		request_stream << "Host: " << server << "\r\n";
		request_stream << "Accept: */*\r\n";
		request_stream << "User-Agent: curl/7.60.0\r\n";
		request_stream << "Connection: close\r\n\r\n";

		// Start an asynchronous resolve to translate the server and service names
		// into a list of endpoints.
		bool ret = socks::parse_url(socks_server, socks_);
		if (!ret)
			return;

		tcp::resolver::query query(socks_.host, socks_.port);
		resolver_.async_resolve(query,
			boost::bind(&client::handle_resolve, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::iterator));
	}

private:
	void handle_resolve(const boost::system::error_code& err,
		tcp::resolver::iterator endpoint_iterator)
	{
		if (!err)
		{
			// Attempt a connection to each endpoint in the list until we
			// successfully establish a connection.
			boost::asio::async_connect(socket_, endpoint_iterator,
				boost::bind(&client::handle_connect, this,
					boost::asio::placeholders::error));
		}
		else
		{
			std::cout << "Error: " << err.message() << "\n";
		}
	}

	void handle_connect(const boost::system::error_code& err)
	{
		if (!err)
		{
			auto socks_cli = boost::make_shared<socks::socks_client>(socket_);

			socks_cli->async_do_proxy(socks_, server_, "80",
				[this, socks_cli](const boost::system::error_code& err)
				{
					if (!err)
					{
						std::cout << "* SOCKS PROXY SUCCESS!\n";

						// The connection was successful. Send the request.
						boost::asio::async_write(socket_, request_,
							boost::bind(&client::handle_write_request, this,
								boost::asio::placeholders::error));
					}
					else
					{
						std::cout << "Error: " << err.message() << "\n";
					}
				});
		}
		else
		{
			std::cout << "Error: " << err.message() << "\n";
		}
	}

	void handle_write_request(const boost::system::error_code& err)
	{
		if (!err)
		{
			// Read the response status line. The response_ streambuf will
			// automatically grow to accommodate the entire line. The growth may be
			// limited by passing a maximum size to the streambuf constructor.
			boost::asio::async_read_until(socket_, response_, "\r\n",
				boost::bind(&client::handle_read_status_line, this,
					boost::asio::placeholders::error));
		}
		else
		{
			std::cout << "Error: " << err.message() << "\n";
		}
	}

	void handle_read_status_line(const boost::system::error_code& err)
	{
		if (!err)
		{
			// Check that response is OK.
			std::istream response_stream(&response_);
			std::string http_version;
			response_stream >> http_version;
			unsigned int status_code;
			response_stream >> status_code;
			std::string status_message;
			std::getline(response_stream, status_message);
			if (!response_stream || http_version.substr(0, 5) != "HTTP/")
			{
				std::cout << "Invalid response\n";
				return;
			}
			if (status_code != 200)
			{
				std::cout << "Response returned with status code ";
				std::cout << status_code << "\n";
				return;
			}

			// Read the response headers, which are terminated by a blank line.
			boost::asio::async_read_until(socket_, response_, "\r\n\r\n",
				boost::bind(&client::handle_read_headers, this,
					boost::asio::placeholders::error));
		}
		else
		{
			std::cout << "Error: " << err << "\n";
		}
	}

	void handle_read_headers(const boost::system::error_code& err)
	{
		if (!err)
		{
			// Process the response headers.
			std::istream response_stream(&response_);
			std::string header;
			while (std::getline(response_stream, header) && header != "\r")
				std::cout << header << "\n";
			std::cout << "\n";

			// Write whatever content we already have to output.
			if (response_.size() > 0)
				std::cout << &response_;

			// Start reading remaining data until EOF.
			boost::asio::async_read(socket_, response_,
				boost::asio::transfer_at_least(1),
				boost::bind(&client::handle_read_content, this,
					boost::asio::placeholders::error));
		}
		else
		{
			std::cout << "Error: " << err << "\n";
		}
	}

	void handle_read_content(const boost::system::error_code& err)
	{
		if (!err)
		{
			// Write all of the data that has been read so far.
			std::cout << &response_;

			// Continue reading remaining data until EOF.
			boost::asio::async_read(socket_, response_,
				boost::asio::transfer_at_least(1),
				boost::bind(&client::handle_read_content, this,
					boost::asio::placeholders::error));
		}
		else if (err != boost::asio::error::eof)
		{
			std::cout << "Error: " << err << "\n";
		}
	}

	tcp::resolver resolver_;
	tcp::socket socket_;
	socks::socks_address socks_;
	std::string server_;
	boost::asio::streambuf request_;
	boost::asio::streambuf response_;
};


int main(int argc, char **argv)
{
	try {
		std::string socks_server;
		std::string http_server;
		std::string http_path;

		po::options_description desc("Options");
		desc.add_options()
			("help,h", "Help message.")
			("version", "Current version.")
			("socks", po::value<std::string>(&socks_server)->default_value("socks5://127.0.0.1:1080"), "Socks server.")
			("target", po::value<std::string>(&http_server)->default_value("www.boost.org"), "HTTP server.")
			("path", po::value<std::string>(&http_path)->default_value("/LICENSE_1_0.txt"), "HTTP path.")
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
		client c(io_service, http_server, http_path, socks_server);
		io_service.run();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}
