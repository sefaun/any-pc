import { Controller, Get } from '@nestjs/common';
import { SkipThrottle } from '@nestjs/throttler';
import { PresenceService } from '../signaling/presence.service';

@Controller('health')
@SkipThrottle()
export class HealthController {
  private readonly startedAt = Date.now();

  constructor(private readonly presence: PresenceService) {}

  @Get()
  check() {
    return {
      status: 'ok',
      uptimeSeconds: Math.round((Date.now() - this.startedAt) / 1000),
      onlineDevices: this.presence.onlineCount,
    };
  }
}
