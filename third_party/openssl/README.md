*HOW TO USE*

adding lines to cmakelists.txt


```
find_package(OpenSSL)
if (NOT OpenSSL_FOUND)
	add_subdirectory(3rd/openssl-cmake)
	set(OPENSSL_LIBRARIES crypto ssl)
else()
	set(OPENSSL_LIBRARIES OpenSSL::Crypto OpenSSL::SSL)
endif()
```
