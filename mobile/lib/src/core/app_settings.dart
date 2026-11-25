import 'dart:io';

import 'package:shared_preferences/shared_preferences.dart';

import 'recent_connections.dart';
import 'server_url.dart';

/// Persistent configuration stored on the device.
class AppSettings {
  AppSettings._(this._prefs);

  static const _kServerUrl = 'serverUrl';
  static const _kDeviceId = 'deviceId';
  static const _kDeviceSecret = 'deviceSecret';
  static const _kDeviceName = 'deviceName';
  static const _kRecent = 'recentConnections';

  final SharedPreferences _prefs;

  static Future<AppSettings> load() async => AppSettings._(await SharedPreferences.getInstance());

  Uri? get serverUrl {
    final raw = _prefs.getString(_kServerUrl);
    return raw == null ? null : normalizeServerUrl(raw);
  }

  Future<void> setServerUrl(Uri url) => _prefs.setString(_kServerUrl, url.toString());

  String? get deviceId => _prefs.getString(_kDeviceId);
  String? get deviceSecret => _prefs.getString(_kDeviceSecret);

  Future<void> setDeviceCredentials(String id, String secret) async {
    await _prefs.setString(_kDeviceId, id);
    await _prefs.setString(_kDeviceSecret, secret);
  }

  Future<void> clearDeviceCredentials() async {
    await _prefs.remove(_kDeviceId);
    await _prefs.remove(_kDeviceSecret);
  }

  String get deviceName {
    final stored = _prefs.getString(_kDeviceName)?.trim() ?? '';
    if (stored.isNotEmpty) return stored;
    final host = Platform.localHostname.trim();
    if (host.isNotEmpty && host != 'localhost') return host.length > 64 ? host.substring(0, 64) : host;
    return Platform.isIOS ? 'iPhone' : 'Android';
  }

  Future<void> setDeviceName(String name) => _prefs.setString(_kDeviceName, name.trim());

  RecentConnectionList get recentConnections => RecentConnectionList.decode(_prefs.getString(_kRecent));

  Future<void> setRecentConnections(RecentConnectionList list) => _prefs.setString(_kRecent, list.encode());
}
