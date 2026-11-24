import { Logger } from '@nestjs/common';
import { AppError } from '../../common/errors/app-error';

const REQUEST_TIMEOUT_MS = 15_000;

export type HttpMethod = 'GET' | 'POST' | 'PUT';

/**
 * Thin JSON-over-HTTPS helper for Cloudflare APIs. Upstream failures are
 * normalised to {@link AppError} with code `UPSTREAM_ERROR` (HTTP 502) while
 * keeping Cloudflare's public `errorCode`/`errorDescription` in the message.
 */
export async function cloudflareRequest<T>(
  logger: Logger,
  method: HttpMethod,
  url: string,
  bearer: string,
  body?: unknown,
): Promise<T> {
  let response: Response;
  try {
    response = await fetch(url, {
      method,
      headers: {
        Authorization: `Bearer ${bearer}`,
        ...(body !== undefined ? { 'Content-Type': 'application/json' } : {}),
      },
      body: body !== undefined ? JSON.stringify(body) : undefined,
      signal: AbortSignal.timeout(REQUEST_TIMEOUT_MS),
    });
  } catch (error) {
    logger.error(`${method} ${redact(url)} failed: ${(error as Error).message}`);
    throw new AppError('UPSTREAM_ERROR', 'Cloudflare API is unreachable', 502);
  }

  const text = await response.text();
  let json: unknown = undefined;
  if (text.length > 0) {
    try {
      json = JSON.parse(text);
    } catch {
      json = undefined;
    }
  }

  if (!response.ok) {
    const { errorCode, errorDescription } = (json ?? {}) as {
      errorCode?: string;
      errorDescription?: string;
    };
    const detail = [errorCode, errorDescription].filter(Boolean).join(': ') || text.slice(0, 200);
    logger.warn(`${method} ${redact(url)} -> ${response.status} ${detail}`);
    throw new AppError(
      'UPSTREAM_ERROR',
      `Cloudflare API error (${response.status})${detail ? `: ${detail}` : ''}`,
      502,
    );
  }

  return (json ?? {}) as T;
}

/** Keeps logs free of identifiers that are not useful for debugging. */
function redact(url: string): string {
  return url.replace(/\/apps\/[^/]+/, '/apps/:appId').replace(/\/keys\/[^/]+/, '/keys/:keyId');
}
