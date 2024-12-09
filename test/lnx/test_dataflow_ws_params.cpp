#include <iostream>
#include <dlfcn.h>

typedef void (*run_websocket_client_with_params_func)(const char*);

int main() {
    // Load the shared library
    void* handle = dlopen("./lib/libdata_flow.so", RTLD_LAZY);
    if (!handle) {
        std::cerr << "Failed to load shared library: " << dlerror() << std::endl;
        return 1;
    }

    // Load the function
    run_websocket_client_with_params_func run_websocket_client_with_params = (run_websocket_client_with_params_func)dlsym(handle, "run_websocket_client_with_params");
    if (!run_websocket_client_with_params) {
        std::cerr << "Failed to locate run_websocket_client_with_params in shared library: " << dlerror() << std::endl;
        dlclose(handle);
        return 1;
    }

    const char* config = R"(
    host=127.0.0.1
    port=10800
    username=default_user
    password=default_pass
    manager_endpoint=127.0.0.1:80
    websocket_test_url=example.com
    buffer_size=8192
    reconnect_delay=5
    )";
    run_websocket_client_with_params(config);

    // Close the shared library
    dlclose(handle);
    return 0;
}
