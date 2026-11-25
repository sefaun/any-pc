import type { IncomingMessage } from 'node:http';
import { Logger, OnModuleDestroy } from '@nestjs/common';
import {
  ConnectedSocket,
  MessageBody,
  OnGatewayConnection,
  OnGatewayDisconnect,
  OnGatewayInit,
  SubscribeMessage,
  WebSocketGateway,
  WebSocketServer,
} from '@nestjs/websockets';
import type { Subscription } from 'rxjs';
import { WebSocket, type Server } from 'ws';
import { extractBearerToken } from '../auth/device-auth.guard';
import { TokenService } from '../auth/token.service';
import { AppError } from '../common/errors/app-error';
import { validatePayload } from '../common/utils/validate-payload';
import { AppConfig } from '../config/app.config';
import type {
  Connection,
  ConnectionEndReason,
  ConnectionEvent,
} from '../connections/connection.entity';
import { ConnectionsService } from '../connections/connections.service';
import { canBeControlled, toDeviceSummary, type DeviceRecord } from '../devices/device.entity';
import { DevicesService } from '../devices/devices.service';
import {
  ConnectionEndDto,
  ConnectionRequestDto,
  ConnectionRespondDto,
  ConnectionSignalDto,
} from './dto/signaling.dto';
import { PresenceService } from './presence.service';
import { ClientEvent, ServerEvent } from './signaling.protocol';

const HEARTBEAT_INTERVAL_MS = 25_000;
const WS_CLOSE_UNAUTHORIZED = 4401;
const MAX_FRAME_BYTES = 256 * 1024;

interface SocketState {
  deviceId: string | null;
  alive: boolean;
}

