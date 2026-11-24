export type ConnectionState = 'pending' | 'active' | 'ended';
export type ConnectionRole = 'host' | 'viewer';

/**
 * A remote control relationship between two devices.
 * The viewer controls the host; the host publishes its screen.
 */
export interface Connection {
  readonly id: string;
  readonly hostId: string;
  readonly viewerId: string;
  state: ConnectionState;
  readonly createdAt: Date;
  acceptedAt: Date | null;
  endedAt: Date | null;
  /** Cloudflare Realtime SFU session ids, one per participant. */
  rtcSessions: Partial<Record<ConnectionRole, string>>;
}

export type ConnectionEndReason =
  'rejected' | 'timeout' | 'cancelled' | 'hangup' | 'peer_disconnected';

export type ConnectionEvent =
  | { type: 'expired'; connection: Connection }
  | { type: 'ended'; connection: Connection; reason: ConnectionEndReason; by: string | null };

export function peerRole(role: ConnectionRole): ConnectionRole {
  return role === 'host' ? 'viewer' : 'host';
}
