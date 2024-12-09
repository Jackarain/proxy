// data_flow.cpp
// ~~~~~~~~~~~~~~~~~
// A SOCKS data flow lib

#ifdef _WIN32
    #define EXPORT_SYMBOL __declspec(dllexport)
#else
    #define EXPORT_SYMBOL __attribute__((visibility("default")))
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/core.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <tuple>
#include <map>
#include <algorithm>
#include <cctype>
#include <string>
#include "proxy/proxy_server.hpp"
#include "proxy/socks_client.hpp"
#include "proxy/strutil.hpp"
#include "proxy/logging.hpp"


namespace net = boost::asio;
namespace websocket = boost::beast::websocket;
namespace http = boost::beast::http;
using tcp = net::ip::tcp;
using namespace proxy;

struct flow_config {
    std::string host = "127.0.0.1";
    unsigned short port = 10800;
    std::string username = "default_user";
    std::string password = "default_pass";
    std::string manager_endpoint = "127.0.0.1:8080";
    std::string websocket_test_url = "example.com";
    std::size_t buffer_size = 4096;
    std::chrono::seconds reconnect_delay = std::chrono::seconds(2);
};

// HELPERS
// Helper function to trim whitespace from a string
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

// Load config from input params string
flow_config parse_config_from_string(const std::string& config_data) {
    flow_config config;
    std::istringstream iss(config_data);
    std::string line;

    while (std::getline(iss, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        std::istringstream line_stream(line);
        std::string key, value;
        if (std::getline(line_stream, key, '=') && std::getline(line_stream, value)) {
            key = trim(key); // Trim key
            value = trim(value); // Trim value
            if (key == "host") config.host = value;
            else if (key == "port") config.port = static_cast<unsigned short>(std::stoi(value));
            else if (key == "username") config.username = value;
            else if (key == "password") config.password = value;
            else if (key == "manager_endpoint") config.manager_endpoint = value;
            else if (key == "websocket_test_url") config.websocket_test_url = value;
            else if (key == "buffer_size") config.buffer_size = static_cast<std::size_t>(std::stoi(value));
            else if (key == "reconnect_delay") config.reconnect_delay = std::chrono::seconds(std::stoi(value));
        }
    }

    return config;
}

// Load configuration from file or default
flow_config load_flow_config(const std::string& config_file = "flow.conf") {
    flow_config config;
    std::ifstream conf(config_file);
    if (!conf) {
        std::cerr << "(load_flow_config) Config file not found. Using default values.\n";
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
            else if (key == "manager_endpoint") config.manager_endpoint = value;
            else if (key == "websocket_test_url") config.websocket_test_url = value;
            else if (key == "buffer_size") config.buffer_size = static_cast<std::size_t>(std::stoi(value));
            else if (key == "reconnect_delay") config.reconnect_delay = std::chrono::seconds(std::stoi(value));
        }
    }
    return config;
}

// Utility function to determine file extension from Content-Type
std::string determine_file_extension(const std::string& content_type) {
    if (content_type.find("pdf") != std::string::npos) return ".pdf";
    if (content_type.find("mp3") != std::string::npos) return ".mp3";
    if (content_type.find("mp4") != std::string::npos) return ".mp4";
    if (content_type.find("flac") != std::string::npos) return ".flac";
    if (content_type.find("json") != std::string::npos) return ".json";
    if (content_type.find("html") != std::string::npos) return ".html";
    if (content_type.find("xml") != std::string::npos) return ".xml";
    if (content_type.find("jpeg") != std::string::npos || content_type.find("jpg") != std::string::npos) return ".jpeg";
    if (content_type.find("png") != std::string::npos) return ".png";
    if (content_type.find("gif") != std::string::npos) return ".gif";
    if (content_type.find("plain") != std::string::npos) return ".txt";
    if (content_type.find("csv") != std::string::npos) return ".csv";
    if (content_type.find("mpeg") != std::string::npos) return ".mpeg";
    if (content_type.find("ogg") != std::string::npos) return ".ogg";
    if (content_type.find("wav") != std::string::npos) return ".wav";
    if (content_type.find("webm") != std::string::npos) return ".webm";
    if (content_type.find("avi") != std::string::npos) return ".avi";
    return ".bin";  // Default for unknown binary types
}

