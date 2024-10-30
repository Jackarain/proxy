//
// simple_proxy.cpp
// ~~~~~~~~~~~~~~~~
// A standalone SOCKS proxy server implementation.
//

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include "proxy/proxy_server.hpp"

namespace net = boost::asio;
namespace websocket = boost::beast::websocket;
namespace http = boost::beast::http;
using tcp = net::ip::tcp;
using namespace proxy;

struct proxy_config {
    std::string host = "127.0.0.1";
    unsigned short port = 10800;
    std::string username = "default_user";
    std::string password = "default_pass";
};

proxy_config load_proxy_config(const std::string& config_file = "proxy.conf") {
    proxy_config config;
    std::ifstream conf(config_file);
    if (!conf) {
        std::cerr << "Config file not found. Using default values.\n";
        return config;
    }

    std::string line;
    while (std::getline(conf, line)) {
        std::istringstream iss(line);
        std::string key, value;
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            if (key == "host") config.host = value;
            else if (key == "port") config.port = static_cast<unsigned short>(std::stoi(value));
            else if (key == "username") config.username = value;
            else if (key == "password") config.password = value;
        }
    }
    return config;
}

net::awaitable<void> start_proxy(net::io_context& ioc, std::shared_ptr<proxy_server>& server, const proxy_config& config) {
    tcp::endpoint listen_endpoint(net::ip::address::from_string(config.host), config.port);

    proxy_server_option opt;
    opt.auth_users_.emplace_back(config.username, config.password);
    opt.listens_.emplace_back(listen_endpoint, false);

    server = proxy_server::make(ioc.get_executor(), opt);
    server->start();

    std::cout << "Proxy server started on " << config.host << ":" << config.port << "\n";
    co_return;
}

int main()
{
    net::io_context ioc(1); // Single-threaded IO context
    proxy_config config = load_proxy_config();
    std::shared_ptr<proxy_server> server;

    // Spawn the proxy server coroutine
    net::co_spawn(ioc, start_proxy(ioc, server, config), net::detached);

    ioc.run(); // Run the IO context to start the server
    return 0;
}
