# AnyPC

AnyDesk benzeri, platformlar arası uzak masaüstü uygulaması.

- **Masaüstü istemcisi** (`desktop/`): Qt 6 / C++17. Windows, macOS ve Linux (X11).
- **Backend** (`backend/`): NestJS / TypeScript. Cihaz kaydı, WebSocket sinyalleşme ve Cloudflare Realtime SFU aracılığı.
- **Medya ve veri taşıma**: [Cloudflare Realtime SFU](https://developers.cloudflare.com/realtime/sfu/) + [Cloudflare TURN](https://developers.cloudflare.com/realtime/turn/).

Her cihaz 9 haneli bir adres alır (ör. `482 913 507`). Bu adresi bilen biri bağlantı isteği gönderir; karşı taraf isteği kabul eder ya da **gözetimsiz erişim** şifresi tanımlıysa şifreyle doğrudan bağlanılır. Bağlantı kurulunca ekran görüntülenir, klavye ve fare uzaktan kullanılır, pano metni iki yönlü eşitlenir.

---

## Mimari

```
 ┌──────────────── Viewer (kontrol eden) ───────────────┐         ┌──────────────── Host (kontrol edilen) ───────────────┐
 │ Qt UI ── ViewerWindow (QWebEngineView)                │         │ Qt UI ── HostSession (görünmez QWebEnginePage)        │
 │            │  viewer.js: video + input yakalama       │         │            │  host.js: ekran yakalama + yayın          │
 │            ▼                                          │         │            ▼                                          │
 │        RtcBridge (QWebChannel) ──► ApiClient ─────────┼──┐   ┌──┼──── ApiClient ◄── RtcBridge ──► InputInjector       │
 │        SignalingClient (QWebSocket) ──────────────────┼─┐│   │┌─┼──── SignalingClient       (SendInput/CGEvent/XTest) │
 └───────────────────────────────────────────────────────┘ ││   ││ └───────────────────────────────────────────────────────┘
                     WebRTC (video + DataChannel)          ││   ││          WebRTC (video + DataChannel)
                ┌───────────────────────────────┐          ││   ││
                │  Cloudflare Realtime SFU/TURN │◄─────────┼┼───┼┼──────────────┐
                └───────────────▲───────────────┘          ││   ││              │
                                │ HTTPS (App Secret)       ▼▼   ▼▼              │
                        ┌───────┴──────────────────────────────────────┐        │
                        │ AnyPC Backend (NestJS, Docker)               │────────┘
                        │  /api/v1  REST   ·   /ws  sinyalleşme        │
                        └──────────────────────────────────────────────┘
```

**Akış**

1. İstemci açılışta `POST /api/v1/devices/register` ile kayıt olur (ilk açılışta 9 haneli ID + gizli anahtar alır, sonraki açılışlarda aynı ID ile yeni token alır) ve `/ws` WebSocket'ine bağlanır.
2. Viewer `connection.request` gönderir. Host'a `connection.incoming` gelir; kullanıcı kabul eder veya gözetimsiz erişim şifresi doğrulanırsa otomatik kabul edilir.
3. **Host**: ekranı yakalar, Cloudflare SFU'ya `screen` adlı video track olarak yayınlar, DataChannel transport'u kurar ve `control` adlı DataChannel'ı yayınlar. Sonra sinyal kanalından `host-ready` duyurur.
4. **Viewer**: `screen` track'ine abone olur, `control` kanalına **`canReply: true`** ile abone olur ve klavye/fare olaylarını bu kanal üzerinden host'a gönderir. `canReply` yetkisini backend yalnızca kontrol eden tarafa verir.
5. Host, gelen olayları işletim sisteminin API'leriyle enjekte eder.

SFU çağrıları Cloudflare'in güncel [Connection API](https://developers.cloudflare.com/realtime/sfu/api/) dokümanındaki sıralamalarla yapılır: `sessions/new` → `tracks/new` → (gerekirse) `renegotiate` → `datachannels/establish` → `datachannels/new`. **App Secret ve TURN anahtarı yalnızca backend'de durur.** İstemciler SFU'ya backend üzerinden erişir. Backend her isteği bağlantıya ve role göre yetkilendirir: istemci yalnızca kendi oturumunda işlem yapabilir, `remote` kaynaklarda karşı tarafın oturum ID'sini backend doldurur.

> Cloudflare, 30 saniye boyunca paket almayan track'leri siler. Ekran sabitken yakalama kare üretmeyi durdurabildiği için host, son kareyi düşük hızla tekrar gönderir (`frame-keepalive.js`).

---

## Depo yapısı

```
backend/                     NestJS servis
  src/config/                ortam değişkenleri doğrulama + tipli AppConfig
  src/storage/               JSON dosya tabanlı kalıcı depo (DATA_DIR)
  src/auth/                  cihaz token'ları (JWT) ve guard
  src/devices/               kayıt / yeniden kimlik doğrulama
  src/connections/           bağlantı yaşam döngüsü (bellek içi)
  src/signaling/             WebSocket gateway + presence
  src/realtime/              Cloudflare SFU/TURN istemcileri + yetkili proxy
  Dockerfile
desktop/                     Qt 6 istemcisi
  src/app/                   kompozisyon kökü, kimlik doğrulama döngüsü
  src/core/                  ayarlar, şifre hash'leme, ID biçimleme
  src/network/               REST istemcisi, sinyalleşme istemcisi, protokol
  src/rtc/                   QWebEngine ortamı, RtcPage, RtcBridge (JS <-> C++)
  src/session/               HostSession, ViewerSession, SessionManager, pano eşitleme
  src/input/                 input modeli, tuş eşleme, platform enjektörleri
  src/ui/                    ana pencere, diyaloglar, viewer penceresi, tema
  resources/web/             gömülü WebRTC uygulaması (host.js, viewer.js, ...)
  packaging/                 Windows (Inno Setup), macOS (DMG), Linux (AppImage)
.github/workflows/           CI ve release pipeline'ları
```

---

## Cloudflare hazırlığı

Cloudflare panelinde **Realtime** bölümünden:

1. **SFU uygulaması** oluşturun → `App ID` ve `App Secret`.
2. **TURN anahtarı** oluşturun → `Turn Key ID` ve `API Token`.

Bu dört değer backend'e ortam değişkeni olarak verilir.

---

## Backend'i çalıştırma (Docker)

Her `v*` etiketinde image GitHub Container Registry'ye yayınlanır: `ghcr.io/sefaun/any-pc-backend`.

```bash
docker run -d --name anypc-backend --restart unless-stopped \
  -p 3000:3000 \
  -v anypc-data:/app/data \
  -e JWT_SECRET='uzun-rastgele-bir-deger' \
  -e CF_REALTIME_APP_ID='...' \
  -e CF_REALTIME_APP_SECRET='...' \
  -e CF_TURN_KEY_ID='...' \
  -e CF_TURN_KEY_API_TOKEN='...' \
  ghcr.io/sefaun/any-pc-backend:latest
```

veya `backend/.env.example` dosyasını `.env` olarak kopyalayıp `--env-file .env` kullanın (`backend/docker-compose.example.yml` örneği de var).

> GHCR paketleri varsayılan olarak **private** oluşturulur. Sunucuda `docker login ghcr.io` yapın ya da paketi GitHub → Packages → any-pc-backend → *Package settings* üzerinden public yapın.

Üretimde backend'i TLS sonlandıran bir reverse proxy (Caddy, Nginx, Traefik, Cloudflare Tunnel) arkasında çalıştırın. Proxy'nin WebSocket upgrade'ini `/ws` yolu için iletmesi gerekir. İstemci `https://` adresinde otomatik olarak `wss://` kullanır.

| Değişken | Varsayılan | Açıklama |
| --- | --- | --- |
| `PORT` | `3000` | HTTP/WS portu |
| `CORS_ORIGINS` | `*` | Virgülle ayrılmış origin listesi |
| `LOG_LEVEL` | `log` | `error`, `warn`, `log`, `debug`, `verbose` |
| `JWT_SECRET` | *(rastgele)* | Cihaz token imza anahtarı (≥16 karakter). Verilmezse her açılışta yeniden üretilir; istemciler sessizce yeniden kayıt olur |
| `JWT_TTL_SECONDS` | `86400` | Token ömrü. İstemci süre dolmadan yeniler |
| `DATA_DIR` | `/app/data` | Cihaz kayıtlarının tutulduğu dizin (`devices.json`) |
| `CONNECTION_REQUEST_TIMEOUT_SECONDS` | `60` | Cevaplanmayan bağlantı isteğinin zaman aşımı |
| `CF_REALTIME_API_BASE` | `https://rtc.live.cloudflare.com/v1` | Cloudflare Realtime API adresi |
| `CF_REALTIME_APP_ID` | **zorunlu** | SFU App ID |
| `CF_REALTIME_APP_SECRET` | **zorunlu** | SFU App Secret |
| `CF_TURN_KEY_ID` | **zorunlu** | TURN Key ID |
| `CF_TURN_KEY_API_TOKEN` | **zorunlu** | TURN Key API Token |
| `CF_TURN_CREDENTIAL_TTL_SECONDS` | `86400` | Üretilen TURN kimlik bilgisinin ömrü |

Veritabanı ve Redis yoktur: cihazlar `DATA_DIR` altındaki JSON dosyasında, bağlantılar ve çevrimiçi durumu bellekte tutulur (tek instance için tasarlandı). Volume kaybolursa istemciler sakladıkları ID ve anahtarla **aynı ID'yi** otomatik olarak geri alır.

### API özeti

| Yöntem | Yol | Açıklama |
| --- | --- | --- |
| `POST` | `/api/v1/devices/register` | Kayıt / token yenileme |
| `GET` | `/api/v1/devices/me` | Token sahibi cihaz |
| `GET` | `/api/v1/rtc/ice-servers` | Kısa ömürlü STUN/TURN bilgileri |
| `POST` | `/api/v1/connections/:id/rtc/sessions` | Bu bağlantı için SFU oturumu aç |
| `POST` | `/api/v1/connections/:id/rtc/tracks` | Track yayınla / abone ol |
| `PUT` | `/api/v1/connections/:id/rtc/renegotiate` | SFU teklifine cevap |
| `PUT` | `/api/v1/connections/:id/rtc/tracks/close` | Track kapat |
| `POST` | `/api/v1/connections/:id/rtc/datachannels/establish` | DataChannel transport'u |
| `POST` | `/api/v1/connections/:id/rtc/datachannels` | DataChannel yayınla / abone ol |
| `PUT` | `/api/v1/connections/:id/rtc/datachannels/close` | DataChannel kapat |
| `GET` | `/api/v1/health` | Sağlık kontrolü |
| `WS` | `/ws` | Sinyalleşme (`Authorization: Bearer <token>` veya `?token=`) |

WebSocket mesajları `{"event": "...", "data": {...}}` biçimindedir. Olayların listesi: `backend/src/signaling/signaling.protocol.ts`.

### Yerel geliştirme

```bash
cd backend
cp .env.example .env   # Cloudflare değerlerini doldurun
npm install
npm run start:dev
```

```bash
npm run lint && npm run typecheck && npm test
```

---

## Masaüstü uygulaması

### Kurulum dosyaları

Her release'te şu dosyalar yayınlanır:

| Platform | Dosya |
| --- | --- |
| Windows 10/11 x64 | `AnyPC-Setup-<sürüm>-x64.exe` (Inno Setup; isteğe bağlı masaüstü kısayolu ve oturum açılışında başlatma) |
| macOS 12+ (Apple Silicon + Intel) | `AnyPC-<sürüm>-macOS.dmg` |
| Linux x86_64 (X11) | `AnyPC-<sürüm>-x86_64.AppImage` |

**Sunucu adresi build'e gömülmez.** Uygulama ilk açıldığında sunucu adresini sorar (ör. `anypc.example.com`; şema yazılmazsa `https` kabul edilir), `/api/v1/health` ile doğrular ve kaydeder. Sonradan **File → Settings → Server → Change…** ile değiştirilebilir. Aynı kurulum dosyası farklı sunucularla kullanılabilir.

### Kullanım

- **This computer**: cihazın adresi. Kopyalayıp karşı tarafa iletin.
- **Remote computer**: bağlanılacak adresi yazıp **Connect**'e basın. Karşı tarafta gözetimsiz erişim açıksa şifreyi de yazın.
- **Gözetimsiz erişim**: *Configure unattended access…* ile şifre belirleyin. Şifre cihazda PBKDF2-SHA256 ile hash'lenmiş olarak saklanır ve doğrulama host tarafında yapılır. Yanlış şifreyle gelen istek otomatik reddedilir.
- Pencere kapatıldığında uygulama sistem tepsisinde çalışmaya devam eder (ayarlardan kapatılabilir).
- Viewer penceresi: **Disconnect**, **Full screen**, **Send clipboard**, **Send files**, **Transfers** ve (birden fazla monitör varsa) **ekran seçici**. Bu düğmelerin klavye kısayolu yoktur; tüm tuşlar uzak bilgisayara gider.
- **Çoklu monitör:** Kontrol eden taraf araç çubuğundaki listeden uzak bilgisayarın hangi ekranını göreceğini seçer; görüntü yeniden bağlanmadan değişir, fare/klavye seçilen ekrana yönlenir.
- **Ekran çerçevesi:** Kontrol edilen bilgisayarda paylaşılan ekranın kenarlarında kırmızı bir çerçeve ve "AnyPC · … is viewing this screen" etiketi görünür. Çerçeve tıklamaları engellemez.
- **Dosya aktarımı:** İki yönlüdür. Viewer'da **Send files**, kontrol edilen tarafta ana penceredeki **Send files…** düğmesiyle dosya seçilir. Gelen dosyalar `İndirilenler/AnyPC` klasörüne kaydedilir (aynı isim varsa `ad (1).uzantı`). Aktarım penceresinde ilerleme görünür.
- **Bildirimler:** Karşı taraf bağlantıyı kapatırsa ya da bağlantı koparsa, kapatılana kadar ekranda kalan bir bilgi penceresi ve sistem tepsisi bildirimi gösterilir.

### Platform notları

| Platform | Not |
| --- | --- |
| **macOS** | İlk bağlantıda *Ekran Kaydı* ve *Erişilebilirlik* izinleri istenir (Sistem Ayarları → Gizlilik ve Güvenlik). Build ad-hoc imzalıdır; Gatekeeper uyarısında uygulamaya sağ tıklayıp *Aç* deyin. Dağıtım için Developer ID imzası + notarization ekleyin (`packaging/macos/build-dmg.sh`). |
| **Windows** | UIPI kuralı gereği yönetici olarak çalışan pencerelere input gönderebilmek için AnyPC'nin de yönetici olarak çalışması gerekir. UAC onay ekranı ve kilit ekranı (secure desktop) yakalanamaz. Installer imzasızdır, SmartScreen uyarı verebilir. |
| **Linux** | Uzaktan kontrol için **X11** oturumu gerekir (XTest). Wayland'de ekran görüntülenebilir ama input enjeksiyonu desteklenmez; viewer "view only" moduna geçer. |

Ekran seçimi Qt 6.7+ ile derlenen sürümlerde (release'ler 6.8) çalışır. Daha eski Qt ile kaynak koddan derlenirse masaüstünün tamamı tek görüntü olarak paylaşılır.

### Sorun giderme

- **Log dosyası:** **Help → Open log folder** (`anypc.log`). Windows'ta `%LOCALAPPDATA%\AnyPC\AnyPC\logs`, macOS'ta `~/Library/Application Support/AnyPC/AnyPC/logs`, Linux'ta `~/.local/share/AnyPC/AnyPC/logs`.
- **Ekran gelirken uygulama kapanıyorsa:** büyük olasılıkla GPU sürücüsü kaynaklıdır. Uygulamayı `--safe-mode` ile başlatın (Windows'ta Başlat menüsünde **AnyPC (Safe Mode)** kısayolu) veya `ANYPC_SAFE_MODE=1` ortam değişkenini tanımlayın; GPU hızlandırması tamamen kapatılır.

### Kaynaktan derleme

Gereksinimler: CMake ≥ 3.21, C++17 derleyici, Qt ≥ 6.4 (release'ler 6.8 LTS ile derlenir). Gerekli Qt modülleri: WebEngine, WebChannel, WebSockets, Positioning. Linux'ta ayrıca `libxtst-dev`.

```bash
cmake -S desktop -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Aynı makinede iki istemciyi denemek için her birine ayrı bir profil verin (ayrı ayar ve cihaz kimliği kullanır):

Birim testleri:

```bash
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

```bash
./build/anypc --profile a
```

```bash
./build/anypc --profile b
```

---

## Release süreci

```bash
git tag v1.0.0
```

```bash
git push origin v1.0.0
```

Alternatif olarak GitHub'da **Actions → Release → Run workflow** ile sürüm numarası girerek de başlatılabilir; bu durumda `v<sürüm>` etiketi seçilen commit üzerinde otomatik oluşturulur.

`.github/workflows/release.yml` şunları yapar:

1. Backend için lint, typecheck ve testleri çalıştırır, ardından `linux/amd64` + `linux/arm64` Docker image'ını GHCR'ye gönderir: `ghcr.io/sefaun/any-pc-backend:1.0.0`, `:1.0` ve `:latest`. Image'a `org.opencontainers.image.source` etiketi eklendiği için repo sayfasında **Packages** altında görünür.
2. Qt 6.8 ile Windows, macOS ve Linux istemcilerini derleyip paketler.
3. Kurulum dosyalarını ve `SHA256SUMS.txt`'yi etikete ait GitHub Release'e ekler. `-rc` gibi ön ek taşıyan sürümler prerelease olarak işaretlenir.

İsteğe bağlı repo değişkeni (**Settings → Secrets and variables → Actions → Variables**):

- `ANYPC_SERVER_URL`: ilk açılıştaki sunucu ekranında önerilecek adres (ör. `https://anypc.example.com`). Zorunlu değildir; kullanıcı adresi her durumda kendisi onaylar.

`ci.yml`, `main`'e yapılan her push'ta ve her PR'da backend testlerini, Docker build'ini ve Linux istemci build'ini çalıştırır.
