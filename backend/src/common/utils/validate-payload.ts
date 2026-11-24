import { plainToInstance, type ClassConstructor } from 'class-transformer';
import { validateSync } from 'class-validator';
import { AppError } from '../errors/app-error';

/**
 * Validates an untrusted payload against a class-validator DTO.
 * Used where Nest's ValidationPipe is not in play (raw WebSocket frames).
 */
export function validatePayload<T extends object>(dto: ClassConstructor<T>, payload: unknown): T {
  if (typeof payload !== 'object' || payload === null || Array.isArray(payload)) {
    throw new AppError('VALIDATION_FAILED', 'Payload must be a JSON object');
  }
  const instance = plainToInstance(dto, payload);
  const errors = validateSync(instance, { whitelist: true, forbidNonWhitelisted: false });
  if (errors.length > 0) {
    const message = errors
      .flatMap((e) => Object.values(e.constraints ?? { [e.property]: 'is invalid' }))
      .join('; ');
    throw new AppError('VALIDATION_FAILED', message);
  }
  return instance;
}
