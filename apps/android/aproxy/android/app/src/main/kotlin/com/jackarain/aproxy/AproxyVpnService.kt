package com.jackarain.aproxy

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.VpnService
import android.os.Handler
import android.os.HandlerThread
import android.os.ParcelFileDescriptor
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import java.io.DataInputStream
import java.io.DataOutputStream
import java.io.EOFException
import java.io.FileInputStream
import java.io.FileOutputStream
import java.net.InetSocketAddress
import java.net.ServerSocket
import java.net.Socket
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import io.flutter.plugin.common.MethodChannel
import org.json.JSONObject

/**
 * VpnService: 建立 Android TUN 设备, 并把 TUN 原始 IP 数据包与纯 Dart 代理引擎
 * 桥接 (Kotlin 只做数据面搬运与传统 socket 的 protect, 引擎逻辑全部在 Dart).
 *
 * 数据面桥接: 每条连接里, 一个线程持续读 TUN fd, 以"长4字节大端 + 原始 IP 包"
 * 帧推给 Dart; 同时从 Dart 读取同样帧写入 TUN fd.
 *
 * 受保护转发器: Dart 出站需要绕过 TUN, 但 Dart 无法把自身 socket fd 交给
 * VpnService.protect; 因此 Dart 连入本地转发器 (目标 host:port 作为首帧),
 * Kotlin 创建可 protect 的真实 Socket 并 connect 上游, 之后双向字节转发.
 */
class AproxyVpnService : VpnService() {

    companion object {
        const val ACTION_START = "com.jackarain.aproxy.START"
        const val ACTION_RESTART = "com.jackarain.aproxy.RESTART"
        const val ACTION_STOP = "com.jackarain.aproxy.STOP"
        const val EXTRA_CONFIG = "config"
        const val EXTRA_LAUNCHER_PORT = "launcher_port"

        private const val CHANNEL_ID = "aproxy_vpn"
        private const val NOTIFY_ID = 1001

        @Volatile
        var instance: AproxyVpnService? = null
            private set

        /** 数据面桥接服务端口 (Dart 引擎连接以收发 TUN IP 包). */
        @Volatile
        var tunBridgePort: Int = 0
            private set

        /** 受保护转发器端口 (Dart 引擎出站连接). */
        @Volatile
        var forwardPort: Int = 0
            private set

        @Volatile
        var tunAddress: String = "10.0.0.2"

        fun startForegroundServiceCompat(context: Context, intent: Intent) {
            ContextCompat.startForegroundService(context, intent)
        }

        fun requestStop(context: Context) {
            context.startService(
                Intent(context, AproxyVpnService::class.java).setAction(ACTION_STOP)
            )
        }

        /********** 引擎端口等待机制 (静态, 不依赖实例, 消除启动竞态) **********/
        private val engineWaiters =
            java.util.concurrent.CopyOnWriteArrayList<MethodChannel.Result>()

        /** 注册等待数据面端口 (Dart 引擎调用; 无需实例已存在). */
        fun registerEngineWaiterCompat(result: MethodChannel.Result) {
            if (tunBridgePort > 0 && forwardPort > 0) {
                Handler(android.os.Looper.getMainLooper()).post {
                    completeEngineWaiterCompat(result)
                }
            } else {
                engineWaiters.add(result)
            }
        }

        /** 端口就绪后完成所有等待者. */
        fun flushEngineWaitersCompat() {
            if (tunBridgePort <= 0 || forwardPort <= 0) return
            val copy = synchronized(engineWaiters) {
                val c = engineWaiters.toList()
                engineWaiters.clear()
                c
            }
            for (w in copy) {
                Handler(android.os.Looper.getMainLooper()).post {
                    completeEngineWaiterCompat(w)
                }
            }
        }

        /** 启动失败: 以错误完成所有等待者. */
        fun failEngineWaitersCompat(message: String) {
            val copy = synchronized(engineWaiters) {
                val c = engineWaiters.toList()
                engineWaiters.clear()
                c
            }
            for (w in copy) {
                Handler(android.os.Looper.getMainLooper()).post {
                    w.error("START_FAILED", message, null)
                }
            }
        }

        private fun completeEngineWaiterCompat(result: MethodChannel.Result) {
            if (tunBridgePort > 0 && forwardPort > 0) {
                result.success(mapOf(
                    "tunBridgePort" to tunBridgePort,
                    "forwardPort" to forwardPort,
                    "address" to tunAddress,
                ))
            } else {
                result.error("NOT_READY", "引擎端口未就绪", null)
            }
        }
    }

