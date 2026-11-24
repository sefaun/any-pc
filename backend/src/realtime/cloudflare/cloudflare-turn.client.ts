import { Injectable, Logger } from '@nestjs/common';
import { AppConfig } from '../../config/app.config';
import { cloudflareRequest } from './cloudflare-http';
import type { GenerateIceServersResponse, IceServer } from './cloudflare.types';

/** Browsers block port 53; drop those URLs to avoid ICE timeouts (per Cloudflare docs). */
const BLOCKED_PORT = /:53(\?|$)/;

/** Issues short-lived Cloudflare TURN credentials. */
@Injectable()
export class CloudflareTurnClient {
  private readonly logger = new Logger(CloudflareTurnClient.name);

  constructor(private readonly config: AppConfig) {}

  async generateIceServers(): Promise<{ iceServers: IceServer[]; ttl: number }> {
    const { apiBase, turnKeyId, turnKeyApiToken, turnCredentialTtlSeconds } =
      this.config.cloudflare;
    const url = `${apiBase}/turn/keys/${encodeURIComponent(turnKeyId)}/credentials/generate-ice-servers`;
    const response = await cloudflareRequest<GenerateIceServersResponse>(
      this.logger,
      'POST',
      url,
      turnKeyApiToken,
      { ttl: turnCredentialTtlSeconds },
    );

    const iceServers = (response.iceServers ?? [])
      .map((server) => {
        const urls = (Array.isArray(server.urls) ? server.urls : [server.urls]).filter(
          (u) => !BLOCKED_PORT.test(u),
        );
        return { ...server, urls };
      })
      .filter((server) => server.urls.length > 0);

    return { iceServers, ttl: turnCredentialTtlSeconds };
  }
}
