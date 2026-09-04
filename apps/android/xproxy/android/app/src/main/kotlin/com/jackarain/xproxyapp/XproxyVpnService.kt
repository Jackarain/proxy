package com.jackarain.xproxyapp

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
import com.jackarain.xproxy.R
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat

/**
 * VpnService: 建立 VpnService tun 设备、放行对外 socket, 并持有
 * libxproxy.so 生命周期.
 *
 * 启动时先经 XproxyBridge 启动 libproxy (tun_wait_fd 模式, 无 tun),
 * 控制通道 WebSocket 连接建立后, Flutter 以用户配置的 tunAddress 调用
 * establishTun 在此建立 VpnService tun 并 detach fd, 再经控制通道
 * set_tun_fd 注入 libproxy. protect 同样经控制通道请求到达, 由本服务
 * 放行, 避免对外 socket 流量回环进 tun.
 *
 * 需要直通物理网络的 socket 一律经控制通道 protect (到上游代理/目标的
 * 连接). 其余流量按路由进入 tun 走 VPN 隧道.
 *
 * 启停均在专用工作线程执行: 避免阻塞主线程 (proxy 启动/停止涉及线程池
 * 创建与回收), 同时保证 START/STOP 串行处理, 不会并发操作同一实例.
 */
class XproxyVpnService : VpnService() {

    companion object {
        const val ACTION_START = "com.jackarain.xproxyapp.START"
        const val ACTION_RESTART = "com.jackarain.xproxyapp.RESTART"
        const val ACTION_STOP = "com.jackarain.xproxyapp.STOP"
        const val EXTRA_CONFIG = "config"
        const val EXTRA_LAUNCHER_PORT = "launcher_port"

        private const val CHANNEL_ID = "xproxy_vpn"
        private const val NOTIFY_ID = 1001

        /** 当前服务实例 (MainActivity 经 MethodChannel 调用 establishTun/protect). */
        @Volatile
        var instance: XproxyVpnService? = null
            private set

        fun startForegroundServiceCompat(context: Context, intent: Intent) {
            ContextCompat.startForegroundService(context, intent)
        }

        fun requestStop(context: Context) {
            context.startService(
                Intent(context, XproxyVpnService::class.java).setAction(ACTION_STOP)
            )
        }

        /** 服务实例代次: 每次新实例 onCreate 递增, 旧实例 onDestroy 据此判断是否让位. */
        @Volatile
        private var generation = 0L

        /** 停止完成回调: MainActivity 的 stop MethodChannel 据此在实例销毁
         *  (onDestroy) 后再返回, 使 Flutter 停止流程与 native teardown 同步. */
        @Volatile
        private var onStopComplete: (() -> Unit)? = null

        /** 注册停止完成回调; 已有未决回调时返回 false(并发 stop 兜底). */
        fun registerStopCallback(cb: () -> Unit): Boolean {
            if (onStopComplete != null) return false
            onStopComplete = cb
            return true
        }
    }


    private val workerThread = HandlerThread("xproxy-worker").apply { start() }
    private val worker = Handler(workerThread.looper)

    @Volatile
    private var started = false

    /** 本实例的代次: onCreate 时从 companion 领取, onDestroy 据此判断是否让位新实例. */
    private var myGeneration = 0L

