import 'reflect-metadata';
import { Logger, ValidationPipe, type LogLevel } from '@nestjs/common';
import { NestFactory } from '@nestjs/core';
import { WsAdapter } from '@nestjs/platform-ws';
import type { NestExpressApplication } from '@nestjs/platform-express';
import { AppModule } from './app.module';
import { AppErrorFilter } from './common/filters/app-error.filter';
import { AppConfig } from './config/app.config';

const LEVELS: LogLevel[] = ['fatal', 'error', 'warn', 'log', 'debug', 'verbose'];

async function bootstrap(): Promise<void> {
  const app = await NestFactory.create<NestExpressApplication>(AppModule, { bufferLogs: true });
  const config = app.get(AppConfig);

  app.useLogger(LEVELS.slice(0, LEVELS.indexOf(config.logLevel) + 1));
  app.set('trust proxy', true);
  app.disable('x-powered-by');
  app.useBodyParser('json', { limit: '512kb' });
  app.setGlobalPrefix('api/v1');
  app.enableCors({
    origin: config.corsOrigins === '*' ? true : config.corsOrigins,
    methods: ['GET', 'POST', 'PUT'],
    allowedHeaders: ['Authorization', 'Content-Type'],
  });
  app.useGlobalPipes(
    new ValidationPipe({ whitelist: true, forbidNonWhitelisted: true, transform: true }),
  );
  app.useGlobalFilters(new AppErrorFilter());
  app.useWebSocketAdapter(new WsAdapter(app));
  app.enableShutdownHooks();

  await app.listen(config.port, '0.0.0.0');
  Logger.log(`AnyPC backend listening on :${config.port} (HTTP /api/v1, WS /ws)`, 'Bootstrap');
}

bootstrap().catch((error: unknown) => {
  console.error(error instanceof Error ? error.message : error);
  process.exit(1);
});
