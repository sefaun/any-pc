import { Injectable } from '@nestjs/common';
import { JwtService } from '@nestjs/jwt';
import { AppConfig } from '../config/app.config';

export interface DeviceTokenPayload {
  /** Device identifier. */
  sub: string;
  typ: 'device';
}

export interface IssuedToken {
  accessToken: string;
  expiresIn: number;
}

@Injectable()
export class TokenService {
  constructor(
    private readonly jwt: JwtService,
    private readonly config: AppConfig,
  ) {}

  async issueDeviceToken(deviceId: string): Promise<IssuedToken> {
    const payload: DeviceTokenPayload = { sub: deviceId, typ: 'device' };
    const accessToken = await this.jwt.signAsync(payload, {
      secret: this.config.jwtSecret,
      expiresIn: this.config.jwtTtlSeconds,
    });
    return { accessToken, expiresIn: this.config.jwtTtlSeconds };
  }

  /** Returns the device id for a valid token, or null. Never throws. */
  async verifyDeviceToken(token: string | undefined | null): Promise<string | null> {
    if (!token) return null;
    try {
      const payload = await this.jwt.verifyAsync<DeviceTokenPayload>(token, {
        secret: this.config.jwtSecret,
      });
      return payload.typ === 'device' && typeof payload.sub === 'string' ? payload.sub : null;
    } catch {
      return null;
    }
  }
}
