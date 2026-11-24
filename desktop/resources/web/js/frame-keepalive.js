/*
 * Cloudflare's SFU garbage-collects a published track after 30 seconds
 * without incoming media packets. Screen capture can stop producing frames
 * while the desktop is static, so this helper re-emits the last frame at a
 * low rate when the source goes idle (Insertable Streams / "breakout box").
 * Falls back to the original track when the APIs are unavailable.
 */
'use strict';

(function (global) {
  function withFrameKeepAlive(track, { idleMs = 1000 } = {}) {
    if (typeof MediaStreamTrackProcessor === 'undefined' || typeof MediaStreamTrackGenerator === 'undefined' || typeof VideoFrame === 'undefined') {
      return { track, stop: () => track.stop() };
    }

    const processor = new MediaStreamTrackProcessor({ track });
    const generator = new MediaStreamTrackGenerator({ kind: 'video' });
    const reader = processor.readable.getReader();
    const writer = generator.writable.getWriter();

    let lastFrame = null;
    let lastArrival = 0;
    let lastWrite = 0;
    let stopped = false;

    // Writing a frame to the generator closes it, hence the clones.
    const write = async (frame) => {
      try {
        await writer.write(frame);
        lastWrite = performance.now();
      } catch (_) {
        frame.close();
      }
    };

    const pump = async () => {
      while (!stopped) {
        const { value, done } = await reader.read();
        if (done) break;
        if (lastFrame) lastFrame.close();
        lastFrame = value.clone();
        lastArrival = performance.now();
        await write(value);
      }
    };

    const timer = setInterval(() => {
      if (stopped || !lastFrame) return;
      const now = performance.now();
      if (now - lastWrite < idleMs) return;
      // Keep timestamps monotonic relative to the source clock.
      const timestamp = lastFrame.timestamp + Math.round((now - lastArrival) * 1000);
      write(new VideoFrame(lastFrame, { timestamp }));
    }, Math.max(100, idleMs / 2));

    const stop = () => {
      if (stopped) return;
      stopped = true;
      clearInterval(timer);
      if (lastFrame) lastFrame.close();
      lastFrame = null;
      reader.cancel().catch(() => {});
      writer.close().catch(() => {});
      track.stop();
      generator.stop();
    };

    track.addEventListener('ended', stop);
    pump().catch(stop);
    return { track: generator, stop };
  }

  global.AnyPCKeepAlive = Object.freeze({ withFrameKeepAlive });
})(window);
