import { Injectable, Logger } from '@nestjs/common';
import { AppError } from '../common/errors/app-error';
import { peerRole, type Connection, type ConnectionRole } from '../connections/connection.entity';
import { ConnectionsService } from '../connections/connections.service';
import type { DeviceRecord } from '../devices/device.entity';
import { CloudflareSfuClient } from './cloudflare/cloudflare-sfu.client';
import { CloudflareTurnClient } from './cloudflare/cloudflare-turn.client';
import type {
  CloseDataChannelsResponse,
  CloseTracksResponse,
  DataChannelObject,
  DataChannelsResponse,
  EstablishDataChannelsTransportResponse,
  IceServer,
  RenegotiateResponse,
  TrackObject,
  TracksResponse,
} from './cloudflare/cloudflare.types';
import type {
  CloseDataChannelsDto,
  CloseTracksDto,
  EstablishDataChannelsDto,
  NewDataChannelsDto,
  NewTracksDto,
  RenegotiateDto,
} from './dto/rtc.dto';

interface SessionContext {
  connection: Connection;
  role: ConnectionRole;
  sessionId: string;
}

/**
 * Authorisation layer in front of the Cloudflare SFU.
 *
 * Rules:
 *  - only participants of an *active* connection may operate on it;
 *  - each participant owns exactly one SFU session per connection;
 *  - `remote` resources can only reference the peer's session (filled in server side);
 *  - only the viewer (controller) may obtain `canReply` on a data channel.
 */
@Injectable()
export class RtcService {
  private readonly logger = new Logger(RtcService.name);

  constructor(
    private readonly connections: ConnectionsService,
    private readonly sfu: CloudflareSfuClient,
    private readonly turn: CloudflareTurnClient,
  ) {}

  iceServers(): Promise<{ iceServers: IceServer[]; ttl: number }> {
    return this.turn.generateIceServers();
  }

  async createSession(device: DeviceRecord, connectionId: string): Promise<{ sessionId: string }> {
    const { connection, role } = this.connections.requireActiveParticipant(connectionId, device.id);
    const response = await this.sfu.createSession(`${connection.id}:${role}`);
    if (!response.sessionId) {
      throw new AppError('UPSTREAM_ERROR', 'Cloudflare did not return a session id', 502);
    }
    connection.rtcSessions[role] = response.sessionId;
    this.logger.debug(`Connection ${connection.id}: ${role} session ${response.sessionId}`);
    return { sessionId: response.sessionId };
  }

  async newTracks(
    device: DeviceRecord,
    connectionId: string,
    dto: NewTracksDto,
  ): Promise<TracksResponse> {
    const ctx = this.context(device, connectionId);
    const tracks: TrackObject[] = dto.tracks.map((track) =>
      track.location === 'remote'
        ? {
            location: 'remote',
            trackName: track.trackName,
            sessionId: this.peerSession(ctx),
            ...(track.kind ? { kind: track.kind } : {}),
          }
        : {
            location: 'local',
            trackName: track.trackName,
            ...(track.mid ? { mid: track.mid } : {}),
          },
    );
    if (tracks.some((t) => t.location === 'local') && !dto.sessionDescription) {
      throw new AppError('VALIDATION_FAILED', 'Publishing local tracks requires an SDP offer');
    }
    if (new Set(tracks.map((t) => t.location)).size > 1) {
      throw new AppError('VALIDATION_FAILED', 'A batch must be all local or all remote');
    }
    return this.sfu.newTracks(ctx.sessionId, {
      ...(dto.sessionDescription ? { sessionDescription: dto.sessionDescription } : {}),
      tracks,
    });
  }

  renegotiate(
    device: DeviceRecord,
    connectionId: string,
    dto: RenegotiateDto,
  ): Promise<RenegotiateResponse> {
    const ctx = this.context(device, connectionId);
    return this.sfu.renegotiate(ctx.sessionId, { sessionDescription: dto.sessionDescription });
  }