    override fun onCreate() {
        super.onCreate()
        myGeneration = ++generation
        instance = this
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_STOP -> worker.post {
                teardownAndStop()
            }
            ACTION_RESTART -> {
                val config = intent.getStringExtra(EXTRA_CONFIG) ?: ""
                val port = intent.getIntExtra(EXTRA_LAUNCHER_PORT, 0)
                // 前台通知必须在 startForegroundService 后尽快发出 (主线程同步).
                startForegroundCompat()
                // 在单个工作线程任务内完成 停旧->启新, 避免 stopSelf 与 START
                // 交错导致服务被系统销毁 (进而误停新实例).
                worker.post { restartVpn(config, port) }
            }
            ACTION_START -> {
                val config = intent.getStringExtra(EXTRA_CONFIG) ?: ""
                val port = intent.getIntExtra(EXTRA_LAUNCHER_PORT, 0)
                // 前台通知必须在 startForegroundService 后尽快发出 (主线程同步).
                startForegroundCompat()
                worker.post { startVpn(config, port) }
            }
        }
        return START_NOT_STICKY
    }

    private fun startForegroundCompat() {
        val manager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        val channel = NotificationChannel(
            CHANNEL_ID, "proxy", NotificationManager.IMPORTANCE_LOW
        )
        manager.createNotificationChannel(channel)

        val contentIntent = packageManager.getLaunchIntentForPackage(packageName)
        val pending = PendingIntent.getActivity(
            this, 0, contentIntent,
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )
        val notification: Notification = NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("proxy")
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
        // 防御: 重复的 START 先停旧实例, 保证同一时刻只有一个.
        if (started) teardown()
        try {
            // 启动 proxy (tun_wait_fd 模式, 无 tun): 控制通道连接后
            // Flutter 建立 VpnService tun 再经 set_tun_fd 注入.
            val rc = XproxyBridge.start(configJson, launcherPort)
            if (rc != 0) {
                XproxyEvents.emitVpnState("error", "xproxy.start 失败: rc=$rc")
                teardownAndStop()
                return
            }
            started = true
            XproxyEvents.emitVpnState("running")
        } catch (e: Exception) {
            XproxyEvents.emitVpnState("error", e.message ?: e.toString())
            teardownAndStop()
        }
    }

    /**
     * 以用户配置的地址建立 VpnService tun, detach 返回 fd (由 libproxy
     * 持有并负责关闭). 地址/路由/MTU 在此一次性配置, 后续不可更改.
     *
     * @param address tun 地址 (来自 VpnConfig).
     * @param prefix  地址前缀.
     * @param mtu     tun MTU.
     * @param routes  需要接入 VPN 的路由 (为空时默认全隧道).
     * @param dns     DNS 服务器列表 (可为空).
     * @param session VPN 会话名称.
     * @return tun fd; 失败抛出异常.
     */
    fun establishTun(
        address: String,
        prefix: Int,
        mtu: Int,
        routes: List<String>,
        dns: List<String>,
        session: String,
    ): Int {
        val builder = Builder()
        builder.setSession(session.ifEmpty { "proxy" })
        builder.addAddress(address, prefix)
        if (routes.isNotEmpty()) {
            for (route in routes) {
                addRoute(builder, route)
            }
        } else {
            // 默认全隧道 (IPv4 + IPv6).
            builder.addRoute("0.0.0.0", 0)
            builder.addRoute("::", 0)
        }
        for (server in dns) {
            if (server.isNotBlank()) builder.addDnsServer(server.trim())
        }
        if (mtu > 0) builder.setMtu(mtu)
        // 指定底层物理网络: 使系统填充"排除 VPN"的路由表,
        // 否则 protectSocket 放行的连接无法路由 (SYN 卡住).
        val underlying = underlyingNetworks()
        android.util.Log.w("xproxy-tun", "underlying: ${underlying.joinToString { it.toString() }}")
        if (underlying.isNotEmpty()) {
            builder.setUnderlyingNetworks(underlying.toTypedArray())
        }
        builder.setBlocking(true)
        val fd = builder.establish()
            ?: throw IllegalStateException("VpnService establish 失败")
        return fd.detachFd()
    }

    /** 当前已连接的物理网络 (排除 VPN 自身), 供 setUnderlyingNetworks 使用. */
    private fun underlyingNetworks(): List<Network> {
        val cm = getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        return cm.allNetworks.filter { n ->
            val caps = cm.getNetworkCapabilities(n) ?: return@filter false
            !caps.hasTransport(NetworkCapabilities.TRANSPORT_VPN) &&
                cm.getNetworkInfo(n)?.isConnected == true
        }
    }

    /** 放行对外 socket (经控制通道 protect 请求到达), 避免回环进 tun. */
    fun protectSocket(fd: Int): Boolean = try {
        protect(fd)
    } catch (_: Throwable) {
        false
    }

    private fun addRoute(builder: Builder, cidr: String) {
        val parsed = parseCidr(cidr)
        if (parsed != null) {
            builder.addRoute(parsed.first, parsed.second)
        } else {
            builder.addRoute(cidr, 32)
        }
    }

    private fun parseCidr(cidr: String): Pair<String, Int>? {
        val slash = cidr.indexOf('/')
        if (slash < 0) return null
        val host = cidr.substring(0, slash).trim()
        val prefix = cidr.substring(slash + 1).trim().toIntOrNull() ?: return null
        if (host.isEmpty() || prefix < 0 || prefix > 128) return null
        return host to prefix
    }

    /** 停止 proxy 并释放资源; 幂等, 可重复调用. tun fd 由 libproxy 持有并关闭. */
    private fun teardown() {
        if (started) {
            // 代次检查: 快速 停止->再运行 时若已有新实例接管 (其 start 流程会
            // 停旧启新), 本实例不得再停 proxy, 否则会误停新实例刚启动的服务,
            // 表现为重新启动后 VPN 无法正常工作.
            if (myGeneration == generation) {
                try {
                    XproxyBridge.stop()
                } catch (_: Throwable) {
                    // 忽略停止时的异常.
                }
            }
            started = false
        }
    }

    private fun teardownAndStop() {
        teardown()
        stopForeground(true)
        stopSelf()
    }

    override fun onDestroy() {
        if (instance === this) instance = null
        // 通知 Flutter 停止已完成(实例已销毁): 取走回调并清空, 使下一次
        // 连接由新实例执行, 避免 establish_tun 时实例已销毁导致 TUN 失败.
        val cb = onStopComplete
        onStopComplete = null
        try {
            cb?.invoke()
        } catch (_: Throwable) {
        }
        // 注意: 这里不再 teardown() 停止 proxy. 复用同一服务实例快速
        // 启停时, 旧实例的 onDestroy 若 teardown 会误停队列中刚启动的
        // 新 proxy(started 指向新 proxy, 同实例代次未变), 表现为快速
        // 启停后 proxy 刚启动就被停, 控制通道永远连不上. proxy 的停止
        // 统一由显式 ACTION_STOP(teardownAndStop) 与下一次启动的防御性
        // teardown 负责, onDestroy 只回收工作线程.
        worker.post { workerThread.quitSafely() }
        super.onDestroy()
    }
}