void handle_response_content(const std::string& content_type, const boost::beast::multi_buffer& body) {
    std::string extension = determine_file_extension(content_type);
    if (content_type.find("text") != std::string::npos || content_type.find("json") != std::string::npos || content_type.find("xml") != std::string::npos) {
        std::cout << "(handle_response_content) Text response: \n" << boost::beast::buffers_to_string(body.data()) << "\n";
    } else {
        std::ofstream out_file("output" + extension, std::ios::binary);
        out_file << boost::beast::buffers_to_string(body.data());
        std::cout << "(handle_response_content) Binary content saved as output" + extension + "\n";
    }
}

std::tuple<std::string, std::string, std::string, std::string> parse_url(const std::string& url) {
    // Regex to parse URLs with optional scheme
    std::regex url_regex(R"(^(?:(https?)://)?([^/:]+)(?::(\d+))?(.*)$)");
    std::smatch match;
    if (std::regex_match(url, match, url_regex)) {
        std::string scheme = match[1].str();
        std::string host = match[2].str();
        std::string port = match[3].str();
        std::string path = match[4].str();
        if (scheme.empty()) scheme = "http";
        if (path.empty() || path[0] != '/') path = "/" + path;
        if (port.empty()) {
            port = (scheme == "https") ? "443" : "80";
        }

        return {scheme, host, port, path};
    } else {
        throw std::runtime_error("(parse_url) Invalid URL format: " + url);
    }
}

// MAIN FNS
// Function to start the data flow service
net::awaitable<void> start_data_flow(net::io_context& ioc, std::shared_ptr<proxy_server>& flow, const flow_config& config) {
    xlogger::toggle_write_logging(false);
    tcp::endpoint listen_endpoint(net::ip::address::from_string(config.host), config.port);

    proxy_server_option opt;
    opt.auth_users_.emplace_back(config.username, config.password);
    opt.listens_.emplace_back(listen_endpoint, false);

    flow = proxy_server::make(ioc.get_executor(), opt);
    flow->start();

    std::cout << "(start_data_flow) Data flow service started on " << config.host << ":" << config.port << "\n";
    co_return;
}

net::awaitable<void> forward_websocket_to_socks(
    websocket::stream<tcp::socket>& ws, 
    tcp::socket& sock, 
    const flow_config& config) 
{
    boost::beast::flat_buffer buffer;
    boost::system::error_code ec;

    while (true) {
        std::size_t bytes_read = co_await ws.async_read_some(buffer.prepare(config.buffer_size), net_awaitable[ec]);
        if (ec) {
            if (ec == boost::asio::error::eof || ec.message() == "end of stream") {
                std::cout << "(forward_websocket_to_socks) EOF from WebSocket; signaling close.\n";
                ec.clear();
            } else {
                std::cerr << "(forward_websocket_to_socks) WebSocket read error: " << ec.message() << "\n";
            }
            break;
        }
        buffer.commit(bytes_read);
        std::cout << "(forward_websocket_to_socks) WebSocket -> SOCKS | Data size: " << bytes_read << "\n";

        if (buffer.size() > 0) {
            std::size_t bytes_written = co_await boost::asio::async_write(sock, buffer.data(), net_awaitable[ec]);
            if (ec) {
                std::cerr << "(forward_websocket_to_socks) SOCKS write error: " << ec.message() << "\n";
                break;
            }
            buffer.consume(bytes_written);
        }
    }

    if (sock.is_open()) {
        sock.shutdown(tcp::socket::shutdown_both, ec);
        if (ec) {
            std::cerr << "(forward_websocket_to_socks) SOCKS shutdown error: " << ec.message() << "\n";
        } else {
            std::cout << "(forward_websocket_to_socks) SOCKS shutdown signaled.\n";
        }
        
        sock.close(ec);
        if (ec) {
            std::cerr << "(forward_websocket_to_socks) Error closing SOCKS connection: " << ec.message() << "\n";
        } else {
            std::cout << "(forward_websocket_to_socks) SOCKS connection closed.\n";
        }
    }
    
    if (ws.is_open()) {
        co_await ws.async_close(websocket::close_code::normal, net::use_awaitable);
        std::cout << "(forward_socks_to_websocket) WebSocket closed gracefully.\n";
    }
    co_return;
}