  closeTracks(
    device: DeviceRecord,
    connectionId: string,
    dto: CloseTracksDto,
  ): Promise<CloseTracksResponse> {
    const ctx = this.context(device, connectionId);
    if (!dto.force && !dto.sessionDescription) {
      throw new AppError('VALIDATION_FAILED', 'A negotiated close requires an SDP offer');
    }
    return this.sfu.closeTracks(ctx.sessionId, {
      tracks: dto.tracks.map((t) => ({ mid: t.mid })),
      force: dto.force,
      ...(dto.sessionDescription ? { sessionDescription: dto.sessionDescription } : {}),
    });
  }

  establishDataChannels(
    device: DeviceRecord,
    connectionId: string,
    dto: EstablishDataChannelsDto,
  ): Promise<EstablishDataChannelsTransportResponse> {
    const ctx = this.context(device, connectionId);
    return this.sfu.establishDataChannels(ctx.sessionId, {
      dataChannel: { location: 'remote', dataChannelName: 'server-events' },
      ...(dto.sessionDescription ? { sessionDescription: dto.sessionDescription } : {}),
    });
  }

  newDataChannels(
    device: DeviceRecord,
    connectionId: string,
    dto: NewDataChannelsDto,
  ): Promise<DataChannelsResponse> {
    const ctx = this.context(device, connectionId);
    if (new Set(dto.dataChannels.map((c) => c.location)).size > 1) {
      throw new AppError('VALIDATION_FAILED', 'A batch must be all local or all remote');
    }

    const dataChannels: DataChannelObject[] = dto.dataChannels.map((channel) => {
      if (channel.maxRetransmits !== undefined && channel.maxPacketLifeTime !== undefined) {
        throw new AppError(
          'VALIDATION_FAILED',
          'Set at most one of maxRetransmits and maxPacketLifeTime',
        );
      }
      const delivery = {
        ...(channel.ordered !== undefined ? { ordered: channel.ordered } : {}),
        ...(channel.maxRetransmits !== undefined ? { maxRetransmits: channel.maxRetransmits } : {}),
        ...(channel.maxPacketLifeTime !== undefined
          ? { maxPacketLifeTime: channel.maxPacketLifeTime }
          : {}),
      };
      if (channel.location === 'local') {
        if (channel.canReply || channel.waitForAck) {
          throw new AppError(
            'VALIDATION_FAILED',
            'canReply/waitForAck apply to remote channels only',
          );
        }
        return { location: 'local', dataChannelName: channel.dataChannelName, ...delivery };
      }
      if (channel.canReply && ctx.role !== 'viewer') {
        throw new AppError('RTC_FORBIDDEN', 'Only the controlling device may reply', 403);
      }
      return {
        location: 'remote',
        dataChannelName: channel.dataChannelName,
        sessionId: this.peerSession(ctx),
        ...delivery,
        ...(channel.canReply !== undefined ? { canReply: channel.canReply } : {}),
        ...(channel.waitForAck !== undefined ? { waitForAck: channel.waitForAck } : {}),
      };
    });

    return this.sfu.newDataChannels(ctx.sessionId, { dataChannels });
  }

  closeDataChannels(
    device: DeviceRecord,
    connectionId: string,
    dto: CloseDataChannelsDto,
  ): Promise<CloseDataChannelsResponse> {
    const ctx = this.context(device, connectionId);
    return this.sfu.closeDataChannels(ctx.sessionId, {
      dataChannels: dto.dataChannels.map((c) => ({ id: c.id })),
    });
  }

  private context(device: DeviceRecord, connectionId: string): SessionContext {
    const { connection, role } = this.connections.requireActiveParticipant(connectionId, device.id);
    const sessionId = connection.rtcSessions[role];
    if (!sessionId) {
      throw new AppError(
        'RTC_SESSION_MISSING',
        'Create an RTC session for this connection first',
        409,
      );
    }
    return { connection, role, sessionId };
  }

  private peerSession(ctx: SessionContext): string {
    const sessionId = ctx.connection.rtcSessions[peerRole(ctx.role)];
    if (!sessionId) {
      throw new AppError('RTC_PEER_NOT_READY', 'The remote device has not published yet', 409);
    }
    return sessionId;
  }
}
