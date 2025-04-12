Boost.MQTT5: A C++17 MQTT client based on Boost.Asio
===============================

Branch | Linux Build | Windows Build | Coverage | Documentation |
-------|-------------|---------------|----------|---------------|
[`master`](https://github.com/boostorg/mqtt5/tree/master) | [![build status](https://github.com/boostorg/mqtt5/actions/workflows/ci-posix.yml/badge.svg?branch=master)](https://github.com/boostorg/mqtt5/actions/workflows/ci-posix.yml) | [![build status](https://github.com/boostorg/mqtt5/actions/workflows/ci-windows.yml/badge.svg?branch=master)](https://github.com/boostorg/mqtt5/actions/workflows/ci-windows.yml) | [![codecov](https://codecov.io/gh/boostorg/mqtt5/branch/master/graph/badge.svg)](https://codecov.io/gh/boostorg/mqtt5/tree/master) | [documentation](https://www.boost.org/doc/libs/master/libs/mqtt5/doc/html/index.html)
[`develop`](https://github.com/boostorg/mqtt5/tree/develop) | [![build status](https://github.com/boostorg/mqtt5/actions/workflows/ci-posix.yml/badge.svg?branch=develop)](https://github.com/boostorg/mqtt5/actions/workflows/ci-posix.yml) | [![build status](https://github.com/boostorg/mqtt5/actions/workflows/ci-windows.yml/badge.svg?branch=develop)](https://github.com/boostorg/mqtt5/actions/workflows/ci-windows.yml) | [![codecov](https://codecov.io/gh/boostorg/mqtt5/branch/develop/graph/badge.svg)](https://codecov.io/gh/boostorg/mqtt5/tree/develop) | [documentation](https://www.boost.org/doc/libs/develop/libs/mqtt5/doc/html/index.html)

Boost.MQTT5 is a professional, industrial-grade C++17 client built on [Boost.Asio](https://www.boost.org/doc/libs/master/doc/html/boost_asio.html). This Client is designed for publishing or receiving messages from an MQTT 5.0 compatible Broker. Boost.MQTT5 represents a comprehensive implementation of the MQTT 5.0 protocol standard, offering full support for publishing or receiving messages with QoS 0, 1, and 2. 

Motivation
---------
 The [MQTT](https://mqtt.org/) protocol is widely used for communication in various real-world scenarios, primarily as a reliable communication protocol for data transfer to and from IoT devices. While the MQTT protocol is relatively straightforward, integrating it into an application can be complex, primarily due to the challenging implementation of message retransmission after a disconnect/reconnect sequence. To address these challenges and simplify MQTT integration once and for all, Boost.MQTT5 was designed with the following core objectives in mind:

 Objective | Description |
 ----------|-------------|
"Plug and play" interface that abstracts away MQTT protocol internals, low-level network events, message retransmission and other complexities | Enables developers to publish or receive messages with just a single line of code, significantly reducing the learning curve and development time. Getting started requires basic MQTT knowledge, making the library accessible to developers of all skill levels. |
Highly reliable and robust Client that handles network losses, unreliable data transport, network latencies, and other unpredictable events | The Client does not expose connect functions (nor asynchronous connect functions); instead, network connectivity, MQTT handshake, and message retransmission are automatically handled within the Client while strictly adhering to the MQTT specification.  This automation eliminates the need for developers to write extensive and error-prone code to handle these scenarios, allowing them to focus on the application's core functionality. |
Complete adherence to the Boost.Asio's universal asynchronous model | The interfaces and implementation strategies are built upon the foundations of Boost.Asio. This compatibility enables seamless integration with any other library within the Boost.Asio ecosystem. |

When to Use
---------
 Boost.MQTT5 might be suitable for you if any of the following statements is true:

- Your application uses Boost.Asio and requires integrating an MQTT Client.
- You require asynchronous access to an MQTT Broker.
- You are developing a higher-level component that requires a connection to an MQTT Broker.
- You require a dependable and resilient MQTT Client to manage all network-related issues automatically.

It may not be suitable for you if:
- You solely require synchronous access to an MQTT Broker.
- The MQTT Broker you connect to does not support the MQTT 5 version.

Features
---------
Boost.MQTT5 is a library designed with the core belief that users should focus solely on their application logic, not the network complexities.
The library attempts to embody this belief with a range of key features designed to elevate the development experience: 

Feature | Description |
--------|-------------|
**Complete TCP, TLS/SSL, and WebSocket support** | The MQTT protocol requires an underlying network protocol that provides ordered, lossless, bi-directional connection (stream). Users can customize this stream through a template parameter. The Boost.MQTT5 library has been tested with the most used transport protocols: TCP/IP using `boost::asio::ip::tcp::socket`, TLS/SSL using `boost::asio::ssl::stream`, WebSocket/TCP and WebSocket/TLS using `boost::beast::websocket::stream`). |
**Automatic reconnect and retry mechanism** | Automatically handles connection loss, backoffs, reconnections, and message transmissions. Automating these processes enables users to focus entirely on the application's functionality. See [Built-in Auto-Reconnect and Retry Mechanism](https://www.boost.org/doc/libs/master/libs/mqtt5/doc/html/mqtt5/auto_reconnect.html). |
**Prioritised efficiency** | Utilises network and memory resources as efficiently as possible. |
**Minimal memory footprint** | Ensures optimal performance in resource-constrained environments typical of IoT devices. |
 **Completion token** | All asynchronous functions are implemented using Boost.Asio's universal asynchronous model and support CompletionToken. This allows versatile usage with callbacks, coroutines, and futures. |
**Custom allocators** | Support for custom allocators allows extra flexibility and control over the memory resources. Boost.MQTT5 will use allocators associated with handlers from asynchronous functions to create instances of objects needed in the library implementation. |
**Per-operation cancellation** | All asynchronous operations support individual, targeted cancellation as per Asio's [Per-Operation Cancellation](https://www.boost.org/doc/libs/1_82_0/doc/html/boost_asio/overview/core/cancellation.html). This enables all asynchronous functions to be used in [Parallel Operations](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/overview/composition/parallel_group.html), which is especially beneficial for executing operations that require a timeout mechanism.
**Full implementation of MQTT 5.0 specification** | Ensures full compatibility with [MQTT 5.0 standard](https://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html). This latest version introduces essential features that enhance system robustness, including advanced error-handling mechanisms, session and message expiry, and other improvements designed to support modern IoT use cases.  |
**Support for QoS 0, QoS 1, and QoS 2**| Fully implements all quality-of-service levels defined by the MQTT protocol to match different reliability needs in message delivery. |
**Custom authentication** | Defines an interface for your custom authenticators to perform [Enhanced Authentication](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901256). In addition to username/password authentication, this customization point enables the implementation of any authentication mechanism supported by the SASL protocol, offering flexibility for advanced security requirements. |
**High availability** | Supports listing multiple Brokers within the same cluster to which the Client can connect. In the event of a connection failure with one Broker, the Client switches to the next in the list. |
**Offline buffering** | Automatically buffers all requests made while offline, ensuring they are sent when the connection is re-established. This allows users to call any asynchronous function freely, regardless of current connection status. |

Getting Started
---------
Boost.MQTT5 is a header-only library. To use Boost.MQTT5 it requires the following: 
- **C++17 capable compiler**
- **Boost 1.82 or later**. In addition to Asio, we use other header-only libraries.
- **OpenSSL**. If you require an SSL connection by using [boost::asio::ssl::stream](https://www.boost.org/doc/libs/1_82_0/doc/html/boost_asio/reference/ssl__stream.html).

Boost.MQTT5 has been tested with the following compilers: 
- clang 12.0 - 18.0 (Linux)
- GCC 9 - 14 (Linux)
- MSVC 14.37 - Visual Studio 2022 (Windows)

Using the Library
---------

1. Download [Boost](https://www.boost.org/users/download/), and add it to your include path.
2. If you use SSL, download [OpenSSL](https://www.openssl.org/), link the library and add it to your include path.
3. Add Boost.MQTT5's `include` folder to your include path.

You can compile the example below with the following command line on Linux:

    $ clang++ -std=c++17 <source-cpp-file> -o example -I<path-to-boost> -Iinclude -pthread


Example
---------
The following example illustrates a scenario of configuring a Client and publishing a
"Hello World!" Application Message with `QoS` 0. 

```cpp
#include <boost/mqtt5/mqtt_client.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <iostream>

int main() {
	boost::asio::io_context ioc;

	boost::mqtt5::mqtt_client<boost::asio::ip::tcp::socket> c(ioc);

	c.brokers("<your-mqtt-broker>", 1883)
		.credentials("<your-client-id>", "<client-username>", "<client-pwd>")
		.async_run(boost::asio::detached);

	c.async_publish<boost::mqtt5::qos_e::at_most_once>(
		"<topic>", "Hello world!",
		boost::mqtt5::retain_e::no, boost::mqtt5::publish_props {},
		[&c](boost::mqtt5::error_code ec) {
			std::cout << ec.message() << std::endl;
			c.async_disconnect(boost::asio::detached); // disconnect and close the Client
		}
	);
	
	ioc.run();
}
```
To see more examples, visit [Examples](https://github.com/boostorg/mqtt5/tree/master/example).

Contributing
---------
When contributing to this repository, please first discuss the change you wish to make via issue, email, or any other method with the owners of this repository before making a change.

You may merge a Pull Request once you have the sign-off from other developers, or you may request the reviewer to merge it for you.

Credits
--------- 

Maintained and authored by [Mireo](https://www.mireo.com).

<p align="center">
<a href="https://www.mireo.com"><img height="200" alt="Mireo" src="https://www.mireo.com/img/assets/mireo-logo.svg"></img></a>
</p>
