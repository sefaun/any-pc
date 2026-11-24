import { Injectable, Logger } from '@nestjs/common';
import { WebSocket } from 'ws';
import type { ServerEventName, ServerEventPayloads, ServerFrame } from './signaling.protocol';

/** Close code used when a newer socket for the same device takes over. */
export const WS_CLOSE_REPLACED = 4001;

/** Tracks which device is reachable through which WebSocket. */
@Injectable()
export class PresenceService {
  private readonly logger = new Logger(PresenceService.name);
  private readonly sockets = new Map<string, WebSocket>();

  attach(deviceId: string, socket: WebSocket): void {
    const previous = this.sockets.get(deviceId);
    if (previous && previous !== socket) {
      this.logger.debug(`Device ${deviceId} reconnected; closing previous socket`);
      previous.close(WS_CLOSE_REPLACED, 'Replaced by a newer connection');
    }
    this.sockets.set(deviceId, socket);
  }

  /** Detaches the socket if it is still the current one. Returns true when detached. */
  detach(deviceId: string, socket: WebSocket): boolean {
    if (this.sockets.get(deviceId) !== socket) return false;
    this.sockets.delete(deviceId);
    return true;
  }

  isOnline(deviceId: string): boolean {
    return this.sockets.get(deviceId)?.readyState === WebSocket.OPEN;
  }

  get onlineCount(): number {
    return this.sockets.size;
  }

  send<E extends ServerEventName>(
    deviceId: string,
    event: E,
    data: ServerEventPayloads[E],
  ): boolean {
    const socket = this.sockets.get(deviceId);
    if (!socket) return false;
    return PresenceService.sendTo(socket, event, data);
  }

  static sendTo<E extends ServerEventName>(
    socket: WebSocket,
    event: E,
    data: ServerEventPayloads[E],
  ): boolean {
    if (socket.readyState !== WebSocket.OPEN) return false;
    const frame: ServerFrame<E> = { event, data };
    socket.send(JSON.stringify(frame));
    return true;
  }
}
