import { Global, Module } from '@nestjs/common';
import { JwtModule } from '@nestjs/jwt';
import { DevicesModule } from '../devices/devices.module';
import { DeviceAuthGuard } from './device-auth.guard';
import { TokenService } from './token.service';

@Global()
@Module({
  imports: [JwtModule.register({}), DevicesModule],
  providers: [TokenService, DeviceAuthGuard],
  exports: [TokenService, DeviceAuthGuard, DevicesModule],
})
export class AuthModule {}
