#include <boost/asio/impl/src.hpp>

#if __has_include(<openssl/ssl.h>) && defined (HAVE_OPENSSL)
#	include <boost/asio/ssl/impl/src.hpp>
#endif
