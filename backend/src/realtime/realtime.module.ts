import { Module } from '@nestjs/common';
import { ConnectionsModule } from '../connections/connections.module';
import { CloudflareSfuClient } from './cloudflare/cloudflare-sfu.client';
import { CloudflareTurnClient } from './cloudflare/cloudflare-turn.client';
import { RtcController } from './rtc.controller';
import { RtcService } from './rtc.service';

@Module({
  imports: [ConnectionsModule],
  controllers: [RtcController],
  providers: [RtcService, CloudflareSfuClient, CloudflareTurnClient],
})
export class RealtimeModule {}
