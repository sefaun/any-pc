# AnyPC Mobile

Flutter ile yazılmış Android ve iOS istemcisi. Telefondan, AnyPC masaüstü uygulaması çalışan bilgisayarları kontrol etmek için kullanılır (viewer rolü). Masaüstüyle aynı backend'i ve aynı Cloudflare Realtime SFU protokolünü kullanır; backend'de ayrıca bir ayar gerekmez.

## Özellikler

- İlk açılışta sunucu adresi sorulur ve `/api/v1/health` ile doğrulanır.
- 9 haneli adresle bağlanma, gözetimsiz erişim şifresi desteği.
- **Recent sessions**: Daha önce bağlanılan bilgisayarlar listelenir. Dokununca bağlanır; uzun basınca yeniden adlandır, adresi kopyala ve sil seçenekleri çıkar. Liste sadece telefonda saklanır.
- Uzak ekran: Cloudflare SFU üzerinden WebRTC video.
- Dokunmatik kontrol:
  | Hareket | Etki |
  | --- | --- |
  | Tek dokunuş | Sol tık |
  | Uzun basma veya iki parmakla dokunma | Sağ tık |
  | Tek parmakla sürükleme | Sol tuş basılı sürükleme (seçim, pencere taşıma) |
  | Kaydırma modu açıkken sürükleme | Fare tekerleği |
  | İki parmakla sıkıştırma/kaydırma | Görüntüyü yakınlaştırma ve kaydırma (yerel) |
- Klavye: Telefon klavyesiyle yazılan metin Unicode olarak gönderilir, Türkçe karakterler sorunsuz yazılır. Özel tuş çubuğunda Esc, Tab, Ctrl/Alt/Shift/Win (yapışkan), oklar, Home/End, PgUp/PgDn, Del, F1–F12 ve Ctrl+Alt+Del var.
- Uzak bilgisayarda birden fazla monitör varsa ekran seçici.
- İki yönlü dosya aktarımı: Telefondan dosya gönderilir; bilgisayardan gelen dosyalar uygulama klasörüne (`Documents/AnyPC`) kaydedilir ve paylaş menüsüyle açılabilir. iOS'ta Dosyalar uygulamasında "AnyPC" altında da görünür.
- Pano metnini bilgisayara gönderme; bilgisayardan gelen pano metni telefona kopyalanır.
- Karşı taraf bağlantıyı kapatınca bildirim.

Telefonun kendisi uzaktan kontrol edilemez; gelen bağlantı istekleri otomatik reddedilir. Android'de bu özellik ileride Accessibility servisiyle eklenebilir. iOS buna izin vermiyor.

## Mimari

```
lib/
  main.dart                 uygulama girişi
  src/app_controller.dart   kayıt/token, sinyalleşme, bağlantı yaşam döngüsü, son bağlantılar
  src/core/                 ayarlar (SharedPreferences), adres/URL yardımcıları, geçmiş listesi
  src/net/                  REST istemcisi, WebSocket sinyalleşme, protokol sabitleri
  src/rtc/                  SFU oturumu (backend proxy), viewer bağlantısı, dosya aktarımı
  src/input/                dokunma → fare eşlemesi, yakınlaştırma, yazılım klavyesi
  src/ui/                   ekranlar (kurulum, ana ekran, uzak ekran, aktarımlar)
test/                       birim ve widget testleri
```

Protokol masaüstüyle birebir aynıdır (`desktop/resources/web/js/viewer.js` ve `file-transfer.js`). Kontrol mesajlarına mobil klavye için `{"t":"tx","s":"..."}` (Unicode metin) eklendi; masaüstü host bunu 0.4 ve sonrasında destekler.

## Geliştirme

Gerekenler: Flutter 3.47+, Android SDK (Android için), Xcode (iOS için).

```bash
cd mobile
flutter pub get
flutter run
```

Kontroller:

```bash
flutter analyze
```

```bash
flutter test
```

## Derleme ve dağıtım

`.github/workflows/mobile.yml`:

1. `mobile` dalına her push'ta (ve `mobile-v*` etiketlerinde) analiz ve testleri çalıştırır.
2. Android APK ve AAB ile imzasız iOS IPA derler.
3. `mobile-v<sürüm>` etiketiyle bir GitHub Release oluşturur. Sürüm `pubspec.yaml`'dan alınır; aynı sürüm tekrar derlenirse release güncellenir. Mobil release'ler masaüstünün "Latest" işaretini değiştirmez.

Yeni sürüm için `pubspec.yaml`'daki `version` değerini artırıp `mobile` dalına push'lamak yeterli.

### Android imzalama (isteğe bağlı)

Secret tanımlanmazsa APK debug anahtarıyla imzalanır: doğrudan kurulabilir, ama Play Store'a yüklenemez ve farklı imzalı sürümün üzerine güncellenemez. Kalıcı imza için:

```bash
keytool -genkey -v -keystore anypc-release.jks -keyalg RSA -keysize 2048 -validity 10000 -alias anypc
```

Repo **Settings → Secrets and variables → Actions** altına şunları ekleyin:

- `ANDROID_KEYSTORE_BASE64`: `base64 -w0 anypc-release.jks` çıktısı
- `ANDROID_KEYSTORE_PASSWORD`
- `ANDROID_KEY_ALIAS` (örn. `anypc`)
- `ANDROID_KEY_PASSWORD`

Yerel derleme için aynı bilgiler `android/key.properties` dosyasına (`storeFile`, `storePassword`, `keyAlias`, `keyPassword`) yazılabilir. Bu dosya git'e eklenmez.

### iOS

Apple, imzasız uygulamaların kurulmasına izin vermez. Seçenekler:

- **Kendi cihazına kurmak:** IPA'yı Sideloadly veya AltStore ile kendi Apple ID'nle imzalayıp kur. Ücretsiz hesapla 7 gün geçerli olur.
- **TestFlight / App Store:** Apple Developer Program üyeliği gerekir (yıllık 99 USD). Sertifika ve provisioning profile secret olarak eklenirse workflow'a imzalama ve TestFlight yükleme adımı eklenebilir.
