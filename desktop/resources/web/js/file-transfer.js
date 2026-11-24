/*
 * File transfer over the "files" data channel (both directions).
 *
 * The host publishes the channel and the viewer subscribes with `canReply`,
 * so each side sends on the same channel and only receives the other side's
 * messages. Native code (FileTransferManager) owns all file system access:
 *
 *   sender                                   receiver
 *   {t:'file-offer', id, name, size}  ───►   bridge.fileIncoming()
 *   binary chunks (≤16 KiB)           ───►   bridge.fileWriteChunk()
 *   {t:'file-end', id}                ───►   bridge.fileIncomingFinished()
 *                                     ◄───   {t:'file-ack', id, ok, error}
 *
 * Each side sends one file at a time, so binary messages always belong to the
 * single in-flight incoming transfer.
 */
'use strict';

(function (global) {
  const HIGH_WATER_MARK = 1024 * 1024;
  const LOW_WATER_MARK = 256 * 1024;
  const ACK_TIMEOUT_MS = 120000;

  /** Calls a Q_INVOKABLE that returns a value and resolves with that value. */
  function invoke(method, ...args) {
    return new Promise((resolve) => method(...args, resolve));
  }

  function base64ToBytes(base64) {
    const binary = atob(base64);
    const bytes = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
    return bytes;
  }

  function bytesToBase64(buffer) {
    const bytes = new Uint8Array(buffer);
    let binary = '';
    const step = 0x8000;
    for (let i = 0; i < bytes.length; i += step) {
      binary += String.fromCharCode.apply(null, bytes.subarray(i, i + step));
    }
    return btoa(binary);
  }

  class FileTransferChannel {
    constructor(bridge, channel, log) {
      this.bridge = bridge;
      this.channel = channel;
      this.log = log;
      this.incoming = null;
      this.pendingAcks = new Map();
      this.sendQueue = Promise.resolve();

      channel.binaryType = 'arraybuffer';
      channel.bufferedAmountLowThreshold = LOW_WATER_MARK;
      channel.addEventListener('message', (event) => this.onMessage(event.data));
      channel.addEventListener('open', () => bridge.fileChannelReady(true));
      channel.addEventListener('close', () => this.onClose());
      bridge.fileOffer.connect((id, name, size) => {
        // Serialise sends: one file at a time.
        this.sendQueue = this.sendQueue.then(() => this.send(id, name, size));
      });
      if (channel.readyState === 'open') bridge.fileChannelReady(true);
    }

    async send(id, name, size) {
      try {
        this.sendJson({ t: 'file-offer', id, name, size });
        for (;;) {
          const chunk = await invoke(this.bridge.fileReadChunk, id);
          if (!chunk) break;
          await this.waitForBuffer();
          this.channel.send(base64ToBytes(chunk));
        }
        this.sendJson({ t: 'file-end', id });
        const ack = await this.waitForAck(id);
        this.bridge.fileSendFinished(id, !!ack.ok, ack.error || '');
      } catch (error) {
        this.log.warn('File send failed', error);
        this.bridge.fileSendFinished(id, false, error.message || String(error));
      }
    }

    sendJson(message) {
      if (this.channel.readyState !== 'open') throw new Error('Connection closed');
      this.channel.send(JSON.stringify(message));
    }

    waitForBuffer() {
      if (this.channel.readyState !== 'open') return Promise.reject(new Error('Connection closed'));
      if (this.channel.bufferedAmount < HIGH_WATER_MARK) return Promise.resolve();
      return new Promise((resolve, reject) => {
        const onLow = () => {
          this.channel.removeEventListener('close', onClose);
          resolve();
        };
        const onClose = () => {
          this.channel.removeEventListener('bufferedamountlow', onLow);
          reject(new Error('Connection closed'));
        };
        this.channel.addEventListener('bufferedamountlow', onLow, { once: true });
        this.channel.addEventListener('close', onClose, { once: true });
      });
    }

    waitForAck(id) {
      return new Promise((resolve) => {
        const timer = setTimeout(() => {
          this.pendingAcks.delete(id);
          resolve({ ok: false, error: 'No confirmation from the remote side' });
        }, ACK_TIMEOUT_MS);
        this.pendingAcks.set(id, (ack) => {
          clearTimeout(timer);
          resolve(ack);
        });
      });
    }

    onMessage(data) {
      if (data instanceof ArrayBuffer) {
        const transfer = this.incoming;
        if (!transfer) return;
        const encoded = bytesToBase64(data);
        // Chunks may arrive before native code accepted the offer: chain them.
        // Bridge calls are delivered in order, so writes land before fileIncomingFinished.
        transfer.chain = transfer.chain.then(() => {
          if (transfer.ok) this.bridge.fileWriteChunk(transfer.id, encoded);
        });
        return;
      }
      const message = window.AnyPC.parseJson(data);
      if (!message) return;
      if (message.t === 'file-offer') this.onOffer(message);
      else if (message.t === 'file-end') this.onEnd(message);
      else if (message.t === 'file-ack') {
        const resolve = this.pendingAcks.get(message.id);
        if (resolve) {
          this.pendingAcks.delete(message.id);
          resolve(message);
        }
      }
    }

    onOffer({ id, name, size }) {
      if (typeof id !== 'string' || typeof name !== 'string' || typeof size !== 'number') return;
      const transfer = { id, ok: false, chain: null };
      this.incoming = transfer;
      transfer.chain = invoke(this.bridge.fileIncoming, id, name, size).then((accepted) => {
        transfer.ok = accepted;
        if (!accepted) this.sendAck(id, false, 'The remote side could not save the file');
      });
    }

    async onEnd({ id }) {
      const transfer = this.incoming;
      if (!transfer || transfer.id !== id) return;
      this.incoming = null;
      await transfer.chain;
      if (!transfer.ok) return;
      const ok = await invoke(this.bridge.fileIncomingFinished, id);
      this.sendAck(id, ok, ok ? '' : 'The file could not be saved completely');
    }

    sendAck(id, ok, error) {
      try {
        this.sendJson({ t: 'file-ack', id, ok, error });
      } catch (_) {
        /* channel closed; the sender times out */
      }
    }

    onClose() {
      if (this.incoming) this.bridge.fileIncomingFailed(this.incoming.id, 'Connection closed');
      this.incoming = null;
      for (const resolve of this.pendingAcks.values()) resolve({ ok: false, error: 'Connection closed' });
      this.pendingAcks.clear();
      this.bridge.fileChannelReady(false);
    }
  }

  global.AnyPCFiles = Object.freeze({ FileTransferChannel, FILES_CHANNEL: 'files' });
})(window);
