#include <iostream>
#include <windows.h>
#include <thread>

typedef void (*run_websocket_test_server_func)();
typedef void (*run_websocket_client_func)();

int main() {
    // Load the DLL
    HMODULE handle = LoadLibrary(TEXT("data_flow.dll"));
    if (!handle) {
        std::cerr << "Failed to load DLL: " << GetLastError() << std::endl;
        return 1;
    }

    // Load the functions
    run_websocket_test_server_func run_websocket_test_server = 
        (run_websocket_test_server_func)GetProcAddress(handle, "run_websocket_test_server");
    if (!run_websocket_test_server) {
        std::cerr << "Failed to locate run_websocket_test_server in DLL: " << GetLastError() << std::endl;
        FreeLibrary(handle);
        return 1;
    }
    
    run_websocket_client_func run_websocket_client = 
        (run_websocket_client_func)GetProcAddress(handle, "run_websocket_client");
    if (!run_websocket_client) {
        std::cerr << "Failed to locate run_websocket_client in DLL: " << GetLastError() << std::endl;
        FreeLibrary(handle);
        return 1;
    }

    // Run the server and client functions in separate threads
    std::thread server_thread(run_websocket_test_server);
    std::thread client_thread(run_websocket_client);

    // Wait for both threads to finish
    server_thread.join();
    client_thread.join();

    // Close the DLL
    FreeLibrary(handle);
    return 0;
}
