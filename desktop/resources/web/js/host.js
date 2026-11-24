/*
 * Host (controlled side). Runs in an off-screen page:
 *  1. captures a screen (selectable when several monitors are connected);
 *  2. publishes it to the Cloudflare SFU as track "screen";
 *  3. publishes the "control" and "files" data channels; the viewer subscribes
 *     with `canReply` and sends input events / files back on them;
 *  4. forwards input to native code for injection.
 */
'use strict';

(function () {
  const { SfuSession, waitForConnection, sendJson, parseJson, CONTROL_CHANNEL, SCREEN_TRACK, MAX_CLIPBOARD_LENGTH } = window.AnyPC;
  const { FileTransferChannel, FILES_CHANNEL } = window.AnyPCFiles;
  const INPUT_TYPES = new Set(['pm', 'pb', 'wh', 'kb']);
  const THUMB_WIDTH = 64;
  const THUMB_HEIGHT = 36;

  function invoke(method, ...args) {
    return new Promise((resolve) => method(...args, resolve));
  }

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
      // On Qt >= 6.7 native code chooses the screen (RtcPage::setScreenIndex).
      return await navigator.mediaDevices.getUserMedia(legacy);
    } catch (legacyError) {
      if (!navigator.mediaDevices.getDisplayMedia) throw legacyError;
      return navigator.mediaDevices.getDisplayMedia({
        audio: false,
        video: { width: { max: maxWidth }, height: { max: maxHeight }, frameRate: { max: frameRate }, cursor: 'always' },
      });
    }
  }

  /**
   * 64x36 grayscale preview of the capture (base64), used by native code to tell
   * which monitor is being shown. Reads a frame directly from the track so it
   * works in a page that is never rendered.
   */
  async function captureThumbnail(track) {
    const empty = { thumbnail: '', width: 0, height: 0 };
    if (typeof MediaStreamTrackProcessor === 'undefined' || typeof OffscreenCanvas === 'undefined') return empty;
    const clone = track.clone();
    const reader = new MediaStreamTrackProcessor({ track: clone }).readable.getReader();
    try {
      const result = await Promise.race([reader.read(), new Promise((resolve) => setTimeout(() => resolve(null), 2000))]);
      if (!result || !result.value) return empty;
      const frame = result.value;
      // The real frame size: getSettings() can report the requested maximum instead.
      const width = frame.displayWidth;
      const height = frame.displayHeight;
      const canvas = new OffscreenCanvas(THUMB_WIDTH, THUMB_HEIGHT);
      const context = canvas.getContext('2d');
      context.drawImage(frame, 0, 0, THUMB_WIDTH, THUMB_HEIGHT);
      frame.close();
      const { data } = context.getImageData(0, 0, THUMB_WIDTH, THUMB_HEIGHT);
      let binary = '';
      for (let i = 0; i < data.length; i += 4) {
        binary += String.fromCharCode(Math.round(0.299 * data[i] + 0.587 * data[i + 1] + 0.114 * data[i + 2]));
      }
      return { thumbnail: btoa(binary), width, height };
    } catch (_) {
      return empty;
    } finally {
      reader.cancel().catch(() => {});
      clone.stop();
    }
  }

  class HostController {
    constructor(bridge, api, config, log) {
      this.bridge = bridge;
      this.api = api;
      this.config = config;
      this.log = log;
      this.pc = null;
      this.sender = null;
      this.channel = null;
      this.files = null;
      this.capture = null;
      this.announcement = null;
      this.screens = { screens: [], current: 0 };
      this.switching = false;
      this.viewerConnected = false;
      this.stopped = false;
    }

    /** Captures the selected screen and reports it to native code. */
    async openCapture() {
      const stream = await captureScreen(this.config.capture);
      const sourceTrack = stream.getVideoTracks()[0];
      const settings = sourceTrack.getSettings();
      sourceTrack.addEventListener('ended', () => this.fail('Screen capture stopped'));
      const preview = await captureThumbnail(sourceTrack);
      const width = preview.width || settings.width || 0;
      const height = preview.height || settings.height || 0;
      this.bridge.captureStarted(width, height, preview.thumbnail);

      const capture = window.AnyPCKeepAlive.withFrameKeepAlive(sourceTrack);
      capture.track.contentHint = 'detail';
      capture.width = width;
      capture.height = height;
      return capture;
    }

    async start() {
      this.bridge.reportState('starting', 'Capturing screen');
      const [{ iceServers }, capture] = await Promise.all([this.api.iceServers(), this.openCapture()]);
      if (this.stopped) {
        capture.stop();
        return;
      }
      this.capture = capture;

      this.pc = new RTCPeerConnection({ iceServers, bundlePolicy: 'max-bundle' });
      this.pc.addEventListener('connectionstatechange', () => this.onConnectionStateChange());
      const transceiver = this.pc.addTransceiver(capture.track, {
        direction: 'sendonly',
        sendEncodings: [{ maxBitrate: this.config.capture.maxBitrate, maxFramerate: this.config.capture.frameRate }],
      });
      this.sender = transceiver.sender;

      this.log.info(`Screen captured (${capture.width}x${capture.height}); publishing`);
      this.bridge.reportState('connecting', 'Publishing screen');
      const sfu = new SfuSession(this.api, this.config.connectionId, this.pc);
      await sfu.create();
      await sfu.publishTracks([{ transceiver, trackName: SCREEN_TRACK }]);
      await waitForConnection(this.pc);
      await this.preferSharpness(transceiver.sender);

      await sfu.establishDataChannels();
      await waitForConnection(this.pc);
      const allocations = await sfu.createDataChannels([
        { location: 'local', dataChannelName: CONTROL_CHANNEL },
        { location: 'local', dataChannelName: FILES_CHANNEL },
      ]);
      const idOf = (name) => allocations.find((a) => a.dataChannelName === name).id;

      this.channel = this.pc.createDataChannel(CONTROL_CHANNEL, { negotiated: true, id: idOf(CONTROL_CHANNEL) });
      this.channel.addEventListener('message', (event) => this.onControlMessage(event.data));
      const filesChannel = this.pc.createDataChannel(FILES_CHANNEL, { negotiated: true, id: idOf(FILES_CHANNEL) });
      this.files = new FileTransferChannel(this.bridge, filesChannel, this.log);

      this.announcement = {
        kind: 'host-ready',
        trackName: SCREEN_TRACK,
        dataChannelName: CONTROL_CHANNEL,
        filesChannelName: FILES_CHANNEL,
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

    /** Switches the published video to another monitor without renegotiation. */
    async switchScreen(index) {
      if (this.switching || !this.sender || index === this.screens.current) return;
      this.switching = true;
      try {
        this.log.info(`Switching to screen ${index + 1}`);
        await invoke(this.bridge.selectScreen, index);
        const capture = await this.openCapture();
        if (this.stopped) {
          capture.stop();
          return;
        }
        await this.sender.replaceTrack(capture.track);
        const previous = this.capture;
        this.capture = capture;
        if (previous) previous.stop();
      } catch (error) {
        this.log.warn('Screen switch failed', error);
      } finally {
        this.switching = false;
        this.sendInfo();
      }
    }

    announce() {
      if (this.announcement) this.bridge.sendSignal(this.announcement);
    }

    sendInfo() {
      sendJson(this.channel, {
        t: 'info',
        platform: this.config.platform,
        inputSupported: this.config.inputSupported,
        screens: this.screens.screens || [],
        current: this.screens.current || 0,
      });
    }

    onScreensUpdated(info) {
      this.screens = info;
      if (this.viewerConnected) this.sendInfo();
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
        this.sendInfo();
      } else if (message.t === 'screen' && Number.isInteger(message.index)) {
        this.switchScreen(message.index);
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
    let screens = null;

    bridge.start.connect((config) => {
      controller = new HostController(bridge, api, config, log);
      if (screens) controller.screens = screens;
      controller.start().catch((error) => controller.fail(error.message || String(error)));
    });
    // Emitted by native code while getUserMedia runs, possibly before start() resolves.
    bridge.screensUpdated.connect((info) => {
      screens = info;
      if (controller) controller.onScreensUpdated(info);
    });
    bridge.peerSignal.connect((payload) => controller && controller.onPeerSignal(payload));
    bridge.localClipboard.connect((text) => controller && controller.sendClipboard(text));
    bridge.stop.connect(() => controller && controller.stop());
    bridge.pageReady();
  })();
})();
