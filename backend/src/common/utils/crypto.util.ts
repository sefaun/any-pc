import { randomBytes, randomInt, scrypt as scryptCb, timingSafeEqual } from 'node:crypto';
import { promisify } from 'node:util';

const scrypt = promisify(scryptCb) as (
  password: string,
  salt: Buffer,
  keylen: number,
) => Promise<Buffer>;

const KEY_LENGTH = 32;
const ALGORITHM = 'scrypt';

/** Hashes a secret as `scrypt:<salt>:<hash>` (base64url encoded parts). */
export async function hashSecret(secret: string): Promise<string> {
  const salt = randomBytes(16);
  const hash = await scrypt(secret, salt, KEY_LENGTH);
  return [ALGORITHM, salt.toString('base64url'), hash.toString('base64url')].join(':');
}

export async function verifySecret(secret: string, encoded: string): Promise<boolean> {
  const [algo, saltB64, hashB64] = encoded.split(':');
  if (algo !== ALGORITHM || !saltB64 || !hashB64) return false;
  const expected = Buffer.from(hashB64, 'base64url');
  const actual = await scrypt(secret, Buffer.from(saltB64, 'base64url'), expected.length);
  return expected.length === actual.length && timingSafeEqual(expected, actual);
}

export function generateSecret(bytes = 32): string {
  return randomBytes(bytes).toString('base64url');
}

/** Generates a human friendly numeric identifier that never starts with 0. */
export function generateNumericId(digits: number): string {
  let id = randomInt(1, 10).toString();
  for (let i = 1; i < digits; i++) id += randomInt(0, 10).toString();
  return id;
}