    private val workerThread = HandlerThread("aproxy-worker").apply { start() }
    private val worker = Handler(workerThread.looper)
    private val mainHandler = Handler(android.os.Looper.getMainLooper())

    @Volatile
    private var started = false

    private var tun: ParcelFileDescriptor? = null
    private var bridgeServer: ServerSocket? = null
    private var forwardServer: ServerSocket? = null

    // 有界线程池 + 信号量限流, 避免大量并发 DNS/TCP 请求导致的线程与内存爆炸 (OOM).
    // bridge 需要至少 2 线程: 一个 accept, 其余跑 bridgeLoop (避免单线程下
    // accept 循环占满而 bridgeLoop 永远排不上, 导致 TUN 数据面不搬运).
    private val bridgeExecutor: ExecutorService = Executors.newFixedThreadPool(8)
    // 每条隧道占用 1 个执行线程 (主循环阻塞在 upIn.read) + 1 个 toUp 线程;
    // 手机后台 app 可同时保持数百条连接, 池太小会导致新转发任务排队,
    // Dart 侧 ack 15s 超时报"转发器连接超时". 因此放宽到 1024/2048.
    private val forwardExecutor: ExecutorService = Executors.newFixedThreadPool(1024)
    private val forwardSemaphore = java.util.concurrent.Semaphore(2048)

