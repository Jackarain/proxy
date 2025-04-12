// Copyright 2024 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#include <boost/dll/detail/demangling/msvc.hpp>

struct CallbackInfo;

#ifdef _MSC_VER
typedef void(__stdcall* Callback)(CallbackInfo* pInfo);
#else
typedef void(*Callback)(CallbackInfo* pInfo);
#endif


int main() {
    namespace parser = boost::dll::detail::parser;
    boost::dll::detail::mangled_storage_impl ms;
    
    // The following case of mangling is not supported at the moment. Make 
    // sure that we detect it on compile time.
    parser::is_mem_fn_with_name<int, void(*)(Callback)>("set_callback", ms)
        ("public: void __cdecl int::set_callback(void (__cdecl*)(struct CallbackInfo * __ptr64)) __ptr64")
    ;
}

