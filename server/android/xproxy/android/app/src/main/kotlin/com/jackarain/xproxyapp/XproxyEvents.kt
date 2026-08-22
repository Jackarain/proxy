package com.jackarain.xproxyapp

import android.os.Handler
import android.os.Looper
import io.flutter.plugin.common.EventChannel

/** 将原生 VPN 状态事件转发给 Flutter EventChannel (日志已改由 WS 控制通道上报). */
object XproxyEvents {
    @Volatile
    private var sink: EventChannel.EventSink? = null

    private val mainHandler = Handler(Looper.getMainLooper())

    fun setSink(s: EventChannel.EventSink?) {
        sink = s
    }

    fun emitVpnState(state: String, message: String? = null) {
        post(mapOf(
            "type" to "vpn_state",
            "state" to state,
            "message" to (message ?: ""),
        ))
    }

    private fun post(data: Map<String, Any?>) {
        mainHandler.post { sink?.success(data) }
    }
}
