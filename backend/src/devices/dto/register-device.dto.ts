import { Transform } from 'class-transformer';
import { IsIn, IsOptional, IsString, Length, Matches, ValidateIf } from 'class-validator';
import { DEVICE_ID_PATTERN, DEVICE_PLATFORMS, type DevicePlatform } from '../device.entity';

const trim = ({ value }: { value: unknown }) => (typeof value === 'string' ? value.trim() : value);

export class RegisterDeviceDto {
  /** Previously issued identifier. Omit on first launch. */
  @IsOptional()
  @Matches(DEVICE_ID_PATTERN, { message: 'deviceId must be a 9 digit number' })
  deviceId?: string;

  /** Secret issued together with deviceId. Required when deviceId is provided. */
  @ValidateIf((o: RegisterDeviceDto) => o.deviceId !== undefined)
  @IsString()
  @Length(32, 128)
  deviceSecret?: string;

  @Transform(trim)
  @IsString()
  @Length(1, 64)
  name: string;

  @IsIn(DEVICE_PLATFORMS)
  platform: DevicePlatform;

  @IsOptional()
  @IsString()
  @Length(1, 32)
  appVersion?: string;
}

export interface RegisterDeviceResponse {
  deviceId: string;
  /** Only returned when a new secret was issued; clients must persist it. */
  deviceSecret?: string;
  accessToken: string;
  expiresIn: number;
}
