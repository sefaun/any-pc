/*
 * Shared WebRTC helpers for the embedded AnyPC pages.
 *
 * All Cloudflare Realtime SFU calls go through the native bridge, which
 * forwards them to the AnyPC backend; the backend holds the SFU App Secret.
 * Sequences follow Cloudflare's "Connection patterns" guide:
 * https://developers.cloudflare.com/realtime/sfu/get-started/connection-patterns/
 */
'use strict';

(function (global) {
  const ICE_GATHERING_TIMEOUT_MS = 2500;
  const CONNECTION_TIMEOUT_MS = 20000;

  /** Resolves with the native `bridge` object exposed through QWebChannel. */
  function connectBridge() {
    return new Promise((resolve) => {
      // eslint-disable-next-line no-undef
      new QWebChannel(qt.webChannelTransport, (channel) => resolve(channel.objects.bridge));
    });
  }

  function createLogger(bridge, scope) {
    const emit = (level, args) => {
      const message = `[${scope}] ` + args.map((a) => (a instanceof Error ? a.message : typeof a === 'string' ? a : JSON.stringify(a))).join(' ');
      bridge.log(level, message);
    };
    return {
      info: (...args) => emit('info', args),
      warn: (...args) => emit('warn', args),
      error: (...args) => emit('error', args),
    };
  }

  /** Promise based request/response over the bridge's apiRequest/apiResponse pair. */
  class BackendApi {
    constructor(bridge) {
      this.bridge = bridge;
      this.sequence = 0;
      this.pending = new Map();
      bridge.apiResponse.connect((id, status, bodyText, error) => {
        const entry = this.pending.get(id);
        if (!entry) return;
        this.pending.delete(id);
        const body = bodyText ? parseJson(bodyText) : null;
        if (status >= 200 && status < 300) {
          entry.resolve(body ?? {});
        } else {
          const err = new Error(error || (body && body.message) || `Request failed (${status})`);
          err.status = status;
          err.code = body && body.code;
          entry.reject(err);
        }
      });
    }

    request(method, path, body) {
      const id = ++this.sequence;
      return new Promise((resolve, reject) => {
        this.pending.set(id, { resolve, reject });
        this.bridge.apiRequest(id, method, path, body === undefined ? null : body);
      });
    }

    iceServers() {
      return this.request('GET', '/rtc/ice-servers');
    }
  }

  /** Throws if the SFU response or any of its items reports an error. */
  function assertSfuResult(response, listKey) {
    if (response.errorCode) {
      throw new Error(`SFU error ${response.errorCode}: ${response.errorDescription || ''}`.trim());
    }
    const items = listKey ? response[listKey] : null;
    const single = listKey === 'dataChannel' ? [response.dataChannel] : null;
    for (const item of single || items || []) {
      if (item && item.errorCode) {
        throw new Error(`SFU ${listKey} error ${item.errorCode}: ${item.errorDescription || ''}`.trim());
      }
    }
    return response;
  }

  function waitForIceGathering(pc, timeoutMs = ICE_GATHERING_TIMEOUT_MS) {
    if (pc.iceGatheringState === 'complete') return Promise.resolve();
    return new Promise((resolve) => {
      const done = () => {
        clearTimeout(timer);
        pc.removeEventListener('icegatheringstatechange', onChange);
        resolve();
      };
      const onChange = () => pc.iceGatheringState === 'complete' && done();
      // Bounded gathering: proceed with the candidates found so far.
      const timer = setTimeout(done, timeoutMs);
      pc.addEventListener('icegatheringstatechange', onChange);
    });
  }

  function waitForConnection(pc, timeoutMs = CONNECTION_TIMEOUT_MS) {
    if (pc.connectionState === 'connected') return Promise.resolve();
    return new Promise((resolve, reject) => {
      const cleanup = () => {
        clearTimeout(timer);
        pc.removeEventListener('connectionstatechange', onChange);
      };
      const onChange = () => {
        if (pc.connectionState === 'connected') {
          cleanup();
          resolve();
        } else if (pc.connectionState === 'failed' || pc.connectionState === 'closed') {
          cleanup();
          reject(new Error(`Peer connection ${pc.connectionState}`));
        }
      };
      const timer = setTimeout(() => {
        cleanup();
        reject(new Error('Timed out while connecting to the media server'));
      }, timeoutMs);
      pc.addEventListener('connectionstatechange', onChange);
    });
  }

  /**
   * One endpoint's SFU session. Every mutation is awaited sequentially by its
   * single owner, which satisfies the SFU's per-session serialisation rule.
   */
  class SfuSession {
    constructor(api, connectionId, pc) {
      this.api = api;
      this.pc = pc;
      this.base = `/connections/${connectionId}/rtc`;
      this.sessionId = null;
    }

    async create() {
      const response = await this.api.request('POST', `${this.base}/sessions`);
      this.sessionId = response.sessionId;
      return this.sessionId;
    }

    /**
     * Publishes local transceivers. `entries` is [{ transceiver, trackName }];
     * mids are only assigned once the local offer has been applied.
     */
    async publishTracks(entries) {
      const offer = await this.pc.createOffer();
      await this.pc.setLocalDescription(offer);
      await waitForIceGathering(this.pc);
      const tracks = entries.map(({ transceiver, trackName }) => ({ location: 'local', mid: transceiver.mid, trackName }));
      const response = assertSfuResult(
        await this.api.request('POST', `${this.base}/tracks`, {
          sessionDescription: { type: 'offer', sdp: this.pc.localDescription.sdp },
          tracks,
        }),
        'tracks',
      );
      await this.pc.setRemoteDescription(response.sessionDescription);
      return response;
    }

    async subscribeTracks(tracks) {
      const response = assertSfuResult(await this.api.request('POST', `${this.base}/tracks`, { tracks }), 'tracks');
      if (response.requiresImmediateRenegotiation) await this.answerSfuOffer(response.sessionDescription);
      return response;
    }

    async establishDataChannels() {
      const response = assertSfuResult(
        await this.api.request('POST', `${this.base}/datachannels/establish`, {}),
        'dataChannel',
      );
      if (response.sessionDescription && response.sessionDescription.type === 'offer') {
        await this.answerSfuOffer(response.sessionDescription);
      }
      return response;
    }

    async createDataChannels(dataChannels) {
      const response = assertSfuResult(
        await this.api.request('POST', `${this.base}/datachannels`, { dataChannels }),
        'dataChannels',
      );
      return response.dataChannels || [];
    }

    /** Applies an SFU offer, answers it and submits the answer via /renegotiate. */
    async answerSfuOffer(offer) {
      await this.pc.setRemoteDescription(offer);
      const answer = await this.pc.createAnswer();
      await this.pc.setLocalDescription(answer);
      await waitForIceGathering(this.pc);
      const response = await this.api.request('PUT', `${this.base}/renegotiate`, {
        sessionDescription: { type: 'answer', sdp: this.pc.localDescription.sdp },
      });
      assertSfuResult(response);
    }
  }

  function sendJson(channel, message) {
    if (channel && channel.readyState === 'open') {
      channel.send(JSON.stringify(message));
      return true;
    }
    return false;
  }

  function parseJson(data) {
    if (typeof data !== 'string') return null;
    try {
      const value = JSON.parse(data);
      return value && typeof value === 'object' ? value : null;
    } catch (_) {
      return null;
    }
  }

  global.AnyPC = Object.freeze({
    connectBridge,
    createLogger,
    BackendApi,
    SfuSession,
    waitForConnection,
    waitForIceGathering,
    sendJson,
    parseJson,
    CONTROL_CHANNEL: 'control',
    SCREEN_TRACK: 'screen',
    MAX_CLIPBOARD_LENGTH: 16 * 1024,
  });
})(window);
