/**
 * Subset of the Cloudflare Realtime SFU Connection API schema (2024-05-21)
 * used by this service. Field names mirror the upstream OpenAPI document.
 * @see https://developers.cloudflare.com/realtime/sfu/api/
 */

export interface SessionDescription {
  type: 'offer' | 'answer';
  sdp: string;
}

export interface UpstreamError {
  errorCode?: string;
  errorDescription?: string;
}

export interface TrackObject {
  location: 'local' | 'remote';
  mid?: string;
  sessionId?: string;
  trackName?: string;
  kind?: string;
  bidirectionalMediaStream?: boolean;
  simulcast?: {
    preferredRid?: string;
    priorityOrdering?: 'none' | 'asciibetical';
    ridNotAvailable?: 'none' | 'asciibetical';
  };
}

export interface NewSessionResponse extends UpstreamError {
  sessionId: string;
  sessionDescription?: SessionDescription;
}

export interface TracksRequest {
  sessionDescription?: SessionDescription;
  tracks: TrackObject[];
  autoDiscover?: boolean;
}

export interface TracksResponse extends UpstreamError {
  requiresImmediateRenegotiation?: boolean;
  sessionDescription?: SessionDescription;
  tracks?: (TrackObject & UpstreamError)[];
}

export interface RenegotiateRequest {
  sessionDescription: SessionDescription;
}

export interface RenegotiateResponse extends UpstreamError {
  sessionDescription?: SessionDescription;
}

export interface CloseTracksRequest {
  tracks: { mid: string }[];
  sessionDescription?: SessionDescription;
  force: boolean;
}

export interface CloseTracksResponse extends UpstreamError {
  sessionDescription?: SessionDescription;
  tracks?: ({ mid: string } & UpstreamError)[];
  requiresImmediateRenegotiation?: boolean;
}

export interface DataChannelObject {
  location: 'local' | 'remote';
  dataChannelName: string;
  sessionId?: string;
  ordered?: boolean;
  maxRetransmits?: number;
  maxPacketLifeTime?: number;
  waitForAck?: boolean;
  canReply?: boolean;
}

export interface EstablishDataChannelsTransportRequest {
  dataChannel: { location: 'remote'; dataChannelName: 'server-events' };
  sessionDescription?: SessionDescription;
}

export interface EstablishDataChannelsTransportResponse extends UpstreamError {
  requiresImmediateRenegotiation?: boolean;
  dataChannel?: { dataChannelName: string; id: number } & UpstreamError;
  sessionDescription?: SessionDescription;
}

export interface DataChannelsRequest {
  dataChannels: DataChannelObject[];
}

export interface DataChannelsResponse extends UpstreamError {
  dataChannels?: (DataChannelObject & { id?: number } & UpstreamError)[];
}

export interface CloseDataChannelsRequest {
  dataChannels: { id: number }[];
}

export interface CloseDataChannelsResponse extends UpstreamError {
  dataChannels?: ({ id: number } & UpstreamError)[];
}

export interface IceServer {
  urls: string | string[];
  username?: string;
  credential?: string;
}

export interface GenerateIceServersResponse {
  iceServers: IceServer[];
}