net::awaitable<void> forward_socks_to_websocket(
    tcp::socket& sock, 
    websocket::stream<tcp::socket>& ws, 
    const flow_config& config) 
{
    boost::beast::flat_buffer buffer;
    boost::system::error_code ec;

    while (true) {
        std::size_t bytes_read = co_await sock.async_read_some(buffer.prepare(config.buffer_size), net_awaitable[ec]);
        if (ec) {
            if (ec == boost::asio::error::eof || ec.message() == "end of stream") {
                std::cout << "(forward_socks_to_websocket) EOF from SOCKS; signaling close.\n";
                ec.clear();
            } else {
                std::cerr << "(forward_socks_to_websocket) SOCKS read error: " << ec.message() << "\n";
            }
            break;
        }
        buffer.commit(bytes_read);
        std::cout << "(forward_socks_to_websocket) SOCKS -> WebSocket | Data size: " << bytes_read << "\n";

        if (buffer.size() > 0) {
            std::size_t bytes_written = co_await ws.async_write(buffer.data(), net_awaitable[ec]);
            if (ec) {
                std::cerr << "(forward_socks_to_websocket) WebSocket write error: " << ec.message() << "\n";
                break;
            }
            buffer.consume(bytes_written);
        }
    }
    
    if (ws.is_open()) {
        co_await ws.async_close(websocket::close_code::normal, net::use_awaitable);
        std::cout << "(forward_socks_to_websocket) WebSocket closed gracefully.\n";
    }

    if (sock.is_open()) {
        sock.shutdown(tcp::socket::shutdown_both, ec);
        if (ec) {
            std::cerr << "(forward_socks_to_websocket) SOCKS shutdown error: " << ec.message() << "\n";
        } else {
            std::cout << "(forward_socks_to_websocket) SOCKS shutdown signaled.\n";
        }
        
        sock.close(ec);
        if (ec) {
            std::cerr << "(forward_socks_to_websocket) Error closing SOCKS connection: " << ec.message() << "\n";
        } else {
            std::cout << "(forward_socks_to_websocket) SOCKS connection closed.\n";
        }
    }
    co_return;
}

