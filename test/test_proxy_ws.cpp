// test_proxy.cpp
// ~~~~~~~~~~~~~~~~
// A wrapper for testing data flow

#include <iostream>
#include <dlfcn.h>

typedef void (*run_websocket_client_func)();

int main() {
    // Load the shared library
    void* handle = dlopen("./lib/libdata_flow.so", RTLD_LAZY);
    if (!handle) {
        std::cerr << "Failed to load shared library: " << dlerror() << std::endl;
        return 1;
    }

    // Load the function
    run_websocket_client_func run_websocket_client = (run_websocket_client_func)dlsym(handle, "run_websocket_client");
    if (!run_websocket_client) {
        std::cerr << "Failed to locate run_websocket_client in shared library: " << dlerror() << std::endl;
        dlclose(handle);
        return 1;
    }

    // Run the service
    run_websocket_client();

    // Close the shared library
    dlclose(handle);
    return 0;
}
