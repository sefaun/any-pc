import { createParamDecorator, ExecutionContext } from '@nestjs/common';
import type { DeviceRecord } from '../devices/device.entity';
import type { AuthenticatedRequest } from './device-auth.guard';

/** Injects the authenticated device resolved by {@link DeviceAuthGuard}. */
export const CurrentDevice = createParamDecorator(
  (_data: unknown, context: ExecutionContext): DeviceRecord =>
    context.switchToHttp().getRequest<AuthenticatedRequest>().device,
);