net::awaitable<void> handle_data_flow_request(
    net::io_context& ioc,
    websocket::stream<tcp::socket>& ws,
    const flow_config& config)
{
    auto executor = ioc.get_executor();
    tcp::socket sock(executor);
    boost::beast::flat_buffer buffer;
    boost::system::error_code ec;

    // Set control callback to handle pings
    // ws.control_callback(
    // [&](websocket::frame_type kind, boost::beast::string_view payload) {
    //     if (kind == websocket::frame_type::ping) {
    //         std::cout << "(handle_data_flow_request) Ping received, responding with pong.\n";
    //         boost::system::error_code pong_ec;
    //         ws.pong(websocket::ping_data(payload), pong_ec);
    //         if (pong_ec) {
    //             std::cerr << "(handle_data_flow_request) Error sending pong: " << pong_ec.message() << "\n";
    //         }
    //     }
    // });

    // Connect to the SOCKS proxy server
    tcp::endpoint proxy_endpoint(net::ip::address::from_string(config.host), config.port);

    co_await sock.async_connect(proxy_endpoint, net_awaitable[ec]);
    if (ec) {
        std::cerr << "(handle_data_flow_request) Failed to connect to SOCKS proxy server: " << ec.message() << "\n";
        co_return;
    }

    // Read HTTP request from WebSocket
    co_await ws.async_read(buffer, net_awaitable[ec]);
    if (ec) {
        std::cerr << "(handle_data_flow_request) WebSocket read error: " << ec.message() << "\n";
        co_return;
    }
    // Parse HTTP request
    http::request_parser<http::dynamic_body> req_parser;
    req_parser.eager(true);
    req_parser.put(buffer.data(), ec);
    buffer.consume(buffer.size());
    if (ec) {
        std::cerr << "(handle_data_flow_request) Error parsing request: " << ec.message() << "\n";
        co_return;
    }
    auto req = req_parser.release();
    std::cout << "(handle_data_flow_request) Raw request:\n" << req << "\n";

    // Extract target info using parse_url
    std::string scheme, target_host, target_port_str, path;
    std::tie(scheme, target_host, target_port_str, path) = parse_url(std::string(req.target()));
    unsigned short target_port = static_cast<unsigned short>(std::stoi(target_port_str));

    // Perform SOCKS handshake with the target
    proxy::socks_client_option opt;
    opt.target_host = target_host;
    opt.target_port = target_port;
    opt.proxy_hostname = true;
    opt.username = config.username;
    opt.password = config.password;
    co_await proxy::async_socks_handshake(sock, opt, net_awaitable[ec]);
    if (ec) {
        std::cerr << "(handle_data_flow_request) SOCKS handshake failed: " << ec.message() << "\n";
        co_return;
    }
    std::cout << "(handle_data_flow_request) SOCKS handshake successful to " << target_host << ":" << target_port << "\n";

    if (scheme == "https" || req.method() == http::verb::connect) {
        // Send 200 OK response to the client to confirm tunnel setup
        std::string connect_success_response = "HTTP/1.1 200 OK\r\n\r\n";
        co_await ws.async_write(net::buffer(connect_success_response), net_awaitable[ec]);
        if (ec) {
            std::cerr << "(handle_data_flow_request) WebSocket write error for CONNECT success: " << ec.message() << "\n";
            co_return;
        }
        std::cout << "(handle_data_flow_request) Sent 200 OK response for CONNECT to client.\n";

        auto websocket_to_socks = forward_websocket_to_socks(ws, sock, config);
        auto socks_to_websocket = forward_socks_to_websocket(sock, ws, config);

        // Wait for both coroutines to complete
        co_await (boost::asio::experimental::awaitable_operators::operator&&(
            std::move(websocket_to_socks),
            std::move(socks_to_websocket)
        ));

    } else if (scheme == "http") {
        std::cout << "(handle_data_flow_request) Sending request:\n" << req << "\n";
        co_await http::async_write(sock, req, net_awaitable[ec]);

        // Read the HTTP response from the target
        boost::beast::flat_buffer response_buffer;
        http::response<http::dynamic_body> response;
        co_await http::async_read(sock, response_buffer, response, net_awaitable[ec]);
        std::cout << "(handle_data_flow_request) Raw response:\n" << response << "\n";
        if (ec) {
            std::cerr << "(handle_data_flow_request) SOCKS read error: " << ec.message() << "\n";
            co_return;
        }

        std::ostringstream response_stream;
        response_stream << response;
        std::string response_str = response_stream.str();
        std::cout << "(handle_data_flow_request) Serialized response size before WebSocket send: " << response_str.size() << "\n";

        // Send the HTTP response over WebSocket
        co_await ws.async_write(net::buffer(response_str), net_awaitable[ec]);
        if (ec) {
            std::cerr << "(handle_data_flow_request) WebSocket write error when forwarding response: " << ec.message() << "\n";
            co_return;
        }
        std::cout << "(handle_data_flow_request) Complete response forwarded to WebSocket.\n";

        response_buffer.consume(response_buffer.size());
        buffer.consume(buffer.size());
    } else {
        std::cerr << "(handle_data_flow_request) Unsupported scheme: " << scheme << "\n";
        co_return;
    }

    // Close the SOCKS connection
    if (sock.is_open()) {
        sock.shutdown(boost::asio::socket_base::shutdown_both, ec);
        if (ec) {
            std::cerr << "(handle_data_flow_request) Socket shutdown error: " << ec.message() << "\n";
        } else {
            std::cout << "(handle_data_flow_request) Socket shutdown successful.\n";
        }

        sock.close(ec);
        if (ec) {
            std::cerr << "(handle_data_flow_request) Error closing SOCKS connection: " << ec.message() << "\n";
        } else {
            std::cout << "(handle_data_flow_request) SOCKS connection closed.\n";
        }
    }
}

