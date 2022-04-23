# boost
cmake based plugable static compiled boost library
---

have you been tired of adding include directories and link search path over and over again?

have you been tired of explaining how to compile your boost dependended project to your college?

How here is CMake based boost and it's statically linked into your project.

how to use it?

simple

```
$ git submodule add https://github.com/microcai/boost.git third_party/boost
```

then add

```
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost 1.55 COMPONENTS thread system filesystem program_options random atomic chrono)
if (NOT Boost_FOUND)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/boost)
endif()
```

to your project CMakeLists.txt

then use

```
target_link_libraries("your target" ${BOOST_LIBRARIES} )
```

下面是照顾不会英语的人的
===


如何使用？

简单，首先 git 子模块添加进来


```
$ git submodule add https://github.com/microcai/boost.git third_party/boost
```

然后在 CMakeLists.txt 里加入这么一条

```
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost 1.55 COMPONENTS thread system filesystem program_options random atomic chrono)
if (NOT Boost_FOUND)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/boost)
endif()
```

接下来就可以使用了。用如下的命令添加链接就可以了。头文件路径会自动加入。

```
target_link_libraries("your target" ${BOOST_LIBRARIES} )
```


# 如何在不能使用 Boost 的公司使用 Boost ？

一般而言，公司禁止 Boost 无非就是2个原因

其一，领导是70后 MFC 程序员。只学习过 C 和 C with class。

其二，公司为节省成本大量招聘不合格程序员，这些程序员没有真正掌握 C++，公司害怕无人能维护使用Boost的代码。

在第一种情况下，没有任何领导敢勇于承认自己是个傻逼，不会 Boost。一般都会拿出诸如 “配置繁琐” “带上一大堆库不好” 等理由搪塞。
针对此理由，解决之道就是使用本项目。将 Boost 源码整个纳入工程后，不再需要额外编译配置 Boost。只要打开自己的工程按下 F7 即可编译。
而且 Boost 是以静态库链入可执行文件。同时也是使用的整个工程完全相同的参数编译，不会出现编译参数不同引起的兼容性问题。

如果是第二点，无解，换公司吧，你这样的人才太厉害了，公司根本不需要啊！

