# 音乐播放器示例

这是一个利用 `proxy_server` 的 `http` 服务器功能实现的音乐播放器示例

你可用利用 `proxy_server` 启动一个 `http` 服务，运行参数如：

``` bash
./proxy_server --autoindex true --http_doc /user/music --server_listen 0.0.0.0:1080
```

参数 `autoindex` 是启动 `http` 文件浏览功能

参数 `http_doc` 是启动 `http` 文件所在目录

在 `/user/music` 目录下，你可以将 `mp3，aac, m4a` 等音乐文件拷贝到这里，甚至是 `.lrc` 的歌词文件也拷贝到这个目录下(或 `/user/music/lyrics` 目录下)，同时也将这个 `index.html` 文件拷贝到 `/user/music` 目录下

这样一个网络在线音乐播放器就完成了，你可以通过浏览器打开这个页面, 就可以播放 `/user/music` 下的音乐了，而且还有歌词显示功能。

示例：https://www.jackarain.org/music/index.html

![image](https://github.com/user-attachments/assets/3910015e-f4d6-4162-912b-8b7594c41d88)