// WebSocket client to forward requests and receive responses
net::awaitable<void> run_websocket_with_data_flow(net::io_context& ioc, const flow_config& config) {
    std::shared_ptr<proxy_server> flow_service;
    auto executor = co_await net::this_coro::executor;
    tcp::resolver resolver(executor);
    net::co_spawn(executor, start_data_flow(ioc, flow_service, config), net::detached);

    while (true) {
        try {
            websocket::stream<tcp::socket> ws(executor);

            // Resolve and connect to the manager endpoint
            auto const results = co_await resolver.async_resolve(
                config.manager_endpoint.substr(0, config.manager_endpoint.find(":")),
                config.manager_endpoint.substr(config.manager_endpoint.find(":") + 1),
                net::use_awaitable);
            co_await net::async_connect(ws.next_layer(), results);

            // Auth
            std::string auth_str = config.username + ":" + config.password;
            std::string auth_header = "Basic " + strutil::base64_encode(auth_str);
            ws.set_option(websocket::stream_base::decorator(
                [auth_header](websocket::request_type& req) {
                    req.set(http::field::authorization, auth_header);
                }
            ));

            // Perform WebSocket handshake
            co_await ws.async_handshake(
                config.manager_endpoint.substr(0, config.manager_endpoint.find(":")), "/", net::use_awaitable);
            std::cout << "(run_websocket_with_data_flow) WebSocket connection established with manager at " << config.manager_endpoint << "\n";

            // Start data flow request handler
            ws.binary(true);
            co_await handle_data_flow_request(ioc, ws, config);
            if (ws.is_open()) {
                co_await ws.async_close(websocket::close_code::normal, net::use_awaitable);
                std::cout << "(run_websocket_with_data_flow) WebSocket connection closed.\n";
            }
        } catch (const boost::system::system_error& se) {
            std::cerr << "(run_websocket_with_data_flow) WebSocket error: " << se.what() << "\n";
        } catch (const std::exception& e) {
            std::cerr << "(run_websocket_with_data_flow) Reconnection attempt due to error: " << e.what() << "\n";
        }

        // Delay before attempting to reconnect
        co_await net::steady_timer(ioc, config.reconnect_delay).async_wait(net::use_awaitable);
    }

    // Close the data flow service when the WebSocket connection ends
    if (flow_service) {
        flow_service->close();
        std::cout << "(run_websocket_with_data_flow) Data flow service stopped after WebSocket connection ended\n";
    }

    co_return;
}

