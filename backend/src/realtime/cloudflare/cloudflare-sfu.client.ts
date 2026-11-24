import { Injectable, Logger } from '@nestjs/common';
import { AppConfig } from '../../config/app.config';
import { cloudflareRequest, type HttpMethod } from './cloudflare-http';
import type {
  CloseDataChannelsRequest,
  CloseDataChannelsResponse,
  CloseTracksRequest,
  CloseTracksResponse,
  DataChannelsRequest,
  DataChannelsResponse,
  EstablishDataChannelsTransportRequest,
  EstablishDataChannelsTransportResponse,
  NewSessionResponse,
  RenegotiateRequest,
  RenegotiateResponse,
  TracksRequest,
  TracksResponse,
} from './cloudflare.types';

/**
 * Typed client for the Cloudflare Realtime SFU Connection API.
 * The App Secret never leaves this process.
 */
@Injectable()
export class CloudflareSfuClient {
  private readonly logger = new Logger(CloudflareSfuClient.name);

  constructor(private readonly config: AppConfig) {}

  createSession(correlationId?: string): Promise<NewSessionResponse> {
    const query = correlationId ? `?correlationId=${encodeURIComponent(correlationId)}` : '';
    return this.call('POST', `/sessions/new${query}`);
  }

  newTracks(sessionId: string, body: TracksRequest): Promise<TracksResponse> {
    return this.call('POST', `/sessions/${sessionId}/tracks/new`, body);
  }

  renegotiate(sessionId: string, body: RenegotiateRequest): Promise<RenegotiateResponse> {
    return this.call('PUT', `/sessions/${sessionId}/renegotiate`, body);
  }

  closeTracks(sessionId: string, body: CloseTracksRequest): Promise<CloseTracksResponse> {
    return this.call('PUT', `/sessions/${sessionId}/tracks/close`, body);
  }

  establishDataChannels(
    sessionId: string,
    body: EstablishDataChannelsTransportRequest,
  ): Promise<EstablishDataChannelsTransportResponse> {
    return this.call('POST', `/sessions/${sessionId}/datachannels/establish`, body);
  }

  newDataChannels(sessionId: string, body: DataChannelsRequest): Promise<DataChannelsResponse> {
    return this.call('POST', `/sessions/${sessionId}/datachannels/new`, body);
  }

  closeDataChannels(
    sessionId: string,
    body: CloseDataChannelsRequest,
  ): Promise<CloseDataChannelsResponse> {
    return this.call('PUT', `/sessions/${sessionId}/datachannels/close`, body);
  }

  private call<T>(method: HttpMethod, path: string, body?: unknown): Promise<T> {
    const { apiBase, appId, appSecret } = this.config.cloudflare;
    const url = `${apiBase}/apps/${encodeURIComponent(appId)}${path}`;
    return cloudflareRequest<T>(this.logger, method, url, appSecret, body);
  }
}
