import 'dart:async';
import 'dart:convert';

import 'package:http/http.dart' as http;

import '../core/server_url.dart';

/// Error returned by the AnyPC backend (or a transport failure, status 0).
class ApiException implements Exception {
  ApiException(this.status, this.message, {this.code});

  final int status;
  final String message;
  final String? code;

  @override
  String toString() => message;
}

/// JSON REST client for the AnyPC backend (`/api/v1`).
class ApiClient {
  ApiClient({http.Client? httpClient, this.userAgent = 'AnyPC-Mobile'}) : _http = httpClient ?? http.Client();

  static const _timeout = Duration(seconds: 20);

  final http.Client _http;
  final String userAgent;
  Uri? baseUrl;
  String? accessToken;

  Future<Map<String, dynamic>> get(String path) => _send('GET', path);
  Future<Map<String, dynamic>> post(String path, [Object? body]) => _send('POST', path, body);
  Future<Map<String, dynamic>> put(String path, [Object? body]) => _send('PUT', path, body);

  Future<Map<String, dynamic>> _send(String method, String path, [Object? body]) async {
    final base = baseUrl;
    if (base == null) throw ApiException(0, 'No server configured');
    final request = http.Request(method, joinUrl(base, '/api/v1$path'));
    request.headers['Accept'] = 'application/json';
    request.headers['User-Agent'] = userAgent;
    final token = accessToken;
    if (token != null && token.isNotEmpty) request.headers['Authorization'] = 'Bearer $token';
    if (body != null) {
      request.headers['Content-Type'] = 'application/json';
      request.body = jsonEncode(body);
    }

    final http.Response response;
    try {
      response = await http.Response.fromStream(await _http.send(request).timeout(_timeout));
    } on TimeoutException {
      throw ApiException(0, 'The server did not respond in time');
    } catch (e) {
      throw ApiException(0, 'Cannot reach the server ($e)');
    }

    Object? decoded;
    if (response.body.isNotEmpty) {
      try {
        decoded = jsonDecode(response.body);
      } on FormatException {
        decoded = null;
      }
    }
    final map = decoded is Map<String, dynamic> ? decoded : <String, dynamic>{};
    if (response.statusCode < 200 || response.statusCode >= 300) {
      final message = map['message'];
      throw ApiException(
        response.statusCode,
        message is String ? message : (message is List && message.isNotEmpty ? '${message.first}' : 'HTTP ${response.statusCode}'),
        code: map['code'] is String ? map['code'] as String : null,
      );
    }
    return map;
  }

  void close() => _http.close();
}
