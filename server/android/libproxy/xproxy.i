%module xproxy
%{
#include "xproxy.hpp"
%}

%include <std_string.i>
%include <std_shared_ptr.i>
%include <stdint.i>

namespace xproxy {

	std::string min_sdk_version();
	std::string build_version();

	int start(const std::string& config);
	void stop();
}
