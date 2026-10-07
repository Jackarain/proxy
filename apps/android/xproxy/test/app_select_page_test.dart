import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:xproxy/pages/app_select_page.dart';

const MethodChannel _channel = MethodChannel('com.jackarain.xproxy/vpn');

const List<Map<String, Object>> _apps = [
  {
    'package': 'com.android.chrome',
    'label': 'Chrome',
    'system': false,
    'uid': 10001,
  },
  {
    'package': 'org.mozilla.firefox',
    'label': 'Firefox',
    'system': false,
    'uid': 10002,
  },
  {
    'package': 'com.example.weixin',
    'label': '微信',
    'system': false,
    'uid': 10003,
  },
  {
    'package': 'com.android.settings',
    'label': '系统设置',
    'system': true,
    'uid': 1000,
  },
];

void main() {
  void mockListApps(Future<Object?> Function() handler) {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(_channel, (call) async {
          if (call.method == 'list_apps') return handler();
          return null;
        });
  }

  setUp(() {
    mockListApps(() async => _apps);
  });

  tearDown(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(_channel, null);
  });

  Future<void> openPage(
    WidgetTester tester, {
    List<String> selected = const [],
    List<List<String>?>? results,
  }) async {
    await tester.pumpWidget(
      MaterialApp(
        home: Builder(
          builder:
              (ctx) => TextButton(
                onPressed: () async {
                  final picked = await Navigator.of(ctx).push<List<String>>(
                    MaterialPageRoute(
                      builder: (_) => AppSelectPage(selected: selected),
                    ),
                  );
                  results?.add(picked);
                },
                child: const Text('打开'),
              ),
        ),
      ),
    );
    await tester.tap(find.text('打开'));
    await tester.pumpAndSettle();
  }

  testWidgets('默认隐藏系统应用, 可切换显示', (tester) async {
    await openPage(tester);

    expect(find.text('Chrome'), findsOneWidget);
    expect(find.text('微信'), findsOneWidget);
    expect(find.text('系统设置'), findsNothing);

    await tester.tap(find.text('显示系统应用'));
    await tester.pumpAndSettle();
    expect(find.text('系统设置'), findsOneWidget);
  });

  testWidgets('已选中的系统应用始终可见', (tester) async {
    await openPage(tester, selected: ['com.android.settings']);

    expect(find.text('系统设置'), findsOneWidget);
    expect(find.text('选择应用 (1)'), findsOneWidget);
  });

  testWidgets('搜索按名称或包名过滤', (tester) async {
    await openPage(tester);

    await tester.enterText(find.byType(TextField), 'fire');
    await tester.pumpAndSettle();
    expect(find.text('Firefox'), findsOneWidget);
    expect(find.text('Chrome'), findsNothing);

    await tester.enterText(find.byType(TextField), 'com.example');
    await tester.pumpAndSettle();
    expect(find.text('微信'), findsOneWidget);
    expect(find.text('Firefox'), findsNothing);
  });

  testWidgets('全选当前列表与清空', (tester) async {
    await openPage(tester);

    await tester.enterText(find.byType(TextField), 'com.android');
    await tester.pumpAndSettle();
    await tester.tap(find.text('全选当前列表'));
    await tester.pumpAndSettle();
    expect(find.text('选择应用 (1)'), findsOneWidget);

    await tester.enterText(find.byType(TextField), '');
    await tester.pumpAndSettle();
    await tester.tap(find.text('清空'));
    await tester.pumpAndSettle();
    expect(find.text('选择应用 (0)'), findsOneWidget);
  });

  testWidgets('选择后点完成返回包名列表', (tester) async {
    final results = <List<String>?>[];
    await openPage(tester, results: results);

    await tester.tap(find.text('Chrome'));
    await tester.pumpAndSettle();
    expect(find.text('选择应用 (1)'), findsOneWidget);

    await tester.tap(find.text('完成'));
    await tester.pumpAndSettle();
    expect(results.single, ['com.android.chrome']);
  });

  testWidgets('应用列表读取失败可重试', (tester) async {
    var fail = true;
    mockListApps(() async {
      if (fail) {
        throw PlatformException(code: 'LIST_APPS_FAILED', message: 'native 失败');
      }
      return _apps;
    });

    await openPage(tester);
    expect(find.textContaining('读取应用列表失败'), findsOneWidget);

    fail = false;
    await tester.tap(find.text('重试'));
    await tester.pumpAndSettle();
    expect(find.text('Chrome'), findsOneWidget);
  });
}
