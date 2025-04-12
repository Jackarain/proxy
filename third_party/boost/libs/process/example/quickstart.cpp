#include <boost/process.hpp>
#include <boost/asio.hpp>

namespace asio = boost::asio;
using boost::process::process;


int main(int /*argv*/, char ** /*argv*/)
{
  asio::io_context ctx;
  {
    //tag::cp[]
    // process(asio::any_io_executor, filesystem::path, range<string> args, AdditionalInitializers...)
    process proc(ctx.get_executor(), // <1>
                 "/usr/bin/cp", // <2>
                 {"source.txt", "target.txt"} // <3>
    ); // <4>
    //end::cp[]
  }
  {
    //tag::ls[]
    process proc(ctx, "/bin/ls", {});
    assert(proc.wait() == 0);
    //end::ls[]
  }
  {
    //tag::terminate[]
    process proc(ctx, "/bin/totally-not-a-virus", {});
    proc.terminate();
    //end::terminate[]
  }
  {
    //tag::request_exit[]
    process proc(ctx, "/bin/bash", {});
    proc.request_exit();
    proc.wait();
    //end::request_exit[]
  }
  {
    //tag::interrupt[]
    process proc(ctx, "/usr/bin/addr2line", {});
    proc.interrupt();
    proc.wait();
    //end::interrupt[]
  }
  {
    //tag::execute[]
    assert(execute(process(ctx, "/bin/ls", {})) == 0);
    //end::execute[]
  }
  {
    //tag::async_execute[]
    async_execute(process(ctx, "/usr/bin/g++", {"hello_world.cpp"}))
        (asio::cancel_after(std::chrono::seconds(10), asio::cancellation_type::partial)) // <1>
        (asio::cancel_after(std::chrono::seconds(10), asio::cancellation_type::terminal)) //<2>
        (asio::detached);
    //end::async_execute[]
    ctx.run();
  }
  return 0;
}


