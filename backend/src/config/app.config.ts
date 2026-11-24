import { randomBytes } from 'node:crypto';
import { resolve } from 'node:path';
import { Inject, Injectable, Logger } from '@nestjs/common';
import { ConfigService } from '@nestjs/config';
import type { EnvironmentVariables } from './env.validation';

/**
 * Strongly typed, read-only view over the validated environment.
 * Inject this instead of ConfigService throughout the application.
 */
@Injectable()
export class AppConfig {
  private readonly logger = new Logger(AppConfig.name);

  readonly port: number;
  readonly corsOrigins: string[] | '*';
  readonly logLevel: EnvironmentVariables['LOG_LEVEL'];
  readonly jwtSecret: string;
  readonly jwtTtlSeconds: number;
  readonly dataDir: string;
  readonly connectionRequestTimeoutMs: number;
  readonly cloudflare: {
    readonly apiBase: string;
    readonly appId: string;
    readonly appSecret: string;
    readonly turnKeyId: string;
    readonly turnKeyApiToken: string;
    readonly turnCredentialTtlSeconds: number;
  };

  constructor(@Inject(ConfigService) config: ConfigService<EnvironmentVariables, true>) {
    const get = <K extends keyof EnvironmentVariables>(key: K): EnvironmentVariables[K] =>
      config.get(key, { infer: true });

    this.port = get('PORT');
    const origins = get('CORS_ORIGINS').trim();
    this.corsOrigins =
      origins === '*'
        ? '*'
        : origins
            .split(',')
            .map((o) => o.trim())
            .filter(Boolean);
    this.logLevel = get('LOG_LEVEL');
    this.jwtTtlSeconds = get('JWT_TTL_SECONDS');
    this.dataDir = resolve(get('DATA_DIR'));
    this.connectionRequestTimeoutMs = get('CONNECTION_REQUEST_TIMEOUT_SECONDS') * 1000;

    const secret = get('JWT_SECRET');
    if (secret && secret.length >= 16) {
      this.jwtSecret = secret;
    } else {
      this.jwtSecret = randomBytes(48).toString('base64url');
      this.logger.warn(
        'JWT_SECRET is missing or too short; generated an ephemeral secret. ' +
          'Issued tokens will be invalidated on restart.',
      );
    }

    this.cloudflare = Object.freeze({
      apiBase: get('CF_REALTIME_API_BASE').replace(/\/+$/, ''),
      appId: get('CF_REALTIME_APP_ID'),
      appSecret: get('CF_REALTIME_APP_SECRET'),
      turnKeyId: get('CF_TURN_KEY_ID'),
      turnKeyApiToken: get('CF_TURN_KEY_API_TOKEN'),
      turnCredentialTtlSeconds: get('CF_TURN_CREDENTIAL_TTL_SECONDS'),
    });
  }
}
