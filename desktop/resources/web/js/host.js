/*
 * Host (controlled side). Runs in an off-screen page:
 *  1. captures the primary screen;
 *  2. publishes it to the Cloudflare SFU as track "screen";
 *  3. publishes the "control" data channel; the viewer subscribes with
 *     `canReply` and sends input events back on it;
 *  4. forwards those events to native code for injection.
 */
'use strict';

(function () {
  const { SfuSession, waitForConnection, sendJson, parseJson, CONTROL_CHANNEL, SCREEN_TRACK, MAX_CLIPBOARD_LENGTH } = window.AnyPC;
  const INPUT_TYPES = new Set(['pm', 'pb', 'wh', 'kb']);

  async function captureScreen({ maxWidth, maxHeight, frameRate }) {
    const legacy = {
      audio: false,
      video: {
        mandatory: {
          chromeMediaSource: 'screen',
          maxWidth,
          maxHeight,
          maxFrameRate: frameRate,
        },
      },
    };
    try {
      // Qt WebEngine's unattended desktop capture path (no picker, no user gesture).
      return await navigator.mediaDevices.getUserMedia(legacy);
    } catch (legacyError) {
      if (!navigator.mediaDevices.getDisplayMedia) throw legacyError;
      return navigator.mediaDevices.getDisplayMedia({
        audio: false,
        video: { width: { max: maxWidth }, height: { max: maxHeight }, frameRate: { max: frameRate }, cursor: 'always' },
      });
    }
  }

  class HostController {
    constructor(bridge, api, config, log) {
      this.bridge = bridge;
      this.api = api;
      this.config = config;
      this.log = log;
      this.pc = null;
      this.channel = null;
      this.capture = null;
      this.announcement = null;
      this.viewerConnected = false;
      this.stopped = false;
    }

    async start() {
      this.bridge.reportState('starting', 'Capturing screen');
      const [{ iceServers }, stream] = await Promise.all([this.api.iceServers(), captureScreen(this.config.capture)]);
      if (this.stopped) {
        stream.getTracks().forEach((t) => t.stop());
        return;
      }

      const sourceTrack = stream.getVideoTracks()[0];
      const { width = 0, height = 0 } = sourceTrack.getSettings();
      this.bridge.captureStarted(width, height);
      sourceTrack.addEventListener('ended', () => this.fail('Screen capture stopped'));

      this.capture = window.AnyPCKeepAlive.withFrameKeepAlive(sourceTrack);
      const track = this.capture.track;
      track.contentHint = 'detail';

      this.pc = new RTCPeerConnection({ iceServers, bundlePolicy: 'max-bundle' });
      this.pc.addEventListener('connectionstatechange', () => this.onConnectionStateChange());
      const transceiver = this.pc.addTransceiver(track, {
        direction: 'sendonly',
        sendEncodings: [{ maxBitrate: this.config.capture.maxBitrate, maxFramerate: this.config.capture.frameRate }],
      });

      this.log.info(`Screen captured (${width}x${height}); publishing`);
      this.bridge.reportState('connecting', 'Publishing screen');
      const sfu = new SfuSession(this.api, this.config.connectionId, this.pc);
      await sfu.create();
      await sfu.publishTracks([{ transceiver, trackName: SCREEN_TRACK }]);
      await waitForConnection(this.pc);
      await this.preferSharpness(transceiver.sender);

      await sfu.establishDataChannels();
      await waitForConnection(this.pc);
      const [allocation] = await sfu.createDataChannels([{ location: 'local', dataChannelName: CONTROL_CHANNEL }]);
      this.channel = this.pc.createDataChannel(CONTROL_CHANNEL, { negotiated: true, id: allocation.id });
      this.channel.addEventListener('message', (event) => this.onControlMessage(event.data));

      this.announcement = {
        kind: 'host-ready',
        trackName: SCREEN_TRACK,
        dataChannelName: CONTROL_CHANNEL,
        width,
        height,
        platform: this.config.platform,
        inputSupported: this.config.inputSupported,
      };
      this.bridge.reportState('waiting', 'Waiting for the viewer');
      this.announce();
    }

    /** Text legibility matters more than motion for remote desktops. */
    async preferSharpness(sender) {
      try {
        const params = sender.getParameters();
        params.degradationPreference = 'maintain-resolution';
        await sender.setParameters(params);
      } catch (error) {
        this.log.warn('Could not set degradation preference', error);
      }
    }

    announce() {
      if (this.announcement) this.bridge.sendSignal(this.announcement);
    }

    onPeerSignal(payload) {
      // The viewer (re)announces itself when its page is ready.
      if (payload && payload.kind === 'viewer-ready') this.announce();
    }

    onControlMessage(data) {
      const message = parseJson(data);
      if (!message) return;
      if (INPUT_TYPES.has(message.t)) {
        this.bridge.inputEvent(message);
      } else if (message.t === 'hello') {
        if (!this.viewerConnected) {
          this.viewerConnected = true;
          this.bridge.reportState('connected', '');
        }
        sendJson(this.channel, { t: 'info', platform: this.config.platform, inputSupported: this.config.inputSupported });
      } else if (message.t === 'clip' && typeof message.text === 'string') {
        this.bridge.remoteClipboard(message.text.slice(0, MAX_CLIPBOARD_LENGTH));
      }
    }

    sendClipboard(text) {
      if (this.viewerConnected && text && text.length <= MAX_CLIPBOARD_LENGTH) sendJson(this.channel, { t: 'clip', text });
    }

    onConnectionStateChange() {
      if (this.pc && this.pc.connectionState === 'failed') this.fail('Media connection failed');
    }

    fail(reason) {
      if (this.stopped) return;
      this.log.error(reason);
      this.bridge.reportState('failed', reason);
    }

    stop() {
      this.stopped = true;
      if (this.channel) this.channel.close();
      if (this.pc) this.pc.close();
      if (this.capture) this.capture.stop();
      this.channel = null;
      this.pc = null;
      this.capture = null;
    }
  }

  (async () => {
    const bridge = await window.AnyPC.connectBridge();
    const api = new window.AnyPC.BackendApi(bridge);
    const log = window.AnyPC.createLogger(bridge, 'host');
    let controller = null;

    bridge.start.connect((config) => {
      controller = new HostController(bridge, api, config, log);
      controller.start().catch((error) => controller.fail(error.message || String(error)));
    });
    bridge.peerSignal.connect((payload) => controller && controller.onPeerSignal(payload));
    bridge.localClipboard.connect((text) => controller && controller.sendClipboard(text));
    bridge.stop.connect(() => controller && controller.stop());
    bridge.pageReady();
  })();
})();
