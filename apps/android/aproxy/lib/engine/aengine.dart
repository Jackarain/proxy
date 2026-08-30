import 'dart:async';
import 'dart:convert';

import 'package:flutter/services.dart';

import 'agent.dart';
import 'dns.dart';
import 'forwarder.dart';
import 'logging.dart';
import 'route_table.dart';
import 'tunnel_engine.dart';
import 'tun_bridge.dart';
import 'upstream.dart';
import '../models/vpn_config.dart';

export 'logging.dart' show EngineLog;

/// 纯 Dart 代理引擎。
///
/// 编排数据面各组件:
///   控制的 Kotlin VpnService (TUN 数据面 + 受保护转发器)
///   -> [TunBridge] 收 TUN 原始 IP 包 -> [TunnelEngine] 分流 + TCP/DNS
///   -> [Forwarder] + [UpstreamClient] 出站 (SOCKS5/HTTP/直连, 可 TLS)
///   控制 [EngineAgent] 以 WS 连接控制端上报 register/status/log。
///
/// 生命周期: [start] 连接数据面与控制通道, [stop] 关闭全部。
class DartEngine {
  DartEngine._();
  static final DartEngine instance = DartEngine._();

  final EngineLog log = EngineLog();
  EngineAgent? _agent;
  TunBridge? _bridge;
  TunnelEngine? _tunnel;
  Forwarder? _forwarder;
  UpstreamClient? _upstream;
  RouteTable? _routeTable;
  String _buildVersion = 'dart-1.0.0';
  int _launcherPort = 0;
  bool _running = false;
  bool _stopping = false;

  static const MethodChannel _nativeChannel =
      MethodChannel('com.jackarain.aproxy/vpn');

  /// 获取原生构建版本标识 (构建时注入的 git commit hash); 通道不可用时回退.
  Future<String> _loadBuildVersion() async {
    try {
      final v = await _nativeChannel.invokeMethod<String>('build_version');
      if (v != null && v.isNotEmpty) return v;
    } catch (_) {}
    return 'dart-1.0.0';
  }

  bool get running => _running;

