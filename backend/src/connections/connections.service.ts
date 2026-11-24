import { randomUUID } from 'node:crypto';
import { Injectable, Logger, OnModuleDestroy } from '@nestjs/common';
import { Subject, type Observable } from 'rxjs';
import { AppConfig } from '../config/app.config';
import { AppError } from '../common/errors/app-error';
import type {
  Connection,
  ConnectionEndReason,
  ConnectionEvent,
  ConnectionRole,
} from './connection.entity';

export interface ParticipantView {
  connection: Connection;
  role: ConnectionRole;
}

/** How long ended connections are kept around for late/duplicate messages. */
const ENDED_RETENTION_MS = 60_000;

/**
 * In-memory registry of connection lifecycles. Single instance deployment is
 * assumed, so no shared state store is involved.
 */
@Injectable()
export class ConnectionsService implements OnModuleDestroy {
  private readonly logger = new Logger(ConnectionsService.name);
  private readonly connections = new Map<string, Connection>();
  private readonly timers = new Map<string, NodeJS.Timeout>();
  private readonly events = new Subject<ConnectionEvent>();

  constructor(private readonly config: AppConfig) {}

  get events$(): Observable<ConnectionEvent> {
    return this.events.asObservable();
  }

  request(viewerId: string, hostId: string): Connection {
    if (viewerId === hostId) {
      throw new AppError('SELF_CONNECTION', 'A device cannot connect to itself');
    }
    for (const c of this.connections.values()) {
      if (c.state === 'pending' && c.viewerId === viewerId && c.hostId === hostId) {
        throw new AppError(
          'CONNECTION_ALREADY_PENDING',
          'A connection request to this device is already pending',
          409,
        );
      }
    }

    const connection: Connection = {
      id: randomUUID(),
      hostId,
      viewerId,
      state: 'pending',
      createdAt: new Date(),
      acceptedAt: null,
      endedAt: null,
      rtcSessions: {},
    };
    this.connections.set(connection.id, connection);
    this.schedule(connection.id, this.config.connectionRequestTimeoutMs, () => {
      if (connection.state !== 'pending') return;
      this.finish(connection, 'timeout', null);
      this.events.next({ type: 'expired', connection });
    });
    this.logger.log(`Connection ${connection.id} requested: ${viewerId} -> ${hostId}`);
    return connection;
  }

  accept(connectionId: string, hostId: string): Connection {
    const connection = this.require(connectionId);
    if (connection.hostId !== hostId) {
      throw new AppError('CONNECTION_FORBIDDEN', 'Only the host can answer this request', 403);
    }
    if (connection.state !== 'pending') {
      throw new AppError('CONNECTION_NOT_ACTIVE', 'Connection request is no longer pending', 409);
    }
    this.clearTimer(connectionId);
    connection.state = 'active';
    connection.acceptedAt = new Date();
    this.logger.log(`Connection ${connection.id} accepted`);
    return connection;
  }

  reject(connectionId: string, hostId: string): Connection {
    const connection = this.require(connectionId);
    if (connection.hostId !== hostId) {
      throw new AppError('CONNECTION_FORBIDDEN', 'Only the host can answer this request', 403);
    }
    if (connection.state !== 'pending') {
      throw new AppError('CONNECTION_NOT_ACTIVE', 'Connection request is no longer pending', 409);
    }
    this.finish(connection, 'rejected', hostId);
    return connection;
  }

  /** Ends a pending or active connection on behalf of one of its participants. */
  end(connectionId: string, deviceId: string): Connection {
    const { connection } = this.requireParticipant(connectionId, deviceId);
    if (connection.state === 'ended') return connection;
    const reason: ConnectionEndReason = connection.state === 'pending' ? 'cancelled' : 'hangup';
    this.finish(connection, reason, deviceId);
    return connection;
  }

  /** Terminates every live connection a device takes part in (e.g. on disconnect). */
  endAllFor(deviceId: string): Connection[] {
    const ended: Connection[] = [];
    for (const connection of this.connections.values()) {
      if (connection.state === 'ended') continue;
      if (connection.hostId !== deviceId && connection.viewerId !== deviceId) continue;
      this.finish(connection, 'peer_disconnected', deviceId);
      ended.push(connection);
    }
    return ended;
  }

  requireParticipant(connectionId: string, deviceId: string): ParticipantView {
    const connection = this.require(connectionId);
    if (connection.hostId === deviceId) return { connection, role: 'host' };
    if (connection.viewerId === deviceId) return { connection, role: 'viewer' };
    throw new AppError('CONNECTION_FORBIDDEN', 'Not a participant of this connection', 403);
  }

  requireActiveParticipant(connectionId: string, deviceId: string): ParticipantView {
    const view = this.requireParticipant(connectionId, deviceId);
    if (view.connection.state !== 'active') {
      throw new AppError('CONNECTION_NOT_ACTIVE', 'Connection is not active', 409);
    }
    return view;
  }

  onModuleDestroy(): void {
    for (const timer of this.timers.values()) clearTimeout(timer);
    this.timers.clear();
    this.events.complete();
  }

  private require(connectionId: string): Connection {
    const connection = this.connections.get(connectionId);
    if (!connection) throw new AppError('CONNECTION_NOT_FOUND', 'Unknown connection', 404);
    return connection;
  }

  private finish(connection: Connection, reason: ConnectionEndReason, by: string | null): void {
    if (connection.state === 'ended') return;
    connection.state = 'ended';
    connection.endedAt = new Date();
    this.logger.log(`Connection ${connection.id} ended (${reason})`);
    if (reason !== 'timeout') this.events.next({ type: 'ended', connection, reason, by });
    this.schedule(connection.id, ENDED_RETENTION_MS, () => this.connections.delete(connection.id));
  }

  private schedule(connectionId: string, delayMs: number, fn: () => void): void {
    this.clearTimer(connectionId);
    const timer = setTimeout(() => {
      this.timers.delete(connectionId);
      fn();
    }, delayMs);
    timer.unref();
    this.timers.set(connectionId, timer);
  }

  private clearTimer(connectionId: string): void {
    const timer = this.timers.get(connectionId);
    if (timer) clearTimeout(timer);
    this.timers.delete(connectionId);
  }
}
