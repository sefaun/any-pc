import { plainToInstance, Transform } from 'class-transformer';
import {
  IsIn,
  IsInt,
  IsNotEmpty,
  IsOptional,
  IsString,
  IsUrl,
  Max,
  Min,
  validateSync,
} from 'class-validator';

const LOG_LEVELS = ['error', 'warn', 'log', 'debug', 'verbose'] as const;

const toInt = ({ value }: { value: unknown }) =>
  value === undefined || value === '' ? undefined : Number(value);

/**
 * Raw environment contract. Every variable the service reads is declared here
 * so that misconfiguration fails fast at boot instead of at first use.
 */
export class EnvironmentVariables {
  @IsOptional()
  @Transform(toInt)
  @IsInt()
  @Min(1)
  @Max(65535)
  PORT = 3000;

  @IsOptional()
  @IsString()
  CORS_ORIGINS = '*';

  @IsOptional()
  @IsIn(LOG_LEVELS)
  LOG_LEVEL: (typeof LOG_LEVELS)[number] = 'log';

  @IsOptional()
  @IsString()
  JWT_SECRET?: string;

  @IsOptional()
  @Transform(toInt)
  @IsInt()
  @Min(60)
  JWT_TTL_SECONDS = 86_400;

  @IsOptional()
  @IsString()
  DATA_DIR = './data';

  @IsOptional()
  @Transform(toInt)
  @IsInt()
  @Min(5)
  @Max(600)
  CONNECTION_REQUEST_TIMEOUT_SECONDS = 60;

  @IsOptional()
  @IsUrl({ require_tld: false })
  CF_REALTIME_API_BASE = 'https://rtc.live.cloudflare.com/v1';

  @IsString()
  @IsNotEmpty()
  CF_REALTIME_APP_ID: string;

  @IsString()
  @IsNotEmpty()
  CF_REALTIME_APP_SECRET: string;

  @IsString()
  @IsNotEmpty()
  CF_TURN_KEY_ID: string;

  @IsString()
  @IsNotEmpty()
  CF_TURN_KEY_API_TOKEN: string;

  @IsOptional()
  @Transform(toInt)
  @IsInt()
  @Min(60)
  CF_TURN_CREDENTIAL_TTL_SECONDS = 86_400;
}

export function validateEnvironment(config: Record<string, unknown>): EnvironmentVariables {
  const validated = plainToInstance(EnvironmentVariables, config, {
    enableImplicitConversion: false,
    exposeDefaultValues: true,
  });
  const errors = validateSync(validated, { skipMissingProperties: false });
  if (errors.length > 0) {
    const details = errors
      .map((e) => `  - ${e.property}: ${Object.values(e.constraints ?? {}).join(', ')}`)
      .join('\n');
    throw new Error(`Invalid environment configuration:\n${details}`);
  }
  return validated;
}
