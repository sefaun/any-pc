import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { JsonFileStore } from '../storage/json-file-store';
import type { StorageService } from '../storage/storage.service';
import type { DeviceIdChange } from './devices.service';
import { DevicesService } from './devices.service';

describe('DevicesService.changeId', () => {
  let dir: string;
  let service: DevicesService;

  beforeEach(async () => {
    dir = await mkdtemp(join(tmpdir(), 'anypc-devices-'));
    const storage = {
      open: <T>(name: string) =>
        Promise.resolve(new JsonFileStore<T>(join(dir, `${name}.json`), 0)),
    } as unknown as StorageService;
    service = new DevicesService(storage);
    await service.onModuleInit();
  });

  afterEach(async () => {
    await rm(dir, { recursive: true, force: true });
  });

  it('moves the device to a new random address and keeps its secret', async () => {
    const { device, issuedSecret } = await service.register({
      name: 'Office PC',
      platform: 'windows',
    });
    const changes: DeviceIdChange[] = [];
    service.idChanges$.subscribe((c) => changes.push(c));

    const moved = service.changeId(device.id);

    expect(moved.id).toMatch(/^[1-9]\d{8}$/);
    expect(moved.id).not.toBe(device.id);
    expect(moved.secretHash).toBe(device.secretHash);
    expect(moved.name).toBe('Office PC');
    expect(service.findById(device.id)).toBeUndefined();
    expect(service.findById(moved.id)).toBeDefined();
    expect(changes).toEqual([{ oldId: device.id, newId: moved.id }]);

    // The same secret now authenticates the new address.
    const again = await service.register({
      name: 'Office PC',
      platform: 'windows',
      deviceId: moved.id,
      deviceSecret: issuedSecret,
    });
    expect(again.device.id).toBe(moved.id);
    expect(again.issuedSecret).toBeUndefined();
  });

  it('rejects unknown devices', () => {
    expect(() => service.changeId('123456789')).toThrow();
  });
});
