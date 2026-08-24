package com.jackarain.xproxyapp

import com.jackarain.xproxy
import org.json.JSONArray
import org.json.JSONObject

/**
 * libxproxy.so 的 JNI 桥.
 *
 * 注意: System.loadLibrary 必须先于任何 com.jackarain.xproxy* 类的使用,
 * 因为 xproxyJNI 的静态初始化会调用 native swig_module_init.
 *
 * 与 libproxy 的交互以控制通道 WebSocket 为主 (日志/status/protect),
 * 本桥仅保留进程内必须直调的启动/停止.
 */
object XproxyBridge {
    init {
        System.loadLibrary("xproxy")
    }

    /**
     * 启动 proxy: 将 Flutter 下发的 UI 配置 (VpnConfig.toJson) 翻译为
     * libxproxy 配置后调用 xproxy.start. tun fd 不在启动时传入, 由控制
     * 通道连接建立后建立 VpnService tun 再经 set_tun_fd 注入.
     *
     * @param config 用户配置 json (VpnConfig.toJson, 含 VpnService 专用字段).
     * @param launcherPort 本地 JSON-RPC over WS 控制端端口.
     */
    fun start(config: String, launcherPort: Int): Int {
        val cfg = JSONObject(config)
        val out = JSONObject()
        out.put("proxy_pass", cfg.optString("proxyPass", ""))
        out.put("tun", true)
        out.put("tun_mtu", cfg.optInt("tunMtu", 1400))
        out.put("tun_wait_fd", true)
        if (cfg.has("proxyDomains")) {
            out.put("proxy_domains", cfg.optJSONArray("proxyDomains") ?: JSONArray())
        }
        if (cfg.has("proxyCidr")) {
            out.put("proxy_cidr", cfg.optJSONArray("proxyCidr") ?: JSONArray())
        }
        // DNS 分流: 国内 DNS 直连解析, 国外 DNS/DoH 经代理解析.
        if (cfg.has("dns")) {
            out.put("dns_domestic", cfg.optJSONArray("dns") ?: JSONArray())
        }
        if (cfg.has("dnsForeign")) {
            out.put("dns_foreign", cfg.optJSONArray("dnsForeign") ?: JSONArray())
        }
        val doh = cfg.optString("dnsForeignDoh", "").trim()
        if (doh.isNotEmpty()) {
            out.put("dns_doh", doh)
        }
        // DNS 查询结果缓存与禁用 IPv6 解析.
        if (cfg.optBoolean("dnsCache", true)) {
            out.put("dns_cache_size", 4096)
            out.put("dns_cache_ttl", 300)
        }
        out.put("dns_no_ipv6", cfg.optBoolean("noIpv6", true))
        out.put("disable_check_cert", cfg.optBoolean("disableCheckCert", true))
        out.put("udp_timeout", cfg.optInt("udpTimeout", 300))
        out.put("launcher_url", "ws://127.0.0.1:$launcherPort")
        return xproxy.start(out.toString())
    }

    fun stop() = xproxy.stop()
}
