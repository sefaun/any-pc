import 'package:anypc_mobile/src/core/device_id.dart';
import 'package:anypc_mobile/src/core/recent_connections.dart';
import 'package:anypc_mobile/src/core/server_url.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('DeviceId', () {
    test('normalizes, validates and formats', () {
      expect(DeviceId.normalize(' 482 913-507 '), '482913507');
      expect(DeviceId.isValid('482913507'), isTrue);
      expect(DeviceId.isValid('082913507'), isFalse);
      expect(DeviceId.isValid('48291350'), isFalse);
      expect(DeviceId.format('482913507'), '482 913 507');
    });
  });

  group('normalizeServerUrl', () {
    final cases = {
      'anypc.example.com': 'https://anypc.example.com',
      'https://anypc.example.com/': 'https://anypc.example.com',
      'http://localhost:3000': 'http://localhost:3000',
      'https://a.example.com/?x=1#f': 'https://a.example.com',
      'https://example.com/anypc/': 'https://example.com/anypc',
    };
    cases.forEach((input, expected) {
      test(input, () => expect(normalizeServerUrl(input).toString(), expected));
    });
    test('rejects unusable input', () {
      expect(normalizeServerUrl('   '), isNull);
      expect(normalizeServerUrl('ftp://example.com'), isNull);
    });
    test('joinUrl keeps the base path', () {
      expect(joinUrl(Uri.parse('https://example.com/anypc'), '/api/v1/health').toString(),
          'https://example.com/anypc/api/v1/health');
    });
  });

  group('RecentConnectionList', () {
    test('records attempts most recent first and counts them', () {
      var list = RecentConnectionList();
      list = list.recordAttempt('111111111').recordAttempt('222222222').recordAttempt('111111111');
      list = list.recordAttempt('012345678'); // invalid, ignored
      expect(list.entries.map((e) => e.id), ['111111111', '222222222']);
      expect(list.entries.first.connectionCount, 2);
      expect(list.entries.first.lastConnected, isNotNull);
    });

    test('updates details, renames, removes and survives encoding', () {
      var list = RecentConnectionList()
          .recordAttempt('333333333')
          .updateDetails('333333333', name: 'Office PC', platform: 'windows')
          .recordAttempt('444444444')
          .rename('444444444', '  Home Mac  ');
      final decoded = RecentConnectionList.decode(list.encode());
      expect(decoded.find('333333333')!.displayName, 'Office PC');
      expect(decoded.find('333333333')!.platform, 'windows');
      expect(decoded.find('444444444')!.displayName, 'Home Mac');
      list = decoded.remove('333333333');
      expect(list.find('333333333'), isNull);
    });

    test('display name falls back to the formatted id', () {
      expect(const RecentConnection(id: '555666777').displayName, '555 666 777');
    });

    test('caps the history and tolerates corrupt data', () {
      var list = RecentConnectionList();
      for (var i = 0; i < RecentConnectionList.maxEntries + 5; i++) {
        list = list.recordAttempt('${100000000 + i}');
      }
      expect(list.entries.length, RecentConnectionList.maxEntries);
      expect(RecentConnectionList.decode('not json').entries, isEmpty);
      expect(RecentConnectionList.decode('[{"id":"bad"},{"id":"123456789"}]').entries.single.id, '123456789');
    });
  });
}