  /// 启动引擎。
  ///
  /// [configJson] 为 VpnConfig.toJson 快照; [launcherPort] 为控制端端口;
  /// [tunBridgePort]/[forwardPort] 来自 Kotlin VpnService。
  Future<void> start({
    required String configJson,
    required int launcherPort,
    required int tunBridgePort,
    required int forwardPort,
    List<String> cnCidrs = const [],
  }) async {
    _stopping = false;
    _launcherPort = launcherPort;
    _buildVersion = await _loadBuildVersion();
    final cfg = VpnConfig.fromJson(
      jsonDecode(configJson) as Map<String, dynamic>,
    );

    _routeTable = RouteTable(
      proxyDomains: cfg.proxyDomains,
      proxyCidr: cfg.proxyCidr,
      bypassCn: cfg.bypassCn,
      hasProxy: cfg.proxyPass.trim().isNotEmpty,
    );
    if (cfg.bypassCn && cnCidrs.isNotEmpty) {
      _routeTable!.setCnCidrs(cnCidrs);
    }

    _forwarder = Forwarder(forwardPort: forwardPort, log: log);
    _upstream = UpstreamClient(
      forwarder: _forwarder!,
      proxyPass: cfg.proxyPass,
      poolSize: cfg.proxyPassPoolSize,
      disableCheckCert: cfg.disableCheckCert,
      log: log,
    );

    final dnsConfig = DnsConfig(
      proxyPass: cfg.proxyPass,
      domesticDns: cfg.dns,
      foreignDns: cfg.dnsForeign,
      foreignDoh: cfg.dnsForeignDoh,
      proxyDomains: cfg.proxyDomains,
      noIpv6: cfg.noIpv6,
      cacheSize: cfg.dnsCache ? cfg.dnsCacheSize : 0,
      cacheTtlMin: cfg.dnsCacheTtl,
    );
    _tunnel = TunnelEngine(
      log: log,
      routeTable: _routeTable!,
      upstream: _upstream!,
      forwarder: _forwarder!,
      dnsConfig: dnsConfig,
      disableCheckCert: cfg.disableCheckCert,
      tunMtu: cfg.tunMtu,
    );

    // 让上游解析 proxy_pass 域名时复用引擎的 DnsResolver。
    _tunnel!.wireUpstreamResolver(_upstream!);
    // 启动连接池: 预建到上游代理的 TCP(+TLS) 连接, 有代理需求时免握手.
    _upstream!.startPool();
    // 预热 proxy_pass 自身域名解析, 避免启动瞬间数十个 DoH 并发解析打满
    // 转发器连接池 (每次 DoH 都要先解析 proxy_pass 域名).
    if (parseProxyPass(cfg.proxyPass)?.host case final proxyHost?) {
      unawaited(_tunnel!.resolveHostToIp(proxyHost));
    }

    final pp = parseProxyPass(cfg.proxyPass);
    log.log('【引擎启动】launcher端口=$_launcherPort 数据面=$tunBridgePort '
        '转发=$forwardPort', level: 1);
    if (pp == null) {
      log.log('【配置告警】proxy_pass 解析失败: ${cfg.proxyPass}', level: 2);
    } else {
      log.log('【代理】类型=${pp.type} 主机=${pp.host} 端口=${pp.port} '
          '认证=${pp.user.isNotEmpty ? '启用' : '无'}', level: 1);
    }
    log.log('【DNS】国内=${cfg.dns.join(",")} 国外=${cfg.dnsForeign.join(",")} '
        'DoH=${cfg.dnsForeignDoh.isEmpty ? '未配置' : cfg.dnsForeignDoh} '
        '域名分流=${cfg.proxyDomains.join(",")}', level: 1);

    // 建立数据面桥接.
    _bridge = TunBridge(
      port: tunBridgePort,
      log: log,
      onPacket: (p) => _tunnel?.handlePacket(p),
    );
    _bridge!.onDisconnect = _onBridgeDisconnect;
    await _bridge!.connect();
    _tunnel!.sendPacketImpl = (p) => _bridge?.sendPacket(p);

    // 控制通道 agent.
    _agent = EngineAgent(
      launcherPort: launcherPort,
      instanceId: 'aproxy-${DateTime.now().millisecondsSinceEpoch}',
      log: log,
      tunnel: _tunnel!,
      version: _buildVersion,
      poolCount: () => _upstream?.poolCount ?? 0,
      onShutdown: () => unawaited(stop()),
      onApplyConfig: _applyConfig,
    );
    _agent!.onConnected = () => log.log('控制通道已连接', level: 1);
    _running = true;
    unawaited(_agent!.connect());
  }

  /// 桥接断开 (Kotlin 重启 TUN / 服务停止): 若仍应运行则重连.
  Future<void> _onBridgeDisconnect() async {
    log.log('数据面断开, 尝试重连', level: 2);
    if (!_running || _stopping) return;
    // 重新向 Kotlin 获取最新端口 (TUN 重建后 port 可能变化).
    await Future<void>.delayed(const Duration(seconds: 1));
    if (!_running || _stopping) return;
    log.log('等待 Kotlin 数据面端口...', level: 2);
  }

  /// 处理控制端下发的热更新配置 (set_config)。
  Future<void> _applyConfig(Map<String, dynamic> options) async {
    _upstream?.proxyPass = (options['proxy_pass'] as String? ?? '');
    _routeTable?.setDomainsAndCidr(
      _strList(options['proxy_domains']),
      _strList(options['proxy_cidr']),
    );
    log.log('配置已热更新', level: 1);
  }

  List<String> _strList(dynamic v) {
    if (v is List) return v.whereType<String>().toList();
    return [];
  }

  Future<void> stop() async {
    if (_stopping) return;
    _stopping = true;
    _running = false;
    _agent?.stop();
    _agent = null;
    _tunnel?.shutdown();
    _tunnel = null;
    _upstream?.close();
    _upstream = null;
    _bridge?.close();
    _bridge = null;
    _stopping = false;
  }
}
