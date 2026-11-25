/*
 * Viewer (controlling side):
 *  1. waits for the host's "host-ready" announcement;
 *  2. subscribes to the published screen track (and the sound track, when the
 *     host shares its sound);
 *  3. subscribes to the host's "control" data channel with `canReply`
 *     and streams keyboard/mouse input back on it.
 */
'use strict';

(function () {
  const { SfuSession, waitForConnection, sendJson, parseJson, MAX_CLIPBOARD_LENGTH } = window.AnyPC;

  const WHEEL_LINE_PX = 40;
  const WHEEL_PAGE_PX = 800;

  /** Translates DOM input on the video element into normalised remote events. */
  class InputCapture {
    constructor(video, send) {
      this.video = video;
      this.send = send;
      this.enabled = false;
      this.pendingMove = null;
      this.wheel = { dx: 0, dy: 0 };
      this.pressedKeys = new Set();
      this.pressedButtons = new Set();
      this.frameRequested = false;
      this.lastPosition = { x: 0.5, y: 0.5 };
      this.install();
    }

    /** Maps client coordinates into [0,1] within the letterboxed video content. */
    normalise(clientX, clientY) {
      const rect = this.video.getBoundingClientRect();
      const vw = this.video.videoWidth || rect.width;
      const vh = this.video.videoHeight || rect.height;
      const scale = Math.min(rect.width / vw, rect.height / vh);
      const width = vw * scale;
      const height = vh * scale;
      const left = rect.left + (rect.width - width) / 2;
      const top = rect.top + (rect.height - height) / 2;
      const clamp = (v) => Math.min(1, Math.max(0, v));
      this.lastPosition = { x: clamp((clientX - left) / width), y: clamp((clientY - top) / height) };
      return this.lastPosition;
    }

    scheduleFlush() {
      if (this.frameRequested) return;
      this.frameRequested = true;
      requestAnimationFrame(() => {
        this.frameRequested = false;
        this.flush();
      });
    }

    flush() {
      if (this.pendingMove) {
        this.send({ t: 'pm', ...this.pendingMove });
        this.pendingMove = null;
      }
      if (this.wheel.dx || this.wheel.dy) {
        this.send({ t: 'wh', dx: Math.round(this.wheel.dx), dy: Math.round(this.wheel.dy) });
        this.wheel = { dx: 0, dy: 0 };
      }
    }

    install() {
      const video = this.video;
      video.addEventListener('pointermove', (e) => {
        if (!this.enabled) return;
        this.pendingMove = this.normalise(e.clientX, e.clientY);
        this.scheduleFlush();
      });
      video.addEventListener('pointerdown', (e) => {
        if (!this.enabled) return;
        e.preventDefault();
        video.setPointerCapture(e.pointerId);
        this.flush();
        this.pressedButtons.add(e.button);
        this.send({ t: 'pb', b: e.button, d: true, ...this.normalise(e.clientX, e.clientY) });
      });
      video.addEventListener('pointerup', (e) => {
        if (!this.enabled || !this.pressedButtons.has(e.button)) return;
        e.preventDefault();
        this.flush();
        this.pressedButtons.delete(e.button);
        this.send({ t: 'pb', b: e.button, d: false, ...this.normalise(e.clientX, e.clientY) });
      });
      video.addEventListener(
        'wheel',
        (e) => {
          e.preventDefault();
          if (!this.enabled) return;
          const unit = e.deltaMode === 1 ? WHEEL_LINE_PX : e.deltaMode === 2 ? WHEEL_PAGE_PX : 1;
          this.wheel.dx += e.deltaX * unit;
          this.wheel.dy += e.deltaY * unit;
          this.scheduleFlush();
        },
        { passive: false },
      );
      video.addEventListener('contextmenu', (e) => e.preventDefault());
      video.addEventListener('dragstart', (e) => e.preventDefault());

      window.addEventListener('keydown', (e) => this.onKey(e, true), true);
      window.addEventListener('keyup', (e) => this.onKey(e, false), true);
      window.addEventListener('blur', () => this.releaseAll());
      document.addEventListener('visibilitychange', () => document.hidden && this.releaseAll());
    }

    onKey(event, down) {
      event.preventDefault();
      event.stopPropagation();
      if (!this.enabled || !event.code) return;
      // The host OS generates its own auto-repeat while the key is held.
      if (down && event.repeat) return;
      if (down) this.pressedKeys.add(event.code);
      else if (!this.pressedKeys.delete(event.code)) return;
      this.send({ t: 'kb', c: event.code, d: down });
    }

    /** Prevents stuck keys/buttons on the host when focus is lost. */
    releaseAll() {
      for (const code of this.pressedKeys) this.send({ t: 'kb', c: code, d: false });
      for (const button of this.pressedButtons) this.send({ t: 'pb', b: button, d: false, ...this.lastPosition });
      this.pressedKeys.clear();
      this.pressedButtons.clear();
    }

    setEnabled(enabled) {
      if (!enabled) this.releaseAll();
      this.enabled = enabled;
    }
  }

  class ViewerController {
    constructor(bridge, api, config, log, ui) {
      this.bridge = bridge;
      this.api = api;
      this.config = config;
      this.log = log;
      this.ui = ui;
      this.pc = null;
      this.channel = null;
      this.started = false;
      this.stopped = false;
      this.input = new InputCapture(ui.video, (message) => sendJson(this.channel, message));
    }

    start() {
      this.ui.setStatus(`Waiting for ${this.config.peerName || 'the remote computer'}…`);
      this.bridge.reportState('connecting', 'Waiting for the host');
      this.bridge.sendSignal({ kind: 'viewer-ready' });
    }

    onPeerSignal(payload) {
      if (!payload || payload.kind !== 'host-ready' || this.started || this.stopped) return;
      this.started = true;
      this.connect(payload).catch((error) => this.fail(error.message || String(error)));
    }

    async connect(info) {
      this.ui.setStatus('Connecting…');
      const { iceServers } = await this.api.iceServers();
      this.pc = new RTCPeerConnection({ iceServers, bundlePolicy: 'max-bundle' });
      this.pc.addEventListener('connectionstatechange', () => {
        if (this.pc && this.pc.connectionState === 'failed') this.fail('Media connection failed');
      });
      this.pc.addEventListener('track', (event) => {
        this.log.info('Remote track received', event.track.kind);
        if (event.track.kind === 'audio') {
          this.ui.audio.srcObject = new MediaStream([event.track]);
          this.ui.audio.muted = this.config.audioMuted === true;
          this.ui.audio.play().catch((error) => this.log.warn('Sound playback failed', error));
          this.bridge.remoteAudio(true);
          return;
        }
        this.ui.video.srcObject = new MediaStream([event.track]);
        this.ui.video.play().catch(() => {});
      });

      const sfu = new SfuSession(this.api, this.config.connectionId, this.pc);
      await sfu.create();
      this.log.info('Subscribing to the host screen');
      const tracks = [{ location: 'remote', trackName: info.trackName, kind: 'video' }];
      if (info.audioTrackName) tracks.push({ location: 'remote', trackName: info.audioTrackName, kind: 'audio' });
      await sfu.subscribeTracks(tracks);
      await waitForConnection(this.pc);
      this.log.info('Media connected; opening control channel');

      await sfu.establishDataChannels();
      await waitForConnection(this.pc);
      const wanted = [{ location: 'remote', dataChannelName: info.dataChannelName, canReply: true }];
      if (info.filesChannelName) wanted.push({ location: 'remote', dataChannelName: info.filesChannelName, canReply: true });
      const allocations = await sfu.createDataChannels(wanted);
      const idOf = (name) => allocations.find((a) => a.dataChannelName === name).id;

      if (info.filesChannelName) {
        const filesChannel = this.pc.createDataChannel(info.filesChannelName, { negotiated: true, id: idOf(info.filesChannelName) });
        this.files = new window.AnyPCFiles.FileTransferChannel(this.bridge, filesChannel, this.log);
      }
      this.channel = this.pc.createDataChannel(info.dataChannelName, { negotiated: true, id: idOf(info.dataChannelName) });
      this.channel.addEventListener('message', (event) => this.onControlMessage(event.data));
      this.channel.addEventListener('open', () => {
        sendJson(this.channel, { t: 'hello' });
        this.input.setEnabled(info.inputSupported !== false);
        this.ui.setStatus(info.inputSupported === false ? 'View only: remote input is not available on the host.' : '', info.inputSupported === false);
        this.bridge.reportState('connected', '');
      });
      this.channel.addEventListener('close', () => this.input.setEnabled(false));
    }

    onControlMessage(data) {
      const message = parseJson(data);
      if (!message) return;
      if (message.t === 'info') {
        this.bridge.remoteScreens({ screens: Array.isArray(message.screens) ? message.screens : [], current: message.current || 0 });
      } else if (message.t === 'clip' && typeof message.text === 'string') {
        this.bridge.remoteClipboard(message.text.slice(0, MAX_CLIPBOARD_LENGTH));
      }
    }

    setAudioMuted(muted) {
      this.config.audioMuted = muted;
      this.ui.audio.muted = muted;
      if (!muted) this.ui.audio.play().catch(() => {});
    }

    requestScreen(index) {
      sendJson(this.channel, { t: 'screen', index });
    }

    sendClipboard(text) {
      if (text && text.length <= MAX_CLIPBOARD_LENGTH) sendJson(this.channel, { t: 'clip', text });
    }

    fail(reason) {
      if (this.stopped) return;
      this.log.error(reason);
      this.ui.setStatus(reason);
      this.bridge.reportState('failed', reason);
    }

    stop() {
      this.stopped = true;
      this.input.setEnabled(false);
      if (this.channel) this.channel.close();
      if (this.pc) this.pc.close();
      this.channel = null;
      this.pc = null;
      this.ui.video.srcObject = null;
      this.ui.audio.srcObject = null;
      this.bridge.remoteAudio(false);
    }
  }

  function createUi() {
    const video = document.getElementById('screen');
    const audio = document.getElementById('sound');
    const overlay = document.getElementById('overlay');
    const status = document.getElementById('status');
    const spinner = document.getElementById('spinner');
    return {
      video,
      audio,
      /** Empty text hides the overlay; `persistent` shows a non-blocking banner. */
      setStatus(text, persistent = false) {
        status.textContent = text;
        overlay.classList.toggle('hidden', !text);
        overlay.classList.toggle('banner', persistent);
        spinner.classList.toggle('hidden', persistent || !text);
      },
    };
  }

  (async () => {
    const ui = createUi();
    const bridge = await window.AnyPC.connectBridge();
    const api = new window.AnyPC.BackendApi(bridge);
    const log = window.AnyPC.createLogger(bridge, 'viewer');
    let controller = null;

    bridge.start.connect((config) => {
      controller = new ViewerController(bridge, api, config, log, ui);
      controller.start();
    });
    bridge.peerSignal.connect((payload) => controller && controller.onPeerSignal(payload));
    bridge.localClipboard.connect((text) => controller && controller.sendClipboard(text));
    bridge.screenRequested.connect((index) => controller && controller.requestScreen(index));
    bridge.audioMuted.connect((muted) => controller && controller.setAudioMuted(muted));
    bridge.stop.connect(() => controller && controller.stop());
    bridge.pageReady();
  })();
})();
