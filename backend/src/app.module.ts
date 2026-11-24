import { Module } from '@nestjs/common';
import { APP_GUARD } from '@nestjs/core';
import { ThrottlerGuard, ThrottlerModule } from '@nestjs/throttler';
import { AuthModule } from './auth/auth.module';
import { ConfigModule } from './config/config.module';
import { ConnectionsModule } from './connections/connections.module';
import { HealthModule } from './health/health.module';
import { RealtimeModule } from './realtime/realtime.module';
import { SignalingModule } from './signaling/signaling.module';
import { StorageModule } from './storage/storage.module';

@Module({
  imports: [
    ConfigModule,
    StorageModule,
    ThrottlerModule.forRoot([{ name: 'default', ttl: 60_000, limit: 600 }]),
    AuthModule,
    ConnectionsModule,
    SignalingModule,
    RealtimeModule,
    HealthModule,
  ],
  providers: [{ provide: APP_GUARD, useClass: ThrottlerGuard }],
})
export class AppModule {}
