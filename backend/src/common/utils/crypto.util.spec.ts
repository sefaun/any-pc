import { generateNumericId, generateSecret, hashSecret, verifySecret } from './crypto.util';

describe('crypto utils', () => {
  it('hashes and verifies secrets', async () => {
    const secret = generateSecret();
    const hash = await hashSecret(secret);
    await expect(verifySecret(secret, hash)).resolves.toBe(true);
    await expect(verifySecret(`${secret}x`, hash)).resolves.toBe(false);
    await expect(verifySecret(secret, 'garbage')).resolves.toBe(false);
  });

  it('generates 9 digit ids without a leading zero', () => {
    for (let i = 0; i < 200; i++) expect(generateNumericId(9)).toMatch(/^[1-9]\d{8}$/);
  });
});
