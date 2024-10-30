#include <iostream>
#include <windows.h>

typedef void (*run_data_flow_service_func)();

int main() {
    // Load the DLL
    HMODULE handle = LoadLibrary(TEXT("data_flow.dll"));
    if (!handle) {
        std::cerr << "Failed to load DLL: " << GetLastError() << std::endl;
        return 1;
    }

    // Load the functions
    run_data_flow_service_func run_data_flow_service = 
        (run_data_flow_service_func)GetProcAddress(handle, "run_data_flow_service");
    if (!run_data_flow_service) {
        std::cerr << "Failed to locate run_data_flow_service in DLL: " << GetLastError() << std::endl;
        FreeLibrary(handle);
        return 1;
    }

    run_data_flow_service();

    // Close the DLL
    FreeLibrary(handle);
    return 0;
}
