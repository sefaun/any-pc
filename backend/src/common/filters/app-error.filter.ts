import { ArgumentsHost, Catch, ExceptionFilter } from '@nestjs/common';
import type { Response } from 'express';
import { AppError } from '../errors/app-error';

/** Maps {@link AppError} to a consistent JSON HTTP error body. */
@Catch(AppError)
export class AppErrorFilter implements ExceptionFilter<AppError> {
  catch(exception: AppError, host: ArgumentsHost): void {
    const response = host.switchToHttp().getResponse<Response>();
    response.status(exception.httpStatus).json({
      statusCode: exception.httpStatus,
      code: exception.code,
      message: exception.message,
    });
  }
}
