一个socks服务器的实现
=====================

支持socks4/5协议.


使用方法:

在服务器上编译运行命令  socks_server.exe 4567

在客户浏览器上设置socks代理, 主机为服务器IP, 端口为4567.

然后就可以开始使用代理服务器了.


TODO: 

1. 支持认证.
2. 支持ipv4/ipv6/domainname.
3. 支持connect/bind/udp等各种参数.
