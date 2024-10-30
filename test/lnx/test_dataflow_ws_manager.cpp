#include <iostream>
#include <dlfcn.h>
#include <thread>

typedef void (*run_websocket_test_server_func)();
typedef void (*run_websocket_client_func)();

int main() {
    // Load the shared library
    void* handle = dlopen("./lib/libdata_flow.so", RTLD_LAZY);
    if (!handle) {
        std::cerr << "Failed to load shared library: " << dlerror() << std::endl;
        return 1;
    }

    // Load the functions
    run_websocket_test_server_func run_websocket_test_server = (run_websocket_test_server_func)dlsym(handle, "run_websocket_test_server");
    if (!run_websocket_test_server) {
        std::cerr << "Failed to locate run_websocket_test_server in shared library: " << dlerror() << std::endl;
        dlclose(handle);
        return 1;
    }
    
    run_websocket_client_func run_websocket_client = (run_websocket_client_func)dlsym(handle, "run_websocket_client");
    if (!run_websocket_client) {
        std::cerr << "Failed to locate run_websocket_client in shared library: " << dlerror() << std::endl;
        dlclose(handle);
        return 1;
    }

    // Run the server and client functions in separate threads
    std::thread server_thread(run_websocket_test_server);
    std::thread client_thread(run_websocket_client);

    // Wait for both threads to finish
    server_thread.join();
    client_thread.join();

    // Close the shared library
    dlclose(handle);
    return 0;
}
