import 'dart:convert';

import 'device_id.dart';

/// A previously contacted device, stored only on this phone.
class RecentConnection {
  const RecentConnection({
    required this.id,
    this.name = '',
    this.platform = '',
    this.alias = '',
    this.lastConnected,
    this.connectionCount = 0,
  });

  final String id;
  final String name;
  final String platform;
  final String alias;
  final DateTime? lastConnected;
  final int connectionCount;

  String get displayName {
    if (alias.isNotEmpty) return alias;
    if (name.isNotEmpty) return name;
    return DeviceId.format(id);
  }

  RecentConnection copyWith({String? name, String? platform, String? alias, DateTime? lastConnected, int? connectionCount}) =>
      RecentConnection(
        id: id,
        name: name ?? this.name,
        platform: platform ?? this.platform,
        alias: alias ?? this.alias,
        lastConnected: lastConnected ?? this.lastConnected,
        connectionCount: connectionCount ?? this.connectionCount,
      );

  Map<String, Object?> toJson() => {
        'id': id,
        'name': name,
        'platform': platform,
        'alias': alias,
        'lastConnected': lastConnected?.toIso8601String(),
        'count': connectionCount,
      };

  static RecentConnection? fromJson(Object? json) {
    if (json is! Map) return null;
    final id = json['id'];
    if (id is! String || !DeviceId.isValid(id)) return null;
    return RecentConnection(
      id: id,
      name: json['name'] is String ? json['name'] as String : '',
      platform: json['platform'] is String ? json['platform'] as String : '',
      alias: json['alias'] is String ? json['alias'] as String : '',
      lastConnected: json['lastConnected'] is String ? DateTime.tryParse(json['lastConnected'] as String) : null,
      connectionCount: json['count'] is int ? json['count'] as int : 0,
    );
  }
}

/// Pure, immutable-style list operations on the recent connection history.
class RecentConnectionList {
  RecentConnectionList([List<RecentConnection> entries = const []]) : entries = List.unmodifiable(entries);

  static const int maxEntries = 30;

  final List<RecentConnection> entries;

  RecentConnection? find(String id) {
    for (final e in entries) {
      if (e.id == id) return e;
    }
    return null;
  }

  RecentConnectionList recordAttempt(String id, {DateTime? now}) {
    if (!DeviceId.isValid(id)) return this;
    final existing = find(id) ?? RecentConnection(id: id);
    final updated = existing.copyWith(lastConnected: now ?? DateTime.now(), connectionCount: existing.connectionCount + 1);
    final rest = entries.where((e) => e.id != id).toList();
    return RecentConnectionList([updated, ...rest].take(maxEntries).toList());
  }

  RecentConnectionList updateDetails(String id, {String? name, String? platform}) => _map(
        id,
        (e) => e.copyWith(
          name: (name == null || name.isEmpty) ? null : name,
          platform: (platform == null || platform.isEmpty) ? null : platform,
        ),
      );

  RecentConnectionList rename(String id, String alias) {
    final trimmed = alias.trim();
    return _map(id, (e) => e.copyWith(alias: trimmed.length > 64 ? trimmed.substring(0, 64) : trimmed));
  }

  RecentConnectionList remove(String id) => RecentConnectionList(entries.where((e) => e.id != id).toList());

  RecentConnectionList _map(String id, RecentConnection Function(RecentConnection) change) =>
      RecentConnectionList([for (final e in entries) e.id == id ? change(e) : e]);

  String encode() => jsonEncode([for (final e in entries) e.toJson()]);

  static RecentConnectionList decode(String? raw) {
    if (raw == null || raw.isEmpty) return RecentConnectionList();
    try {
      final list = jsonDecode(raw);
      if (list is! List) return RecentConnectionList();
      final seen = <String>{};
      final result = <RecentConnection>[];
      for (final item in list) {
        final entry = RecentConnection.fromJson(item);
        if (entry != null && seen.add(entry.id)) result.add(entry);
      }
      return RecentConnectionList(result.take(maxEntries).toList());
    } on FormatException {
      return RecentConnectionList();
    }
  }
}
