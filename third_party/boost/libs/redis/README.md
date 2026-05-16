# Boost.Redis

Boost.Redis is a high-level [Redis](https://redis.io/) client library built on top of
[Boost.Asio](https://www.boost.org/doc/libs/latest/doc/html/boost_asio.html)
that implements the Redis protocol
[RESP3](https://github.com/redis/redis-specifications/blob/master/protocol/RESP3.md).

Full documentation is [here](https://www.boost.org/doc/libs/master/libs/redis/index.html).

## Requirements

The requirements for using Boost.Redis are:

* Boost 1.84 or higher. Boost.Redis is included in Boost installations since Boost 1.84.
* C++17 or higher. Supported compilers include gcc 11 and later, clang 11 and later, and Visual Studio 16 (2019) and later.
* Redis 6 or higher (must support RESP3).
* OpenSSL.

The documentation assumes basic-level knowledge about [Redis](https://redis.io/docs/) and [Boost.Asio](https://www.boost.org/doc/libs/latest/doc/html/boost_asio.html).

## Building the library

To use the library it is necessary to include the following:

```cpp
#include <boost/redis/src.hpp>
```

in exactly one source file in your applications. Otherwise, the library is header-only.

Boost.Redis unconditionally requires OpenSSL. Targets using Boost.Redis need to link
to the OpenSSL libraries.

## Tutorial

The code below uses a short-lived connection to
[ping](https://redis.io/commands/ping/) a Redis server:


```cpp
#include <boost/redis/connection.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/detached.hpp>
#include <iostream>

namespace net = boost::asio;
using boost::redis::request;
using boost::redis::response;
using boost::redis::config;
using boost::redis::connection;

auto co_main(config const& cfg) -> net::awaitable<void>
{
   auto conn = std::make_shared<connection>(co_await net::this_coro::executor);
   conn->async_run(cfg, {}, net::consign(net::detached, conn));

   // A request containing only a ping command.
   request req;
   req.push("PING", "Hello world");

   // Response object.
   response<std::string> resp;

   // Executes the request.
   co_await conn->async_exec(req, resp);
   conn->cancel();

   std::cout << "PING: " << std::get<0>(resp).value() << std::endl;
}
```

The roles played by the `async_run` and `async_exec` functions are:

* `connection::async_exec`: executes the commands contained in the
  request and stores the individual responses in the response object. Can
  be called from multiple places in your code concurrently.
* `connection::async_run`: keeps the connection healthy. It takes care of hostname resolution, session establishment, health-checks, reconnection and coordination of low-level read and write operations. It should be called only once per connection, regardless of the number of requests to execute.

## Server pushes

Redis servers can also send a variety of pushes to the client. Some of
them are:

* [Pubsub messages](https://redis.io/docs/latest/develop/pubsub/).
* [Keyspace notifications](https://redis.io/docs/latest/develop/pubsub/keyspace-notifications/).
* [Client-side caching](https://redis.io/docs/latest/develop/clients/client-side-caching/).

The connection class supports server pushes by means of the
`connection::async_receive2` function, which can be
called in the same connection that is being used to execute commands.
The coroutine below shows how to use it


```cpp
auto receiver(std::shared_ptr<connection> conn) -> asio::awaitable<void>
{
   generic_flat_response resp;
   conn->set_receive_response(resp);

   // Subscribe to the channel 'mychannel'. You can add any number of channels here.
   request req;
   req.subscribe({"mychannel"});
   co_await conn->async_exec(req);

   // You're now subscribed to 'mychannel'. Pushes sent over this channel will be stored
   // in resp. If the connection encounters a network error and reconnects to the server,
   // it will automatically subscribe to 'mychannel' again. This is transparent to the user.
   // You need to use specialized request::subscribe() function (instead of request::push)
   // to enable this behavior.

   // Loop to read Redis push messages.
   while (conn->will_reconnect()) {
      // Wait for pushes
      auto [ec] = co_await conn->async_receive2(asio::as_tuple);

      // Check for errors and cancellations
      if (ec) {
         std::cerr << "Error during receive: " << ec << std::endl;
         break;
      }

      // This can happen if a SUBSCRIBE command errored (e.g. insufficient permissions)
      if (resp.has_error()) {
         std::cerr << "The receive response contains an error: " << resp.error().diagnostic
                   << std::endl;
         break;
      }

      // The response must be consumed without suspending the
      // coroutine i.e. without the use of async operations.
      for (push_view elem : push_parser(resp.value())) {
         std::cout << "Received message from channel " << elem.channel << ": " << elem.payload
                   << "\n";
      }

      resp.value().clear();
   }
}
```

## Further reading

Full documentation is [here](https://www.boost.org/doc/libs/master/libs/redis/index.html).
