# libxproxy.so (SWIG) 通过静态注册引用这些 Java 类与方法,
# 混淆会改名/删除 SwigDirector_* 等入口导致 NoSuchMethodError.
-keep class com.jackarain.** { *; }
