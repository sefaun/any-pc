export const DEVICE_PLATFORMS = ['windows', 'macos', 'linux', 'android', 'ios', 'unknown'] as const;
export type DevicePlatform = (typeof DEVICE_PLATFORMS)[number];

/** Phones run the viewer only: they can control computers but cannot be controlled. */
const VIEWER_ONLY_PLATFORMS: readonly DevicePlatform[] = ['android', 'ios'];

export function canBeControlled(platform: DevicePlatform): boolean {
  return !VIEWER_ONLY_PLATFORMS.includes(platform);
}

export const DEVICE_ID_LENGTH = 9;
export const DEVICE_ID_PATTERN = /^[1-9]\d{8}$/;

export interface DeviceRecord {
  id: string;
  secretHash: string;
  name: string;
  platform: DevicePlatform;
  appVersion: string | null;
  createdAt: string;
  lastSeenAt: string;
}

/** Public projection of a device, safe to send to other clients. */
export interface DeviceSummary {
  id: string;
  name: string;
  platform: DevicePlatform;
}

export function toDeviceSummary(device: DeviceRecord): DeviceSummary {
  return { id: device.id, name: device.name, platform: device.platform };
}
