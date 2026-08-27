import 'package:flutter/material.dart';

import 'pages/config_list_page.dart';

void main() {
  runApp(const AproxyApp());
}

class AproxyApp extends StatelessWidget {
  const AproxyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'proxy',
      theme: _buildTheme(Brightness.light),
      darkTheme: _buildTheme(Brightness.dark),
      home: const ConfigListPage(),
    );
  }

  /// 直角矩形风格主题 (保留层次与阴影).
  ThemeData _buildTheme(Brightness brightness) {
    final scheme = ColorScheme.fromSeed(
      seedColor: Colors.indigo,
      brightness: brightness,
    );
    final square = RoundedRectangleBorder(borderRadius: BorderRadius.zero);
    return ThemeData(
      colorScheme: scheme,
      useMaterial3: true,
      cardTheme: CardThemeData(
        shape: square,
        margin: const EdgeInsets.symmetric(vertical: 4),
      ),
      floatingActionButtonTheme: FloatingActionButtonThemeData(shape: square),
      dialogTheme: DialogThemeData(shape: square),
      snackBarTheme: SnackBarThemeData(
        behavior: SnackBarBehavior.floating,
        shape: square,
      ),
      inputDecorationTheme: InputDecorationTheme(
        border: const OutlineInputBorder(borderRadius: BorderRadius.zero),
        enabledBorder: const OutlineInputBorder(
          borderRadius: BorderRadius.zero,
        ),
        focusedBorder: const OutlineInputBorder(
          borderRadius: BorderRadius.zero,
        ),
      ),
      filledButtonTheme: FilledButtonThemeData(
        style: FilledButton.styleFrom(shape: square),
      ),
      outlinedButtonTheme: OutlinedButtonThemeData(
        style: OutlinedButton.styleFrom(shape: square),
      ),
      textButtonTheme: TextButtonThemeData(
        style: TextButton.styleFrom(shape: square),
      ),
      tabBarTheme: TabBarThemeData(
        indicatorSize: TabBarIndicatorSize.tab,
        indicator: BoxDecoration(
          color: scheme.primary,
          borderRadius: BorderRadius.zero,
        ),
        labelColor: scheme.onPrimary,
        unselectedLabelColor: scheme.onSurfaceVariant,
      ),
    );
  }
}