@WebSocketGateway({ path: '/ws', maxPayload: MAX_FRAME_BYTES })
export class SignalingGateway
  implements OnGatewayInit, OnGatewayConnection, OnGatewayDisconnect, OnModuleDestroy
{
  private readonly logger = new Logger(SignalingGateway.name);
  private readonly state = new WeakMap<WebSocket, SocketState>();
  private heartbeat: NodeJS.Timeout | null = null;
  private eventsSubscription: Subscription | null = null;

  @WebSocketServer()
  private readonly server: Server;

  constructor(
    private readonly tokens: TokenService,
    private readonly devices: DevicesService,
    private readonly connections: ConnectionsService,
    private readonly presence: PresenceService,
    private readonly config: AppConfig,
  ) {}

  afterInit(): void {
    this.eventsSubscription = this.connections.events$.subscribe((e) => this.onConnectionEvent(e));
    this.heartbeat = setInterval(() => this.sweep(), HEARTBEAT_INTERVAL_MS);
    this.heartbeat.unref();
  }

  onModuleDestroy(): void {
    if (this.heartbeat) clearInterval(this.heartbeat);
    this.eventsSubscription?.unsubscribe();
  }

  async handleConnection(client: WebSocket, request: IncomingMessage): Promise<void> {
    const state: SocketState = { deviceId: null, alive: true };
    this.state.set(client, state);
    client.on('pong', () => (state.alive = true));

    const token =
      extractBearerToken(request.headers.authorization) ?? this.tokenFromQuery(request.url);
    const deviceId = await this.tokens.verifyDeviceToken(token);
    const device = deviceId ? this.devices.findById(deviceId) : undefined;
    if (!device || client.readyState !== WebSocket.OPEN) {
      client.close(WS_CLOSE_UNAUTHORIZED, 'Unauthorized');
      return;
    }

    state.deviceId = device.id;
    this.presence.attach(device.id, client);
    this.devices.touch(device.id);
    PresenceService.sendTo(client, ServerEvent.SessionReady, {
      device: toDeviceSummary(device),
      serverTime: new Date().toISOString(),
    });
    this.logger.log(`Device ${device.id} online (${this.presence.onlineCount} online)`);
  }

  handleDisconnect(client: WebSocket): void {
    const deviceId = this.state.get(client)?.deviceId;
    if (!deviceId || !this.presence.detach(deviceId, client)) return;
    this.devices.touch(deviceId);
    this.connections.endAllFor(deviceId);
    this.logger.log(`Device ${deviceId} offline (${this.presence.onlineCount} online)`);
  }

  @SubscribeMessage(ClientEvent.ConnectionRequest)
  onConnectionRequest(@ConnectedSocket() client: WebSocket, @MessageBody() body: unknown): void {
    this.handle(client, body, ConnectionRequestDto, (viewer, dto) => {
      const host = this.devices.findById(dto.targetId);
      if (!host) throw new AppError('DEVICE_NOT_FOUND', 'No device with this ID exists', 404);
      if (!this.presence.isOnline(host.id)) {
        throw new AppError('DEVICE_OFFLINE', 'The remote device is offline', 409);
      }
      if (!canBeControlled(host.platform)) {
        throw new AppError(
          'DEVICE_NOT_CONTROLLABLE',
          'This device is a phone and cannot be controlled remotely. Connect from the phone to the computer instead.',
          409,
        );
      }

      const connection = this.connections.request(viewer.id, host.id);
      const expiresInMs = this.config.connectionRequestTimeoutMs;
      this.presence.send(host.id, ServerEvent.ConnectionIncoming, {
        connectionId: connection.id,
        from: toDeviceSummary(viewer),
        ...(dto.password ? { password: dto.password } : {}),
        expiresInMs,
      });
      PresenceService.sendTo(client, ServerEvent.ConnectionPending, {
        requestId: dto.requestId,
        connectionId: connection.id,
        target: toDeviceSummary(host),
        expiresInMs,
      });
    });
  }

  @SubscribeMessage(ClientEvent.ConnectionRespond)
  onConnectionRespond(@ConnectedSocket() client: WebSocket, @MessageBody() body: unknown): void {
    this.handle(client, body, ConnectionRespondDto, (host, dto) => {
      if (!dto.accept) {
        const connection = this.connections.reject(dto.connectionId, host.id);
        this.presence.send(connection.viewerId, ServerEvent.ConnectionRejected, {
          connectionId: connection.id,
          reason: 'rejected',
        });
        return;
      }

      const connection = this.connections.accept(dto.connectionId, host.id);
      const viewer = this.devices.findById(connection.viewerId);
      if (!viewer || !this.presence.isOnline(viewer.id)) {
        this.connections.end(connection.id, host.id);
        throw new AppError('DEVICE_OFFLINE', 'The requesting device went offline', 409);
      }
      this.presence.send(viewer.id, ServerEvent.ConnectionAccepted, {
        connectionId: connection.id,
        role: 'viewer',
        peer: toDeviceSummary(host),
      });
      PresenceService.sendTo(client, ServerEvent.ConnectionAccepted, {
        connectionId: connection.id,
        role: 'host',
        peer: toDeviceSummary(viewer),
      });
    });
  }

  @SubscribeMessage(ClientEvent.ConnectionEnd)
  onConnectionEnd(@ConnectedSocket() client: WebSocket, @MessageBody() body: unknown): void {
    this.handle(client, body, ConnectionEndDto, (device, dto) => {
      this.connections.end(dto.connectionId, device.id);
    });
  }

  /** Relays opaque application signals (e.g. "publication ready") to the peer. */
  @SubscribeMessage(ClientEvent.ConnectionSignal)
  onConnectionSignal(@ConnectedSocket() client: WebSocket, @MessageBody() body: unknown): void {
    this.handle(client, body, ConnectionSignalDto, (device, dto) => {
      const { connection, role } = this.connections.requireActiveParticipant(
        dto.connectionId,
        device.id,
      );
      const peerId = role === 'host' ? connection.viewerId : connection.hostId;
      this.presence.send(peerId, ServerEvent.ConnectionSignal, {
        connectionId: connection.id,
        payload: dto.payload,
      });
    });
  }

  private onConnectionEvent(event: ConnectionEvent): void {
    const { connection } = event;
    if (event.type === 'expired') {
      this.presence.send(connection.viewerId, ServerEvent.ConnectionRejected, {
        connectionId: connection.id,
        reason: 'timeout',
      });
      this.notifyEnded(connection, 'timeout', connection.hostId);
      return;
    }
    for (const deviceId of [connection.hostId, connection.viewerId]) {
      if (deviceId === event.by) continue;
      this.notifyEnded(connection, event.reason, deviceId);
    }
    // The initiator also gets a confirmation so every client converges on the same state.
    if (event.by) this.notifyEnded(connection, event.reason, event.by);
  }

  private notifyEnded(connection: Connection, reason: ConnectionEndReason, deviceId: string): void {
    this.presence.send(deviceId, ServerEvent.ConnectionEnded, {
      connectionId: connection.id,
      reason,
    });
  }

  /**
   * Common request pipeline: authentication, validation and error reporting.
   * Errors are reported to the caller as `error` frames, never thrown to Nest.
   */
  private handle<T extends { requestId?: string }>(
    client: WebSocket,
    body: unknown,
    dto: new () => T,
    fn: (device: DeviceRecord, payload: T) => void,
  ): void {
    let requestId: string | undefined;
    try {
      const deviceId = this.state.get(client)?.deviceId;
      const device = deviceId ? this.devices.findById(deviceId) : undefined;
      if (!device) throw new AppError('UNAUTHORIZED', 'Socket is not authenticated', 401);
      const payload = validatePayload(dto, body);
      requestId = payload.requestId;
      fn(device, payload);
    } catch (error) {
      const appError =
        error instanceof AppError
          ? error
          : new AppError('INTERNAL_ERROR', 'Unexpected server error', 500);
      if (!(error instanceof AppError)) this.logger.error(error);
      PresenceService.sendTo(client, ServerEvent.Error, {
        requestId,
        code: appError.code,
        message: appError.message,
      });
    }
  }

  private sweep(): void {
    for (const client of this.server.clients) {
      const state = this.state.get(client);
      if (!state || !state.alive) {
        client.terminate();
        continue;
      }
      state.alive = false;
      client.ping();
    }
  }

  private tokenFromQuery(url: string | undefined): string | null {
    if (!url) return null;
    return new URL(url, 'http://localhost').searchParams.get('token');
  }
}
