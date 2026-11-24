import {
  IsBoolean,
  IsObject,
  IsOptional,
  IsString,
  IsUUID,
  Length,
  Matches,
  MaxLength,
} from 'class-validator';
import { DEVICE_ID_PATTERN } from '../../devices/device.entity';

abstract class RequestEnvelope {
  /** Optional client generated id echoed back in the response/error frame. */
  @IsOptional()
  @IsString()
  @MaxLength(64)
  requestId?: string;
}

export class ConnectionRequestDto extends RequestEnvelope {
  @Matches(DEVICE_ID_PATTERN, { message: 'targetId must be a 9 digit number' })
  targetId: string;

  @IsOptional()
  @IsString()
  @Length(1, 128)
  password?: string;
}

export class ConnectionRespondDto extends RequestEnvelope {
  @IsUUID()
  connectionId: string;

  @IsBoolean()
  accept: boolean;
}

export class ConnectionEndDto extends RequestEnvelope {
  @IsUUID()
  connectionId: string;
}

export class ConnectionSignalDto extends RequestEnvelope {
  @IsUUID()
  connectionId: string;

  @IsObject()
  payload: Record<string, unknown>;
}
