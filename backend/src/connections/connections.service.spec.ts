import { firstValueFrom, take, toArray } from 'rxjs';
import { AppError } from '../common/errors/app-error';
import type { AppConfig } from '../config/app.config';
import { ConnectionsService } from './connections.service';

describe('ConnectionsService', () => {
  const config = { connectionRequestTimeoutMs: 1_000 } as AppConfig;
  let service: ConnectionsService;

  beforeEach(() => {
    jest.useFakeTimers();
    service = new ConnectionsService(config);
  });

  afterEach(() => {
    service.onModuleDestroy();
    jest.useRealTimers();
  });

  it('creates a pending connection and lets the host accept it', () => {
    const connection = service.request('111111111', '222222222');
    expect(connection.state).toBe('pending');

    service.accept(connection.id, '222222222');
    expect(connection.state).toBe('active');
    expect(service.requireActiveParticipant(connection.id, '111111111').role).toBe('viewer');
  });

  it('rejects self connections and duplicate pending requests', () => {
    expect(() => service.request('111111111', '111111111')).toThrow(AppError);
    service.request('111111111', '222222222');
    expect(() => service.request('111111111', '222222222')).toThrow(/already pending/);
  });

  it('only allows the host to answer', () => {
    const connection = service.request('111111111', '222222222');
    expect(() => service.accept(connection.id, '111111111')).toThrow(/Only the host/);
  });

  it('expires unanswered requests', async () => {
    const events = firstValueFrom(service.events$.pipe(take(1)));
    const connection = service.request('111111111', '222222222');
    jest.advanceTimersByTime(1_001);
    await expect(events).resolves.toMatchObject({ type: 'expired' });
    expect(connection.state).toBe('ended');
  });

  it('ends every live connection of a disconnected device', async () => {
    const events = firstValueFrom(service.events$.pipe(take(2), toArray()));
    const a = service.request('111111111', '222222222');
    const b = service.request('333333333', '111111111');
    service.accept(a.id, '222222222');

    expect(service.endAllFor('111111111')).toHaveLength(2);
    const emitted = await events;
    expect(emitted.every((e) => e.type === 'ended' && e.reason === 'peer_disconnected')).toBe(true);
    expect(b.state).toBe('ended');
  });
});
