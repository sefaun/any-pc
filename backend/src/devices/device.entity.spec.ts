import { canBeControlled } from './device.entity';

describe('canBeControlled', () => {
  it('allows computers and rejects phones', () => {
    expect(canBeControlled('windows')).toBe(true);
    expect(canBeControlled('macos')).toBe(true);
    expect(canBeControlled('linux')).toBe(true);
    expect(canBeControlled('unknown')).toBe(true);
    expect(canBeControlled('android')).toBe(false);
    expect(canBeControlled('ios')).toBe(false);
  });
});
