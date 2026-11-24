import type { ConnectionEndReason, ConnectionRole } from '../connections/connection.entity';
import type { DeviceSummary } from '../devices/device.entity';

/**
 * Wire protocol between desktop clients and the signaling gateway.
 * Every frame is a JSON object: `{ "event": string, "data": object }`.
 */
export const ClientEvent = {
  ConnectionRequest: 'connection.request',
  ConnectionRespond: 'connection.respond',
  ConnectionEnd: 'connection.end',
  ConnectionSignal: 'connection.signal',
} as const;

export const ServerEvent = {
  SessionReady: 'session.ready',
  ConnectionPending: 'connection.pending',
  ConnectionIncoming: 'connection.incoming',
  ConnectionAccepted: 'connection.accepted',
  ConnectionRejected: 'connection.rejected',
  ConnectionEnded: 'connection.ended',
  ConnectionSignal: 'connection.signal',
  Error: 'error',
} as const;

export type ServerEventName = (typeof ServerEvent)[keyof typeof ServerEvent];

export interface ServerEventPayloads {
  [ServerEvent.SessionReady]: { device: DeviceSummary; serverTime: string };
  [ServerEvent.ConnectionPending]: {
    requestId?: string;
    connectionId: string;
    target: DeviceSummary;
    expiresInMs: number;
  };
  [ServerEvent.ConnectionIncoming]: {
    connectionId: string;
    from: DeviceSummary;
    /** Unattended-access password typed by the viewer, verified by the host. */
    password?: string;
    expiresInMs: number;
  };
  [ServerEvent.ConnectionAccepted]: {
    connectionId: string;
    role: ConnectionRole;
    peer: DeviceSummary;
  };
  [ServerEvent.ConnectionRejected]: { connectionId: string; reason: 'rejected' | 'timeout' };
  [ServerEvent.ConnectionEnded]: { connectionId: string; reason: ConnectionEndReason };
  [ServerEvent.ConnectionSignal]: { connectionId: string; payload: Record<string, unknown> };
  [ServerEvent.Error]: { requestId?: string; code: string; message: string };
}

export interface ServerFrame<E extends ServerEventName = ServerEventName> {
  event: E;
  data: ServerEventPayloads[E];
}
