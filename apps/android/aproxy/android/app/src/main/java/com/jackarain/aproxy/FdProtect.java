package com.jackarain.aproxy;

import android.net.VpnService;
import android.util.Log;
import java.io.FileDescriptor;
import java.lang.reflect.Field;
import java.net.Socket;

/**
 * 获取 Socket 底层 FileDescriptor 并调用 VpnService.protect()。
 *
 * 遍历 Socket.impl 及父类链上的 fd 字段 (类型可能是 FileDescriptor/int/long),
 * 优先把 FileDescriptor / int 传给 VpnService.protect()。
 */
public class FdProtect {
    private static final String TAG = "aproxy-fd";

    /** 返回 Socket 对应的底层 FileDescriptor; 失败返回 null. */
    public static FileDescriptor socketFd(Socket sock) {
        try {
            Object impl = getField(Socket.class, sock, "impl");
            if (impl == null) {
                Log.w(TAG, "socketFd: impl 为空, Socket 类=" + sock.getClass().getName());
                return null;
            }
            Class<?> c = impl.getClass();
            while (c != null && c != Object.class) {
                try {
                    Field f = c.getDeclaredField("fd");
                    f.setAccessible(true);
                    Object fdObj = f.get(impl);
                    if (fdObj instanceof FileDescriptor) {
                        return (FileDescriptor) fdObj;
                    }
                    if (f.getType() == int.class) {
                        // fd 是 int, 包成一个 FileDescriptor.
                        return makeFd(f.getInt(impl));
                    }
                    Log.w(TAG, "socketFd: fd 字段类型=" + f.getType().getName()
                            + " impl 类=" + c.getName());
                } catch (NoSuchFieldException ignored) {
                    // 继续父类.
                }
                c = c.getSuperclass();
            }
            Log.w(TAG, "socketFd: impl 类层次无 fd 字段, impl 类=" + impl.getClass().getName());
        } catch (Exception e) {
            Log.w(TAG, "socketFd 异常: " + e);
        }
        return null;
    }

    /** 返回 DatagramSocket 的底层 fd; 失败返回 -1. */
    public static int datagramFdInt(java.net.DatagramSocket ds) {
        try {
            Object impl = getField(ds.getClass(), ds, "impl");
            if (impl == null) return -1;
            // DatagramSocketImpl 的 fd 字段 (PlainDatagramSocketImpl.fd).
            Object fdObj = getField(impl.getClass(), impl, "fd");
            if (fdObj instanceof FileDescriptor) {
                return fdInt((FileDescriptor) fdObj);
            }
        } catch (Exception e) {
            Log.w(TAG, "datagramFdInt 异常: " + e);
        }
        return -1;
    }

    /** 用 int fd 构造 FileDescriptor. */
    private static FileDescriptor makeFd(int fd) {
        try {
            FileDescriptor file = new FileDescriptor();
            setFdInt(file, fd);
            return file;
        } catch (Exception e) {
            Log.w(TAG, "makeFd 异常: " + e);
            return null;
        }
    }

    /** 从 FileDescriptor 中取底层 int fd (字段名可变, 按类型取). 失败返回 -1. */
    public static int fdInt(FileDescriptor fd) {
        if (fd == null) return -1;
        try {
            for (Field f : FileDescriptor.class.getDeclaredFields()) {
                f.setAccessible(true);
                if (f.getType() == int.class) {
                    int v = f.getInt(fd);
                    if (v >= 0) return v;
                } else if (f.getType() == long.class) {
                    long lv = f.getLong(fd);
                    if (lv > 0 && lv <= Integer.MAX_VALUE) return (int) lv;
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "fdInt 遍历异常: " + e);
        }
        return -1;
    }

    /** 把 int 写入 FileDescriptor 的 int/long 字段. */
    private static void setFdInt(FileDescriptor fd, int value) throws Exception {
        for (Field f : FileDescriptor.class.getDeclaredFields()) {
            f.setAccessible(true);
            if (f.getType() == int.class) { f.setInt(fd, value); return; }
            if (f.getType() == long.class) { f.setLong(fd, (long) value); return; }
        }
        throw new NoSuchFieldException("no int/long field in FileDescriptor");
    }

    /** 直接调用 VpnService.protect(int). */
    public static boolean protectFd(VpnService vpn, int fd) {
        try {
            return vpn.protect(fd);
        } catch (Exception e) {
            Log.w(TAG, "protect(int) 异常: " + e);
            return false;
        }
    }

    private static Object getField(Class<?> clazz, Object obj, String name) {
        Class<?> c = clazz;
        while (c != null && c != Object.class) {
            try {
                Field f = c.getDeclaredField(name);
                f.setAccessible(true);
                return f.get(obj);
            } catch (NoSuchFieldException ignored) {
                c = c.getSuperclass();
            } catch (Exception e) {
                return null;
            }
        }
        return null;
    }
}

