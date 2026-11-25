export const DEVICE_PLATFORMS = ['windows', 'macos', 'linux', 'android', 'ios', 'unknown'] as const;
export type DevicePlatform = (typeof DEVICE_PLATFORMS)[number];

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
