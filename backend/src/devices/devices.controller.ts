import { Body, Controller, Get, HttpCode, HttpStatus, Post, UseGuards } from '@nestjs/common';
import { Throttle } from '@nestjs/throttler';
import { CurrentDevice } from '../auth/current-device.decorator';
import { DeviceAuthGuard } from '../auth/device-auth.guard';
import { TokenService } from '../auth/token.service';
import { toDeviceSummary, type DeviceRecord, type DeviceSummary } from './device.entity';
import { DevicesService } from './devices.service';
import { RegisterDeviceDto, type RegisterDeviceResponse } from './dto/register-device.dto';

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

  @Get('me')
  @UseGuards(DeviceAuthGuard)
  me(@CurrentDevice() device: DeviceRecord): DeviceSummary {
    return toDeviceSummary(device);
  }
}
