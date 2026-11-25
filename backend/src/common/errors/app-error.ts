/**
 * Domain level error with a stable, machine readable code. Transport layers
 * (HTTP filter, WebSocket gateway) translate it to their own representation.
 */
export class AppError extends Error {
  constructor(
    readonly code: AppErrorCode,
    message: string,
    readonly httpStatus = 400,
  ) {
    super(message);
    this.name = 'AppError';
  }
}

export type AppErrorCode =
  | 'VALIDATION_FAILED'
  | 'UNAUTHORIZED'
  | 'DEVICE_NOT_FOUND'
  | 'DEVICE_OFFLINE'
  | 'DEVICE_NOT_CONTROLLABLE'
  | 'SELF_CONNECTION'
  | 'CONNECTION_NOT_FOUND'
  | 'CONNECTION_NOT_ACTIVE'
  | 'CONNECTION_FORBIDDEN'
  | 'CONNECTION_ALREADY_PENDING'
  | 'RTC_SESSION_MISSING'
  | 'RTC_PEER_NOT_READY'
  | 'RTC_FORBIDDEN'
  | 'UPSTREAM_ERROR'
  | 'UNKNOWN_EVENT'
  | 'INTERNAL_ERROR';
