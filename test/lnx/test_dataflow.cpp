#include <iostream>
#include <dlfcn.h>

typedef void (*run_data_flow_service_func)();

int main() {
    // Load the shared library
    void* handle = dlopen("./lib/libdata_flow.so", RTLD_LAZY);
    if (!handle) {
        std::cerr << "Failed to load shared library: " << dlerror() << std::endl;
        return 1;
    }

    // Load the function
    run_data_flow_service_func run_data_flow_service = (run_data_flow_service_func)dlsym(handle, "run_data_flow_service");
    if (!run_data_flow_service) {
        std::cerr << "Failed to locate run_data_flow_service in shared library: " << dlerror() << std::endl;
        dlclose(handle);
        return 1;
    }

    // Run the service
    run_data_flow_service();

    // Close the shared library
    dlclose(handle);
    return 0;
}
