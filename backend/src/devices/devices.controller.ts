import { Body, Controller, Get, HttpCode, HttpStatus, Post, UseGuards } from '@nestjs/common';
import { Throttle } from '@nestjs/throttler';
import { CurrentDevice } from '../auth/current-device.decorator';
import { DeviceAuthGuard } from '../auth/device-auth.guard';
import { TokenService } from '../auth/token.service';
import { toDeviceSummary, type DeviceRecord, type DeviceSummary } from './device.entity';
import { DevicesService } from './devices.service';
import { RegisterDeviceDto, type RegisterDeviceResponse } from './dto/register-device.dto';

export interface ChangeDeviceIdResponse {
  deviceId: string;
  accessToken: string;
  expiresIn: number;
}

@Controller('devices')
export class DevicesController {
  constructor(
    private readonly devices: DevicesService,
    private readonly tokens: TokenService,
  ) {}

  /** Registers a device (first launch) or exchanges stored credentials for a fresh token. */
  @Post('register')
  @HttpCode(HttpStatus.OK)
  @Throttle({ default: { limit: 20, ttl: 60_000 } })
  async register(@Body() dto: RegisterDeviceDto): Promise<RegisterDeviceResponse> {
    const { device, issuedSecret } = await this.devices.register(dto);
    const token = await this.tokens.issueDeviceToken(device.id);
    return {
      deviceId: device.id,
      ...(issuedSecret ? { deviceSecret: issuedSecret } : {}),
      accessToken: token.accessToken,
      expiresIn: token.expiresIn,
    };
  }

  /**
   * Gives the calling device a new random address. Its secret is unchanged, so
   * the client only has to store the new ID; active sessions are ended.
   */
  @Post('me/change-id')
  @HttpCode(HttpStatus.OK)
  @UseGuards(DeviceAuthGuard)
  @Throttle({ default: { limit: 5, ttl: 60_000 } })
  async changeId(@CurrentDevice() device: DeviceRecord): Promise<ChangeDeviceIdResponse> {
    const moved = this.devices.changeId(device.id);
    const token = await this.tokens.issueDeviceToken(moved.id);
    return { deviceId: moved.id, accessToken: token.accessToken, expiresIn: token.expiresIn };
  }

  @Get('me')
  @UseGuards(DeviceAuthGuard)
  me(@CurrentDevice() device: DeviceRecord): DeviceSummary {
    return toDeviceSummary(device);
  }
}
