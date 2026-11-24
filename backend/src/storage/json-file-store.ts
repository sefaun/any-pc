import { mkdir, readFile, rename, writeFile } from 'node:fs/promises';
import { dirname } from 'node:path';
import { Logger } from '@nestjs/common';

/**
 * Minimal persistent key/value collection backed by a single JSON file.
 *
 * - Everything is kept in memory; the file is only a snapshot.
 * - Writes are debounced and atomic (write to temp file + rename).
 * - A corrupt or missing file simply results in an empty collection,
 *   which is acceptable for this service (data loss is tolerated).
 */
export class JsonFileStore<T> {
  private readonly logger: Logger;
  private readonly items = new Map<string, T>();
  private flushTimer: NodeJS.Timeout | null = null;
  private pendingFlush: Promise<void> = Promise.resolve();

  constructor(
    private readonly filePath: string,
    private readonly flushDelayMs = 250,
  ) {
    this.logger = new Logger(`JsonFileStore(${filePath})`);
  }

  async load(): Promise<void> {
    try {
      const raw = await readFile(this.filePath, 'utf8');
      const parsed = JSON.parse(raw) as Record<string, T>;
      for (const [key, value] of Object.entries(parsed)) this.items.set(key, value);
      this.logger.log(`Loaded ${this.items.size} record(s)`);
    } catch (error) {
      if ((error as NodeJS.ErrnoException).code !== 'ENOENT') {
        this.logger.warn(`Could not read store, starting empty: ${(error as Error).message}`);
      }
    }
  }

  get(key: string): T | undefined {
    return this.items.get(key);
  }

  has(key: string): boolean {
    return this.items.has(key);
  }

  values(): IterableIterator<T> {
    return this.items.values();
  }

  get size(): number {
    return this.items.size;
  }

  set(key: string, value: T): void {
    this.items.set(key, value);
    this.scheduleFlush();
  }

  delete(key: string): boolean {
    const removed = this.items.delete(key);
    if (removed) this.scheduleFlush();
    return removed;
  }

  /** Flushes any pending change immediately. Safe to call multiple times. */
  async flush(): Promise<void> {
    if (this.flushTimer) {
      clearTimeout(this.flushTimer);
      this.flushTimer = null;
      this.pendingFlush = this.pendingFlush.then(() => this.writeSnapshot());
    }
    await this.pendingFlush;
  }

  private scheduleFlush(): void {
    if (this.flushTimer) return;
    this.flushTimer = setTimeout(() => {
      this.flushTimer = null;
      this.pendingFlush = this.pendingFlush.then(() => this.writeSnapshot());
    }, this.flushDelayMs);
  }

  private async writeSnapshot(): Promise<void> {
    const tmpPath = `${this.filePath}.tmp`;
    try {
      await mkdir(dirname(this.filePath), { recursive: true });
      const snapshot = Object.fromEntries(this.items);
      await writeFile(tmpPath, JSON.stringify(snapshot, null, 2), 'utf8');
      await rename(tmpPath, this.filePath);
    } catch (error) {
      this.logger.error(`Failed to persist store: ${(error as Error).message}`);
    }
  }
}
