//
// http_error_body.hpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_10_13__HTTP_ERROR_BODY_HPP
#define INCLUDE__2026_10_13__HTTP_ERROR_BODY_HPP


namespace proxy {

	//////////////////////////////////////////////////////////////////////////
	// HTTP 错误响应正文, 供会话侧各 HTTP 路由复用.

	inline const char* fake_500_content_fmt =
R"x*x*x(<html>
<head><title>500 Internal Server Error</title></head>
<body>
<center><h1>500 Internal Server Error</h1></center>
<hr><center>nginx/1.20.2</center>
</body>
</html>)x*x*x";

	inline const char* fake_400_content_fmt =
R"x*x*x(HTTP/1.1 400 Bad Request
Server: nginx/1.20.2
Date: {}
Content-Type: text/html
Content-Length: 165
Connection: close

<html>
<head><title>400 Bad Request</title></head>
<body bgcolor="white">
<center><h1>400 Bad Request</h1></center>
<hr><center>nginx/1.20.2</center>
</body>
</html>)x*x*x";

	inline const char* fake_400_content =
R"x*x*x(<html>
<head><title>400 Bad Request</title></head>
<body bgcolor="white">
<center><h1>400 Bad Request</h1></center>
<hr><center>nginx/1.20.2</center>
</body>
</html>)x*x*x";

	inline const char* fake_401_content =
R"x*x*x(<html>
<head><title>401 Authorization Required</title></head>
<body>
<center><h1>401 Authorization Required</h1></center>
<hr><center>nginx/1.20.2</center>
</body>
</html>)x*x*x";

	inline const char* fake_403_content =
R"x*x*x(<html>
<head><title>403 Forbidden</title></head>
<body>
<center><h1>403 Forbidden</h1></center>
<hr><center>nginx/1.20.2</center>
</body>
</html>
)x*x*x";

	inline const char* fake_405_content =
R"x*x*x(<html>
<head><title>405 Method Not Allowed</title></head>
<body>
<center><h1>405 Method Not Allowed</h1></center>
<hr><center>nginx/1.20.2</center>
</body>
</html>
)x*x*x";

	inline const char* fake_404_content_fmt =
R"x*x*x(HTTP/1.1 404 Not Found
Server: nginx/1.20.2
Date: {}
Content-Type: text/html
Content-Length: 145
Connection: close

<html><head><title>404 Not Found</title></head>
<body>
<center><h1>404 Not Found</h1></center>
<hr>
<center>nginx/1.20.2</center>
</body>
</html>)x*x*x";

	inline const char* fake_407_content_fmt =
R"x*x*x(HTTP/1.1 407 Proxy Authentication Required
Server: nginx/1.20.2
Date: {}
Connection: close
Proxy-Authenticate: Basic realm="proxy"
Proxy-Connection: close
Content-Length: 0

)x*x*x";

	inline const char* fake_416_content =
R"x*x*x(<html>
<head><title>416 Requested Range Not Satisfiable</title></head>
<body>
<center><h1>416 Requested Range Not Satisfiable</h1></center>
<hr><center>nginx/1.20.2</center>
</body>
</html>
)x*x*x";

	inline const char* fake_302_content =
R"x*x*x(<html>
<head><title>301 Moved Permanently</title></head>
<body>
<center><h1>301 Moved Permanently</h1></center>
<hr><center>nginx/1.20.2</center>
</body>
</html>
)x*x*x";

	inline const char* fake_502_dns_content =
R"x*x*x(<html>
<head><title>502 Bad Gateway</title></head>
<body>
<center><h1>502 Bad Gateway</h1></center>
<hr><center>nginx/1.20.2</center>
<p>DNS resolution failed.</p>
</body>
</html>)x*x*x";

	inline const char* fake_502_socks_connect_content =
R"x*x*x(<html>
<head><title>502 Bad Gateway</title></head>
<body>
<center><h1>502 Bad Gateway</h1></center>
<hr><center>nginx/1.20.2</center>
<p>Cannot connect to upstream SOCKS proxy.</p>
</body>
</html>)x*x*x";

	inline const char* fake_502_ssl_config_content =
R"x*x*x(<html>
<head><title>502 Bad Gateway</title></head>
<body>
<center><h1>502 Bad Gateway</h1></center>
<hr><center>nginx/1.20.2</center>
<p>SSL configuration failed.</p>
</body>
</html>)x*x*x";

	inline const char* fake_502_ssl_handshake_content =
R"x*x*x(<html>
<head><title>502 Bad Gateway</title></head>
<body>
<center><h1>502 Bad Gateway</h1></center>
<hr><center>nginx/1.20.2</center>
<p>SSL handshake with upstream SOCKS proxy failed.</p>
</body>
</html>)x*x*x";

	inline const char* fake_502_socks_associate_content =
R"x*x*x(<html>
<head><title>502 Bad Gateway</title></head>
<body>
<center><h1>502 Bad Gateway</h1></center>
<hr><center>nginx/1.20.2</center>
<p>SOCKS5 UDP ASSOCIATE failed.</p>
</body>
</html>)x*x*x";

	inline const char* fake_502_proxy_connect_content =
R"x*x*x(<html>
<head><title>502 Bad Gateway</title></head>
<body>
<center><h1>502 Bad Gateway</h1></center>
<hr><center>nginx/1.20.2</center>
<p>Cannot connect to upstream proxy.</p>
</body>
</html>)x*x*x";

	inline const char* fake_403_forbidden_content =
R"x*x*x(<html>
<head><title>403 Forbidden</title></head>
<body>
<center><h1>403 Forbidden</h1></center>
<hr><center>nginx/1.20.2</center>
<p>UDP protocol disabled.</p>
</body>
</html>)x*x*x";

}


#endif // INCLUDE__2026_10_13__HTTP_ERROR_BODY_HPP
