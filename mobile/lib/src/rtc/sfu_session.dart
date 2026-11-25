import 'dart:async';

import 'package:flutter_webrtc/flutter_webrtc.dart';

import '../net/api_client.dart';

/// Error reported by the Cloudflare Realtime SFU (top level or per resource).
class SfuException implements Exception {
  SfuException(this.message);
  final String message;
  @override
  String toString() => message;
}

/// One endpoint's Cloudflare SFU session, driven through the AnyPC backend
/// proxy (`/connections/:id/rtc/...`). Mirrors desktop/resources/web/js/rtc-common.js
/// and Cloudflare's "Connection patterns" guide. All mutations are awaited
/// sequentially by the single owner, satisfying per-session serialisation.
class SfuSession {
  SfuSession(this._api, String connectionId, this.pc) : _base = '/connections/$connectionId/rtc';

  static const iceGatheringTimeout = Duration(milliseconds: 2500);
  static const connectionTimeout = Duration(seconds: 30);

  final ApiClient _api;
  final String _base;
  final RTCPeerConnection pc;
  String? sessionId;

  Future<String> create() async {
    final response = await _api.post('$_base/sessions');
    final id = response['sessionId'];
    if (id is! String) throw SfuException('The server did not return an SFU session');
    sessionId = id;
    return id;
  }

  /// Subscribes to remote tracks and answers the SFU's offer.
  Future<void> subscribeTracks(List<Map<String, Object?>> tracks) async {
    final response = _check(await _api.post('$_base/tracks', {'tracks': tracks}), 'tracks');
    if (response['requiresImmediateRenegotiation'] == true) {
      await answerSfuOffer(_description(response['sessionDescription']));
    }
  }

  Future<void> establishDataChannels() async {
    final response = _check(await _api.post('$_base/datachannels/establish', <String, Object?>{}), 'dataChannel');
    final description = response['sessionDescription'];
    if (description is Map && description['type'] == 'offer') {
      await answerSfuOffer(_description(description));
    }
  }

  Future<List<Map<String, dynamic>>> createDataChannels(List<Map<String, Object?>> channels) async {
    final response = _check(await _api.post('$_base/datachannels', {'dataChannels': channels}), 'dataChannels');
    final list = response['dataChannels'];
    return list is List ? list.whereType<Map<String, dynamic>>().toList() : const [];
  }

  /// Applies an SFU offer, answers it and submits the answer via /renegotiate.
  Future<void> answerSfuOffer(RTCSessionDescription offer) async {
    await pc.setRemoteDescription(offer);
    final answer = await pc.createAnswer();
    await pc.setLocalDescription(answer);
    await waitForIceGathering();
    final local = await pc.getLocalDescription();
    if (local == null || local.sdp == null) throw SfuException('No local description');
    _check(await _api.put('$_base/renegotiate', {
      'sessionDescription': {'type': 'answer', 'sdp': local.sdp},
    }), null);
  }

  /// Bounded candidate gathering: proceeds with what was found so far.
  Future<void> waitForIceGathering() async {
    if (pc.iceGatheringState == RTCIceGatheringState.RTCIceGatheringStateComplete) return;
    final done = Completer<void>();
    final previous = pc.onIceGatheringState;
    pc.onIceGatheringState = (state) {
      previous?.call(state);
      if (state == RTCIceGatheringState.RTCIceGatheringStateComplete && !done.isCompleted) done.complete();
    };
    await done.future.timeout(iceGatheringTimeout, onTimeout: () {});
    pc.onIceGatheringState = previous;
  }

  Future<void> waitForConnection() async {
    final deadline = DateTime.now().add(connectionTimeout);
    while (true) {
      final state = pc.connectionState;
      if (state == RTCPeerConnectionState.RTCPeerConnectionStateConnected) return;
      if (state == RTCPeerConnectionState.RTCPeerConnectionStateFailed ||
          state == RTCPeerConnectionState.RTCPeerConnectionStateClosed) {
        throw SfuException('Media connection ${state!.name}');
      }
      if (DateTime.now().isAfter(deadline)) throw SfuException('Timed out while connecting to the media server');
      await Future<void>.delayed(const Duration(milliseconds: 100));
    }
  }

  static RTCSessionDescription _description(Object? json) {
    if (json is! Map || json['sdp'] is! String || json['type'] is! String) {
      throw SfuException('Missing session description');
    }
    return RTCSessionDescription(json['sdp'] as String, json['type'] as String);
  }

  /// Throws if the response or any of its items reports an error.
  static Map<String, dynamic> _check(Map<String, dynamic> response, String? listKey) {
    final code = response['errorCode'];
    if (code != null) throw SfuException('SFU error $code: ${response['errorDescription'] ?? ''}'.trim());
    if (listKey != null) {
      final value = response[listKey];
      final items = value is List ? value : (value is Map ? [value] : const []);
      for (final item in items) {
        if (item is Map && item['errorCode'] != null) {
          throw SfuException('SFU $listKey error ${item['errorCode']}: ${item['errorDescription'] ?? ''}'.trim());
        }
      }
    }
    return response;
  }
}
