import { Injectable, Logger, OnModuleInit, UnauthorizedException } from '@nestjs/common';
import { Subject, type Observable } from 'rxjs';
import { StorageService } from '../storage/storage.service';
import type { JsonFileStore } from '../storage/json-file-store';
import {
  generateNumericId,
  generateSecret,
  hashSecret,
  verifySecret,
} from '../common/utils/crypto.util';
import { DEVICE_ID_LENGTH, type DeviceRecord } from './device.entity';
import type { RegisterDeviceDto } from './dto/register-device.dto';

export interface RegistrationResult {
  device: DeviceRecord;
  issuedSecret?: string;
}

export interface DeviceIdChange {
  oldId: string;
  newId: string;
}

@Injectable()
export class DevicesService implements OnModuleInit {
  private readonly logger = new Logger(DevicesService.name);
  private store: JsonFileStore<DeviceRecord>;
  private readonly idChanges = new Subject<DeviceIdChange>();

  /** Emitted after a device moved to a new address (see changeId). */
  get idChanges$(): Observable<DeviceIdChange> {
    return this.idChanges.asObservable();
  }

  constructor(private readonly storage: StorageService) {}

  async onModuleInit(): Promise<void> {
    this.store = await this.storage.open<DeviceRecord>('devices');
  }

  findById(id: string): DeviceRecord | undefined {
    return this.store.get(id);
  }

  /**
   * Registers a new device or re-authenticates an existing one.
   *
   * If the client presents credentials for an ID the server no longer knows
   * (e.g. the data volume was wiped), the ID is re-created with the presented
   * secret so that users keep their familiar address.
   */
  async register(dto: RegisterDeviceDto): Promise<RegistrationResult> {
    const now = new Date().toISOString();

    if (dto.deviceId && dto.deviceSecret) {
      const existing = this.store.get(dto.deviceId);
      if (existing) {
        if (!(await verifySecret(dto.deviceSecret, existing.secretHash))) {
          throw new UnauthorizedException('Invalid device credentials');
        }
        const updated: DeviceRecord = {
          ...existing,
          name: dto.name,
          platform: dto.platform,
          appVersion: dto.appVersion ?? existing.appVersion,
          lastSeenAt: now,
        };
        this.store.set(updated.id, updated);
        return { device: updated };
      }

      const restored = await this.create(dto.deviceId, dto.deviceSecret, dto, now);
      this.logger.log(`Restored unknown device ${restored.id} from client credentials`);
      return { device: restored };
    }

    const secret = generateSecret();
    const device = await this.create(this.allocateId(), secret, dto, now);
    this.logger.log(`Registered new device ${device.id} (${device.platform})`);
    return { device, issuedSecret: secret };
  }

  /**
   * Moves a device to a new random address. The secret stays the same; the old
   * address stops existing immediately, so nobody can reach the device under
   * it any more and its old access tokens are no longer accepted.
   */
  changeId(oldId: string): DeviceRecord {
    const existing = this.store.get(oldId);
    if (!existing) throw new UnauthorizedException('Unknown device');
    const moved: DeviceRecord = {
      ...existing,
      id: this.allocateId(),
      lastSeenAt: new Date().toISOString(),
    };
    this.store.set(moved.id, moved);
    this.store.delete(oldId);
    this.logger.log(`Device ${oldId} moved to ${moved.id}`);
    this.idChanges.next({ oldId, newId: moved.id });
    return moved;
  }

  touch(id: string): void {
    const device = this.store.get(id);
    if (device) this.store.set(id, { ...device, lastSeenAt: new Date().toISOString() });
  }

  private async create(
    id: string,
    secret: string,
    dto: RegisterDeviceDto,
    now: string,
  ): Promise<DeviceRecord> {
    const device: DeviceRecord = {
      id,
      secretHash: await hashSecret(secret),
      name: dto.name,
      platform: dto.platform,
      appVersion: dto.appVersion ?? null,
      createdAt: now,
      lastSeenAt: now,
    };
    this.store.set(id, device);
    return device;
  }

  private allocateId(): string {
    for (let attempt = 0; attempt < 100; attempt++) {
      const id = generateNumericId(DEVICE_ID_LENGTH);
      if (!this.store.has(id)) return id;
    }
    throw new Error('Unable to allocate a unique device id');
  }
}
