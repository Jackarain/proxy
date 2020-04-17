一个socks服务器的实现
=====================

支持socks4/5 udp代理协议的socks代理服务器实现.


##### 使用方法:

在服务器上编译运行命令

```
socks_server.exe --address 0.0.0.0 --port 4567
```

在客户浏览器上设置socks代理, 主机为服务器IP, 端口为4567.

然后就可以开始使用代理服务器了.

注意：如果服务器需要认证用户名密码，socks_server内置了一个简单的js引擎，

可以在socks_server所在目录实现do_auth.js即可认证。

do_auth.js必须实现do_auth/do_auth4函数, 参考项目所带的 do_auth.js

