import { join } from 'node:path';
import { Injectable, OnApplicationShutdown } from '@nestjs/common';
import { AppConfig } from '../config/app.config';
import { JsonFileStore } from './json-file-store';

/** Factory/registry for file backed collections living under DATA_DIR. */
@Injectable()
export class StorageService implements OnApplicationShutdown {
  private readonly stores: { flush(): Promise<void> }[] = [];

  constructor(private readonly config: AppConfig) {}

  async open<T>(name: string): Promise<JsonFileStore<T>> {
    const store = new JsonFileStore<T>(join(this.config.dataDir, `${name}.json`));
    await store.load();
    this.stores.push(store);
    return store;
  }

  async onApplicationShutdown(): Promise<void> {
    await Promise.all(this.stores.map((s) => s.flush()));
  }
}
