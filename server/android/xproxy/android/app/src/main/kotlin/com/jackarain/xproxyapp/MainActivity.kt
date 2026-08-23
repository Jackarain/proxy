package com.jackarain.xproxyapp

import android.Manifest
import android.app.Activity
import android.content.Intent
import android.content.pm.PackageManager
import android.net.VpnService
import android.os.Build
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.EventChannel
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
    companion object {
        private const val CHANNEL = "com.jackarain.xproxy/vpn"
        private const val EVENTS = "com.jackarain.xproxy/events"
        private const val REQ_VPN = 1001
        private const val REQ_NOTIFICATION = 1002
    }

    private var pendingPrepare: MethodChannel.Result? = null

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)

        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, CHANNEL)
            .setMethodCallHandler { call, result ->
                when (call.method) {
                    "prepare" -> handlePrepare(result)
                    "start" -> {
                        val config = call.argument<String>("config") ?: ""
                        val port = call.argument<Int>("launcherPort") ?: 0
                        handleStart(config, port, result)
                    }
                    "restart" -> {
                        val config = call.argument<String>("config") ?: ""
                        val port = call.argument<Int>("launcherPort") ?: 0
                        handleRestart(config, port, result)
                    }
                    "stop" -> {
                        XproxyVpnService.requestStop(this)
                        result.success(true)
                    }
                    // 控制通道 protect 请求: 放行 libproxy 的对外 socket.
                    "protect" -> {
                        val fd = call.argument<Int>("fd") ?: -1
                        val inst = XproxyVpnService.instance
                        val ok = inst?.protectSocket(fd) ?: false
                        android.util.Log.w(
                            "xproxy-protect",
                            "fd=$fd ok=$ok hasInstance=${inst != null}"
                        )
                        result.success(ok)
                    }
                    // 控制通道连接建立后: 以用户配置的地址建立 tun.
                    "establish_tun" -> {
                        val address = call.argument<String>("address") ?: ""
                        val prefix = call.argument<Int>("prefix") ?: 24
                        val mtu = call.argument<Int>("mtu") ?: 1400
                        val routes = call.argument<List<String>>("routes") ?: emptyList()
                        val dns = call.argument<List<String>>("dns") ?: emptyList()
                        val session = call.argument<String>("session") ?: "proxy"
                        val instance = XproxyVpnService.instance
                        if (instance == null) {
                            result.error("NO_SERVICE", "VpnService 未运行", null)
                        } else {
                            try {
                                result.success(
                                    instance.establishTun(
                                        address, prefix, mtu, routes, dns, session
                                    )
                                )
                            } catch (e: Exception) {
                                result.error("ESTABLISH_FAILED", e.message, null)
                            }
                        }
                    }
                    else -> result.notImplemented()
                }
            }

        EventChannel(flutterEngine.dartExecutor.binaryMessenger, EVENTS)
            .setStreamHandler(object : EventChannel.StreamHandler {
                override fun onListen(arguments: Any?, events: EventChannel.EventSink?) {
                    XproxyEvents.setSink(events)
                }

                override fun onCancel(arguments: Any?) {
                    XproxyEvents.setSink(null)
                }
            })
    }

    private fun handlePrepare(result: MethodChannel.Result) {
        requestNotificationPermissionIfNeeded()
        val intent = VpnService.prepare(this)
        if (intent == null) {
            // 已授权.
            result.success(true)
        } else {
            // 阻塞等待用户在系统授权弹窗中的选择.
            pendingPrepare = result
            startActivityForResult(intent, REQ_VPN)
        }
    }

    /** Android 13+ 请求通知权限, 保证前台 VPN 通知可见 (未授权不影响 VPN 本身). */
    private fun requestNotificationPermissionIfNeeded() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) return
        val granted = ContextCompat.checkSelfPermission(
            this, Manifest.permission.POST_NOTIFICATIONS
        ) == PackageManager.PERMISSION_GRANTED
        if (!granted) {
            ActivityCompat.requestPermissions(
                this, arrayOf(Manifest.permission.POST_NOTIFICATIONS), REQ_NOTIFICATION
            )
        }
    }

    @Suppress("DEPRECATION")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQ_VPN) {
            val ok = resultCode == Activity.RESULT_OK
            pendingPrepare?.success(ok)
            pendingPrepare = null
            XproxyEvents.emitVpnState(if (ok) "prepared" else "permission_denied")
        }
    }

    private fun sendServiceCommand(
        action: String, config: String, launcherPort: Int, result: MethodChannel.Result
    ) {
        if (config.isEmpty() || launcherPort <= 0) {
            result.error("BAD_ARGS", "config/launcherPort 缺失", null)
            return
        }
        try {
            val intent = Intent(this, XproxyVpnService::class.java).apply {
                this.action = action
                putExtra(XproxyVpnService.EXTRA_CONFIG, config)
                putExtra(XproxyVpnService.EXTRA_LAUNCHER_PORT, launcherPort)
            }
            XproxyVpnService.startForegroundServiceCompat(this, intent)
            result.success(true)
        } catch (e: Exception) {
            result.error("START_FAILED", e.message, null)
        }
    }

    private fun handleStart(config: String, launcherPort: Int, result: MethodChannel.Result) {
        sendServiceCommand(XproxyVpnService.ACTION_START, config, launcherPort, result)
    }

    private fun handleRestart(config: String, launcherPort: Int, result: MethodChannel.Result) {
        sendServiceCommand(XproxyVpnService.ACTION_RESTART, config, launcherPort, result)
    }
}
