import { CanActivate, ExecutionContext, Injectable, UnauthorizedException } from '@nestjs/common';
import type { Request } from 'express';
import { DevicesService } from '../devices/devices.service';
import type { DeviceRecord } from '../devices/device.entity';
import { TokenService } from './token.service';

export interface AuthenticatedRequest extends Request {
  device: DeviceRecord;
}

export function extractBearerToken(header: string | undefined): string | null {
  if (!header) return null;
  const [scheme, value] = header.split(' ');
  return scheme?.toLowerCase() === 'bearer' && value ? value : null;
}

/** Authenticates HTTP requests carrying a device access token. */
@Injectable()
export class DeviceAuthGuard implements CanActivate {
  constructor(
    private readonly tokens: TokenService,
    private readonly devices: DevicesService,
  ) {}

  async canActivate(context: ExecutionContext): Promise<boolean> {
    const request = context.switchToHttp().getRequest<AuthenticatedRequest>();
    const deviceId = await this.tokens.verifyDeviceToken(
      extractBearerToken(request.headers.authorization),
    );
    const device = deviceId ? this.devices.findById(deviceId) : undefined;
    if (!device) throw new UnauthorizedException('Invalid or expired access token');
    request.device = device;
    return true;
  }
}