    override fun onCreate() {
        super.onCreate()
        instance = this
        // 若此前已有端口 (进程内复用), 立即完成等待者.
        if (tunBridgePort > 0 && forwardPort > 0) flushEngineWaitersCompat()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_STOP -> worker.post { teardownAndStop() }
            ACTION_START -> {
                val config = intent.getStringExtra(EXTRA_CONFIG) ?: ""
                val port = intent.getIntExtra(EXTRA_LAUNCHER_PORT, 0)
                startForegroundCompat()
                worker.post { startVpn(config, port) }
            }
            ACTION_RESTART -> {
                val config = intent.getStringExtra(EXTRA_CONFIG) ?: ""
                val port = intent.getIntExtra(EXTRA_LAUNCHER_PORT, 0)
                startForegroundCompat()
                worker.post { restartVpn(config, port) }
            }
        }
        return START_NOT_STICKY
    }

    private fun startForegroundCompat() {
        val manager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        val channel = NotificationChannel(CHANNEL_ID, "aproxy", NotificationManager.IMPORTANCE_LOW)
        manager.createNotificationChannel(channel)
        val contentIntent = packageManager.getLaunchIntentForPackage(packageName)
        val pending = PendingIntent.getActivity(
            this, 0, contentIntent,
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )
        val notification: Notification = NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("aproxy")
            .setContentText("VPN 运行中")
            .setSmallIcon(R.drawable.ic_vpn)
            .setContentIntent(pending)
            .setOngoing(true)
            .build()
        startForeground(NOTIFY_ID, notification)
    }

    private fun restartVpn(configJson: String, launcherPort: Int) {
        teardown()
        startVpn(configJson, launcherPort)
    }

    private fun startVpn(configJson: String, launcherPort: Int) {
        if (started) teardown()
        try {
            android.util.Log.i("aproxy-vpn", "startVpn: launcherPort=$launcherPort")
            val cfg = JSONObject(configJson)
            val myAddress = cfg.optString("tunAddress", "")
            val myPrefix = if (cfg.has("tunPrefix")) cfg.getInt("tunPrefix") else 0
            val mtu = if (cfg.has("tunMtu")) cfg.getInt("tunMtu") else 1500
            val bypassCn = cfg.optBoolean("bypassCn", false)
            val (address, prefix) = resolveTun(myAddress, myPrefix)

            val builder = Builder()
            builder.setSession("aproxy")
            builder.addAddress(address, prefix)
            if (bypassCn) {
                // 绕过中国大陆: 仅把非中国段接入 VPN, 中国/私有段由系统直连.
                val routes = cfg.optJSONArray("tunRoutes")
                var added = 0
                if (routes != null) {
                    for (i in 0 until routes.length()) {
                        val r = routes.optString(i, "")
                        val slash = r.indexOf('/')
                        if (slash > 0) {
                            val plen = r.substring(slash + 1).toIntOrNull()
                            if (plen != null && plen in 0..32) {
                                builder.addRoute(r.substring(0, slash), plen)
                                added++
                            }
                        }
                    }
                }
                if (added == 0) builder.addRoute("0.0.0.0", 0) // 路由缺失回退全隧道.
                // IPv6 无法按国别细分, 不加入 VPN, 保持系统直连.
            } else {
                // 默认全隧道 (IPv4 + IPv6).
                builder.addRoute("0.0.0.0", 0)
                builder.addRoute("::", 0)
            }
            // 本应用自身流量必须绕过 VPN (转发器出站走物理网络), 否则其 socket
            // 会回环进 TUN. 部分 ROM 上 VpnService.protect 的放行标记不生效
            // (SYN/UDP 发出后应答被内核丢弃), 因此直接禁止本应用走 VPN.
            builder.addDisallowedApplication(packageName)
            // 固定注入 DNS 保证所有查询进入 TUN, 由 Dart 引擎按 qname 分流.
            builder.addDnsServer("8.8.8.8")
            builder.addDnsServer("1.1.1.1")
            if (mtu > 0) builder.setMtu(mtu)
            val underlying = underlyingNetworks()
            android.util.Log.w("aproxy-tun", "underlying: ${underlying.joinToString { it.toString() }}")
            if (underlying.isNotEmpty()) builder.setUnderlyingNetworks(underlying.toTypedArray())
            builder.setBlocking(true)

            val pfd = builder.establish() ?: throw IllegalStateException("VpnService establish 失败")
            tun = pfd
            tunAddress = address
            android.util.Log.i("aproxy-vpn", "establish 成功, address=$address bridge=${tunBridgePort}")

            startBridgeServer()
            startForwardServer()
            android.util.Log.i("aproxy-vpn", "桥接端口=$tunBridgePort 转发端口=$forwardPort")

            started = true
            AproxyEvents.emitVpnState("running")
            // 端口就绪后通知所有等待中的引擎调用方.
            flushEngineWaitersCompat()
        } catch (e: Exception) {
            android.util.Log.e("aproxy-vpn", "start failed", e)
            AproxyEvents.emitVpnState("error", e.message ?: e.toString())
            // 以错误完成所有等待中的引擎调用方，避免 Dart 侧永久等待.
            failEngineWaitersCompat(e.message ?: e.toString())
            teardownAndStop()
        }
    }

    private fun resolveTun(addr: String, prefix: Int): Pair<String, Int> =
        if (addr.trim().isNotEmpty() && prefix > 0) addr.trim() to prefix else "10.0.0.2" to 24

    private fun startBridgeServer() {
        val ss = ServerSocket(0, 4, java.net.InetAddress.getByName("127.0.0.1"))
        bridgeServer = ss
        tunBridgePort = ss.localPort
        bridgeExecutor.execute {
            try {
                while (started && !ss.isClosed) {
                    val client = ss.accept()
                    bridgeExecutor.execute { bridgeLoop(client) }
                }
            } catch (_: Exception) {
            }
        }
    }

    private fun bridgeLoop(client: Socket) {
        var din: DataInputStream? = null
        var dout: DataOutputStream? = null
        try {
            client.tcpNoDelay = true
            din = DataInputStream(client.getInputStream())
            dout = DataOutputStream(client.getOutputStream())

            // TUN -> Dart: 持续读 fd 并帧化推送.
            val reader = Thread {
                try {
                    val fd = tun?.fileDescriptor ?: return@Thread
                    val tin = FileInputStream(fd)
                    val buf = ByteArray(65536)
                    while (started && !client.isClosed) {
                        val n = tin.read(buf)
                        if (n < 0) break
                        synchronized(dout) {
                            dout.writeInt(n)
                            dout.write(buf, 0, n)
                        }
                        dout.flush()
                    }
                } catch (_: Exception) {
                }
            }
            reader.start()

            // Dart -> TUN: 读帧并写入 fd.
            val fd = tun?.fileDescriptor
            if (fd != null) {
                val tout = FileOutputStream(fd)
                while (started && !client.isClosed) {
                    val n = try { din.readInt() } catch (_: EOFException) { break }
                    if (n <= 0 || n > 65536) break
                    val buf = ByteArray(n)
                    din.readFully(buf)
                    tout.write(buf, 0, n)
                    tout.flush()
                }
            }
            try { reader.interrupt() } catch (_: Exception) {}
        } catch (_: Exception) {
        } finally {
            try { din?.close() } catch (_: Exception) {}
            try { dout?.close() } catch (_: Exception) {}
            try { client.close() } catch (_: Exception) {}
        }
    }

    /** 受保护转发器: Dart 连入, 首帧为目标 "host:port"(长度帧), 随后双向转发. */
    private fun startForwardServer() {
        val ss = ServerSocket(0, 2048, java.net.InetAddress.getByName("127.0.0.1"))
        forwardServer = ss
        forwardPort = ss.localPort
        forwardExecutor.execute {
            try {
                while (started && !ss.isClosed) {
                    val client = ss.accept()
                    // 并发受限: 信号量满则直接丢弃, 避免线程/内存爆炸.
                    if (!forwardSemaphore.tryAcquire()) {
                        try { client.close() } catch (_: Exception) {}
                        continue
                    }
                    forwardExecutor.execute {
                        try {
                            handleForward(client)
                        } finally {
                            forwardSemaphore.release()
                        }
                    }
                }
            } catch (_: Exception) {
            }
        }
    }

    private val UDP_MAGIC = -1

    /**
     * UDP 转发 (受保护): 读 host/port/datagram, 经底层网络发一个 UDP 包,
     * 收一个应答后帧化返回。用于国内 DNS(UDP 53) 等单包请求-应答场景。
     * 协议: int32(UDP_MAGIC) + int32(hostLen)+host + int32(port) +
     *        int32(qLen)+query; 返回 int32(respLen)+resp。
     */
    private fun handleForwardUdp(din: DataInputStream, dout: DataOutputStream) {
        try {
            val hostLen = din.readInt()
            if (hostLen <= 0 || hostLen > 512) { dout.writeInt(0); return }
            val hostBytes = ByteArray(hostLen)
            din.readFully(hostBytes)
            val host = String(hostBytes, Charsets.UTF_8)
            val port = din.readInt()
            if (host.isEmpty() || port !in 1..65535) { dout.writeInt(0); return }
            val qLen = din.readInt()
            if (qLen <= 0 || qLen > 4096) { dout.writeInt(0); return }
            val query = ByteArray(qLen)
            din.readFully(query)
            // 创建未绑定 socket, 显式绑定到底层物理网络的 IPv4 地址. 不能直接用
            // 无参构造(默认绑到 "::" IPv6 通配), 否则 protect 放行的 UDP 报文
            // 在部分 ROM 上路由/应答异常, 导致 DNS 查询超时.
            val u = java.net.DatagramSocket(null)
            val cm2 = getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
            val underlyingNet = bestUnderlyingNetwork()
            val linkProps = underlyingNet?.let { cm2.getLinkProperties(it) }
            var localIp: java.net.InetAddress? = null
            if (linkProps != null) {
                localIp = linkProps.linkAddresses
                    .map { it.address }
                    .firstOrNull { a -> a is java.net.Inet4Address && !a.isLoopbackAddress }
            }
            var bindOk = false
            if (localIp != null) {
                try {
                    u.bind(java.net.InetSocketAddress(localIp, 0))
                    bindOk = true
                    android.util.Log.i("aproxy-fwd", "[UDP] 已绑定本地 $localIp")
                } catch (e: Exception) {
                    android.util.Log.w("aproxy-fwd", "[UDP] bind本地$localIp 失败: $e")
                }
            }
            if (!bindOk) {
                try {
                    u.bind(java.net.InetSocketAddress("0.0.0.0", 0))
                } catch (_: Exception) {}
            }
            val addr = java.net.InetAddress.getByName(host)
            // connected UDP: 固定远端地址与源地址, 应答稳定回到本 socket.
            // 必须先 connect 再 protect: connect 可能重建底层 fd, protect 若
            // 在此之前调用会标记失效; 也不能在 protect 后 bindSocket/bindProcess,
            // 它们会覆盖 protect 打上的放行标记(否则包仍进 TUN 被丢弃).
            u.connect(addr, port)
            var protectOk = false
            try {
                protectOk = protect(u)
            } catch (_: Exception) {}
            if (!protectOk) {
                val fdInt = FdProtect.datagramFdInt(u)
                protectOk = fdInt >= 0 && FdProtect.protectFd(this, fdInt)
            }
            android.util.Log.i("aproxy-fwd", "[UDP] protect=$protectOk 目标=$host:$port 本地=${u.localSocketAddress}")
            if (!protectOk) throw java.io.IOException("UDP protect rejected")
            // 部分 ROM 的 protect 放行表为空, SO_MARK 打上后报文无路由被丢弃,
            // 导致 UDP DNS 查询无应答. 额外把 socket 绑定到底层物理网络,
            // 以网络句柄 fwmark 覆盖 protect 的标记, 走真实网卡收发.
            bindDatagramToNetwork(u, "UDP")
            u.soTimeout = 5000
            u.send(java.net.DatagramPacket(query, query.size))
            val buf = ByteArray(4096)
            val pkt = java.net.DatagramPacket(buf, buf.size)
            u.receive(pkt)
            val resp = pkt.data.copyOf(pkt.length)
            android.util.Log.i("aproxy-fwd", "[UDP] $host:$port 应答 ${resp.size} 字节")
            u.close()
            dout.writeInt(resp.size)
            dout.write(resp)
            dout.flush()
        } catch (e: Exception) {
            android.util.Log.e("aproxy-fwd", "UDP 转发器异常: $e")
            try { dout.writeInt(0); dout.flush() } catch (_: Exception) {}
        }
    }

    private fun handleForward(client: Socket) {
        var upstream: Socket? = null
        try {
            val din = DataInputStream(client.getInputStream())
            val dout = DataOutputStream(client.getOutputStream())
            val mode = din.readInt()
            // UDP 请求模式: 首帧为 UDP_MAGIC, 随后 host / port / datagram.
            if (mode == UDP_MAGIC) {
                handleForwardUdp(din, dout)
                return
            }
            if (mode <= 0 || mode > 512) {
                dout.writeInt(0)
                return
            }
            val hostLen = mode
            val target = ByteArray(hostLen)
            din.readFully(target)
            val dst = String(target, Charsets.UTF_8)
            val sep = dst.lastIndexOf(':')
            val host = if (sep > 0) dst.substring(0, sep) else dst
            val port = if (sep > 0) dst.substring(sep + 1).toIntOrNull() ?: 0 else 0
            if (host.isEmpty() || port !in 1..65535) {
                dout.writeInt(0)
                return
            }

            android.util.Log.i("aproxy-fwd", "[转发] 请求连接目标=$host:$port")
            // 与原版 C++ 等价的方式: 创建真实 socket → 取底层 fd → protect(fd) → connect.
            // 直接保护本 socket 的底层 fd; protect 后绝不调用 bindSocket/bindProcessNetwork,
            // 以免其覆盖 protect 打上的排除标记(否则 SYN 回环进 tun 卡死)。
            val sock = Socket()
            sock.tcpNoDelay = true
            // 不预先 bind 本地地址, 让 fd 与 connect 使用的 fd 保持一致。
            val sockFd = FdProtect.socketFd(sock)
            val fdInt = if (sockFd != null) FdProtect.fdInt(sockFd) else -1
            val protectOk = if (fdInt >= 0) {
                val ok = FdProtect.protectFd(this, fdInt)
                android.util.Log.i(
                    "aproxy-fwd", "[保护] protect(int=$fdInt) 返回=$ok 目标=$host:$port")
                ok
            } else {
                val ok = protect(sock) == true
                android.util.Log.w(
                    "aproxy-fwd", "[保护] 回退 protect(Socket)=$ok 目标=$host:$port")
                ok
            }
            if (!protectOk) throw java.io.IOException("protect rejected")
            // 同上: 部分 ROM protect 后 SYN 无路由被丢弃, 绑定底层物理网络
            // 让连接走真实网卡 (bindSocket 的 fwmark 指向物理网络, 同样绕过 VPN).
            bindSocketToNetwork(sock, "TCP")
            android.util.Log.i("aproxy-fwd", "[转发] 开始 connect 目标=$host:$port")
            val t0 = System.currentTimeMillis()
            val inetAddr = try {
                java.net.InetAddress.getByName(host)
            } catch (e: Exception) {
                android.util.Log.e(
                    "aproxy-fwd", "[转发] 解析 $host 失败: $e (耗时 ${System.currentTimeMillis()-t0}ms)")
                throw e
            }
            android.util.Log.i(
                "aproxy-fwd",
                "[转发] 解析 $host -> ${inetAddr.hostAddress} (耗时 ${System.currentTimeMillis()-t0}ms)")
            sock.connect(java.net.InetSocketAddress(inetAddr, port), 15000)
            android.util.Log.i("aproxy-fwd", "[转发] 已连上游 $host:$port")
            dout.writeInt(1)
            dout.flush()
            upstream = sock

            val upIn = sock.getInputStream()
            val upOut = sock.getOutputStream()
            // client -> upstream.
            val toUp = Thread {
                try {
                    val buf = ByteArray(16384)
                    while (true) {
                        val n = din.read(buf)
                        if (n < 0) break
                        upOut.write(buf, 0, n)
                        upOut.flush()
                    }
                } catch (e: Exception) {
                    android.util.Log.w("aproxy-fwd", "[转发] client read err: $e")
                } finally {
                    // 客户端断开后必须关闭上游, 否则主线程阻塞在 upIn.read()
                    // 永不退出, 连接与信号量泄漏 (占满后所有新转发被拒).
                    try { sock.close() } catch (_: Exception) {}
                }
            }
            toUp.start()
            // upstream -> client (原始字节流; Dart 侧直接在流上做 TLS/HTTP 握手,
            // 若帧化则 TLS 层会读到长度帧头导致握手失败).
            val buf = ByteArray(16384)
            while (true) {
                val n = upIn.read(buf)
                if (n < 0) break
                dout.write(buf, 0, n)
                dout.flush()
            }
            try { toUp.interrupt() } catch (_: Exception) {}
        } catch (e: Exception) {
            android.util.Log.e("aproxy-fwd", "转发器异常: $e");
        } finally {
            try { upstream?.close() } catch (_: Exception) {}
            try { client.close() } catch (_: Exception) {}
        }
    }

    private fun underlyingNetworks(): List<Network> {
        val cm = getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        return cm.allNetworks.filter { n ->
            val caps = cm.getNetworkCapabilities(n) ?: return@filter false
            !caps.hasTransport(NetworkCapabilities.TRANSPORT_VPN) &&
                cm.getNetworkInfo(n)?.isConnected == true
        }
    }

    /**
     * 获取最佳底层物理网络 (排除 VPN), 优先取当前"默认/活跃"网络.
     * 用于 [bindSocketToNetwork] 强制把出站 socket 绑定到真实网络,
     * 从而绕过 fwmark/受保护路由表 (部分 ROM 该表为空会导致 protect 后 SYN 卡死).
     */
    private fun bestUnderlyingNetwork(): Network? {
        val cm = getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        // 优先选当前"活跃/默认"底层物理网络(非 VPN)。
        val active = cm.activeNetwork
        if (active != null) {
            val caps = cm.getNetworkCapabilities(active) ?: return null
            if (!caps.hasTransport(NetworkCapabilities.TRANSPORT_VPN)) return active
        }
        // 兜底: 任一已连接且有 INTERNET 的非 VPN 网络。
        return cm.allNetworks.firstOrNull { n ->
            val caps = cm.getNetworkCapabilities(n) ?: return@firstOrNull false
            !caps.hasTransport(NetworkCapabilities.TRANSPORT_VPN) &&
                caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET) &&
                cm.getNetworkInfo(n)?.isConnected == true
        }
    }

    /** 把出站 socket 显式绑定到底层物理网络, 避免被 TUN 回环. */
    private fun bindSocketToNetwork(sock: Socket, tag: String): Network? {
        try {
            val n = bestUnderlyingNetwork()
            if (n == null) {
                android.util.Log.w("aproxy-fwd", "[$tag] 无底层网络可绑定")
                return null
            }
            n.bindSocket(sock)
            android.util.Log.i("aproxy-fwd", "[$tag] 已绑定底层网络=$n")
            return n
        } catch (e: Exception) {
            android.util.Log.w("aproxy-fwd", "[$tag] 绑定底层网络失败: $e")
            return null
        }
    }

    /** 把出站 UDP socket 显式绑定到底层物理网络, 避免被 TUN 回环或 protect 标记丢包. */
    private fun bindDatagramToNetwork(
        u: java.net.DatagramSocket, tag: String): Network? {
        try {
            val n = bestUnderlyingNetwork()
            if (n == null) {
                android.util.Log.w("aproxy-fwd", "[$tag] 无底层网络可绑定")
                return null
            }
            n.bindSocket(u)
            android.util.Log.i("aproxy-fwd", "[$tag] 已绑定底层网络=$n")
            return n
        } catch (e: Exception) {
            android.util.Log.w("aproxy-fwd", "[$tag] 绑定底层网络失败: $e")
            return null
        }
    }

    private fun teardown() {
        started = false
        try { bridgeServer?.close() } catch (_: Exception) {}
        try { forwardServer?.close() } catch (_: Exception) {}
        try { tun?.close() } catch (_: Exception) {}
        tun = null
        tunBridgePort = 0
        forwardPort = 0
    }

    private fun teardownAndStop() {
        teardown()
        stopForeground(true)
        stopSelf()
    }

    override fun onDestroy() {
        if (instance === this) instance = null
        worker.post {
            teardown()
            bridgeExecutor.shutdownNow()
            forwardExecutor.shutdownNow()
            workerThread.quitSafely()
        }
        super.onDestroy()
    }
}