// Test WebSocket server to simulate the manager with auth validation
net::awaitable<void> websocket_test_server(net::io_context& ioc, const flow_config& config) {
    try {
        std::string host = config.manager_endpoint.substr(0, config.manager_endpoint.find(":"));
        std::string port_str = config.manager_endpoint.substr(config.manager_endpoint.find(":") + 1);
        unsigned short port = static_cast<unsigned short>(std::stoi(port_str));
        boost::beast::flat_buffer buffer;
        boost::system::error_code ec;
        
        tcp::acceptor acceptor(ioc, tcp::endpoint(net::ip::make_address(host), port));
        std::cout << "(websocket_test_server) Test WebSocket server running at " << host << ":" << port << "\n";

        while (true) {
            tcp::socket socket = co_await acceptor.async_accept(net_awaitable[ec]);
            if (ec) {
                std::cerr << "(websocket_test_server) Accept error: " << ec.message() << "\n";
                continue;
            }
            websocket::stream<tcp::socket> ws(std::move(socket));

            http::request<http::string_body> req;
            co_await http::async_read(ws.next_layer(), buffer, req, net_awaitable[ec]);
            buffer.consume(buffer.size());
            if (ec) {
                std::cerr << "(websocket_test_server) Error reading upgrade request: " << ec.message() << "\n";
                continue;
            }

            auto auth_header = req[http::field::authorization];
            if (auth_header.empty() || auth_header != "Basic " + strutil::base64_encode(config.username + ":" + config.password)) {
                std::cerr << "(websocket_test_server) Authentication failed.\n";
                http::response<http::string_body> res{http::status::unauthorized, req.version()};
                res.set(http::field::content_type, "text/plain");
                res.body() = "(websocket_test_server) Unauthorized - Invalid Credentials";
                res.prepare_payload();
                co_await http::async_write(ws.next_layer(), res, net_awaitable[ec]);
                ws.next_layer().shutdown(tcp::socket::shutdown_both, ec);
                continue;
            }
            std::cout << "(websocket_test_server) Authentication successful. Client authorized.\n";
            co_await ws.async_accept(req, net_awaitable[ec]);
            if (ec) {
                std::cerr << "(websocket_test_server) WebSocket accept error: " << ec.message() << "\n";
                continue;
            }
            ws.binary(true);

            std::string scheme, target_host, target_port_str, path;
            std::tie(scheme, target_host, target_port_str, path) = parse_url(config.websocket_test_url);
            if (path.empty()) path = "/";

            std::ostringstream req_oss;
            req_oss << "GET " << scheme << "://" << target_host;
            if ((scheme == "http" && target_port_str != "80") || (scheme == "https" && target_port_str != "443")) {
                req_oss << ":" << target_port_str;
            }
            req_oss << path << " HTTP/1.1\r\n";
            req_oss << "Host: " << target_host << "\r\n";
            req_oss << "User-Agent: TestWebSocketServer/1.0\r\n";
            req_oss << "Accept: */*\r\n";
            req_oss << "\r\n";
            std::string request_str = req_oss.str();
            
            std::cout << "(websocket_test_server) Sending GET request:\n" << request_str << "\n";
            co_await ws.async_write(net::buffer(request_str), net_awaitable[ec]);
            if (ec) {
                std::cerr << "(websocket_test_server) WebSocket write error: " << ec.message() << "\n";
                continue;
            }

            co_await ws.async_read(buffer, net_awaitable[ec]);
            if (ec) {
                std::cerr << "(websocket_test_server) WebSocket read error: " << ec.message() << "\n";
                continue;
            }

            http::response_parser<http::dynamic_body> response_parser;
            response_parser.eager(true);
            response_parser.put(buffer.data(), ec);
            auto response = response_parser.release();

            std::string content_type = std::string(response[http::field::content_type]);
            std::cout << "(websocket_test_server) Received response with content type: " << content_type << "\n";
            handle_response_content(content_type, response.body());

            buffer.consume(buffer.size());
        }
    } catch (const std::exception& e) {
        std::cerr << "(websocket_test_server) Error in websocket_test_server: " << e.what() << "\n";
    }
}

extern "C" EXPORT_SYMBOL void run_data_flow_service() {
    net::io_context ioc(1);
    flow_config config = load_flow_config();
    std::shared_ptr<proxy_server> flow;
    net::co_spawn(ioc, start_data_flow(ioc, flow, config), net::detached);
    ioc.run();
}

extern "C" EXPORT_SYMBOL void run_websocket_client() {
    net::io_context ioc(1);
    flow_config config = load_flow_config();
    net::co_spawn(ioc, run_websocket_with_data_flow(ioc, config), net::detached);
    ioc.run();
}

extern "C" EXPORT_SYMBOL void run_websocket_client_with_params(const char* config_data) {
    std::string config_str(config_data);

    net::io_context ioc(1);
    flow_config config = parse_config_from_string(config_data);
    net::co_spawn(ioc, run_websocket_with_data_flow(ioc, config), net::detached);
    ioc.run();
}

extern "C" EXPORT_SYMBOL void run_websocket_test_server() {
    net::io_context ioc(1);
    flow_config config = load_flow_config();
    net::co_spawn(ioc, websocket_test_server(ioc, config), net::detached);
    ioc.run();
}
