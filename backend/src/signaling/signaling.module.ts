import { Module } from '@nestjs/common';
import { ConnectionsModule } from '../connections/connections.module';
import { PresenceService } from './presence.service';
import { SignalingGateway } from './signaling.gateway';

@Module({
  imports: [ConnectionsModule],
  providers: [SignalingGateway, PresenceService],
  exports: [PresenceService],
})
export class SignalingModule {}
