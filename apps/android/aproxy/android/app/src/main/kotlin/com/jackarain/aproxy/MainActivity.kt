package com.jackarain.aproxy

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
        private const val CHANNEL = "com.jackarain.aproxy/vpn"
        private const val EVENTS = "com.jackarain.aproxy/events"
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
                        sendServiceCommand(AproxyVpnService.ACTION_START, config, port, result)
                    }
                    "restart" -> {
                        val config = call.argument<String>("config") ?: ""
                        val port = call.argument<Int>("launcherPort") ?: 0
                        sendServiceCommand(AproxyVpnService.ACTION_RESTART, config, port, result)
                    }
                    "stop" -> {
                        AproxyVpnService.requestStop(this)
                        result.success(true)
                    }
                    // 返回 Dart 引擎版本标识 (替代原 C++ 编译常量).
                    "build_version" -> {
                        result.success(AproxyEngine.version())
                    }
                    // 控制通道建立后获取数据面桥接与受保护转发器端口.
                    "connect_engine" -> {
                        AproxyVpnService.registerEngineWaiterCompat(result)
                    }
                    "is_active" -> {
                        val active = AproxyVpnService.tunBridgePort > 0 &&
                            AproxyVpnService.forwardPort > 0
                        result.success(active)
                    }
                    else -> result.notImplemented()
                }
            }

        EventChannel(flutterEngine.dartExecutor.binaryMessenger, EVENTS)
            .setStreamHandler(object : EventChannel.StreamHandler {
                override fun onListen(arguments: Any?, events: EventChannel.EventSink?) {
                    AproxyEvents.setSink(events)
                }

                override fun onCancel(arguments: Any?) {
                    AproxyEvents.setSink(null)
                }
            })
    }

    private fun handlePrepare(result: MethodChannel.Result) {
        requestNotificationPermissionIfNeeded()
        val intent = VpnService.prepare(this)
        if (intent == null) {
            result.success(true)
        } else {
            pendingPrepare = result
            startActivityForResult(intent, REQ_VPN)
        }
    }

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
            AproxyEvents.emitVpnState(if (ok) "prepared" else "permission_denied")
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
            val intent = Intent(this, AproxyVpnService::class.java).apply {
                this.action = action
                putExtra(AproxyVpnService.EXTRA_CONFIG, config)
                putExtra(AproxyVpnService.EXTRA_LAUNCHER_PORT, launcherPort)
            }
            AproxyVpnService.startForegroundServiceCompat(this, intent)
            result.success(true)
        } catch (e: Exception) {
            result.error("START_FAILED", e.message, null)
        }
    }
}

/** 构建版本标识: 当前 git commit hash 前几位 (构建时经 BuildConfig 注入). */
object AproxyEngine {
    fun version(): String = BuildConfig.GIT_HASH
}
