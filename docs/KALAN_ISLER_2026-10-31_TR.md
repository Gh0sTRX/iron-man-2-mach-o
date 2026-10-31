# Iron Man 2 Mach-O portu: ne yapıldı, ne kaldı?

31 Ekim 2026 — `Gh0sTRX/iron-man-2-mach-o`, `experiment/ios-macho-loader`.

**Şu an:** Özgün oyunun 297 statik başlangıç işlevi ve kaydettikleri 359 kapanış callback'i Unicorn ARM emülatöründe tamamlandı; `_main` çağrılmadı. Yükleyici ve kısmi iOS uyumluluk kodu derleniyor; yerel otomatik testler geçiyor. Son GitHub Actions işi hesap ödeme/harcama limiti nedeniyle başlamadı; son kaynak commit için CI başarısı iddia edilmiyor. **Özgün oyun native Vita yükleyicisiyle henüz açılmadı.** Vita menüsü, oynanış ve fiziksel cihaz başarısı doğrulanmadı. Linux touchHLE'de görülen gerçek menü ayrı bir referans çalışmasıdır.

Bu rapor bağımsız Mach-O projesine aittir. `Iron-man-2-os` projesi değiştirilmedi; o projenin deneysel sahnesi bu yükleyicinin oyun çalıştırma kanıtı sayılmaz.

## Hazır olan altyapı

| Parça | Gerçekte yapılan | Kanıt sınırı |
|---|---|---|
| Özgün dosyalar | Owner IPA/executable hash kontrolü, Mach-O ve RBS1 analizi | Cihazda yürütme değil |
| Yükleyici | Segment/bounds, relocation ve constructor kontrolleri; sınıf/selector kayıtları | Yerel/sentetik test; tam native boot yok |
| Nesne ömrü | retain/release, autorelease pool, native/guest dispatch ayrımı | Tam Apple runtime değil |
| Metin ve veriler | 165 gerçek literal ile test; UTF-8/UTF-16, NSData ve paket/dosya yolları | Tüm Foundation API'leri değil |
| CF dizileri | Ölçülen NULL callback yolu: borrowed değerler, default allocator verisi; gerçek runtime ile 118 ARM çağrısı | Genel toll-free/custom callback desteği değil |
| Liste/sözlük/sayı | Sahipliği yönetilen collection'lar ve integral NSNumber | Bazı metot/formatlar eksik |
| C metin/numeric ABI | 10 SDK importu, 32-bit wide metin ve unsigned64→double; 56 gerçek ARM çağrısı | C-locale ASCII sınandı; locale/errno uyumu açık |
| Saat/NSDate | 1970/2001 epoch ve UTC tm dönüşümü, kısmi NSDate; gerçek objc_msgSend ile 56 ARM çağrısı | RTC, timezone/formatter ve yerel saat doğrulaması açık |
| C++ sayıdan metin | 8 stream/Init importu; 112 ARM çağrısı ve özgün inline width/fill düzeni | Genel C++ exception/RTTI değil |
| Eski C++ metin sistemi | 24 import: COW string ve atomik referans sayacı; 32 gerçek ARM çağrı testi | Tam C++ exception/RTTI/iostream uyumluluğu değil |
| C++ listeler/ağaçlar | 8 import; SDK algoritmalarıyla düğüm ekleme/silme/splice/iterator, 948 ARM çağrısı | Bütün STL/iostream/RTTI desteği değil |
| CoreGraphics değerleri | 5 import; rotation/scale/rect offset ve gerçek sıfır verileri, 18 ARM çağrısı | GPU çizimi veya UIView transform desteği değil |
| Matematik | 23 eksik libm/sayı okuma çağrısına açık iOS ABI köprüsü; 56 gerçek ARM test çağrısı | Fiziksel Vita/oyun sayısal doğrulaması değil |
| Ayar/dil | Yeni kurulum için gerçek sistem dilini sorgulayan salt okunur AppleLanguages yolu | Eski tercih plist'leri, ülke/locale ve kaydetme eksik |
| UIKit başlangıcı | AppController delegate başlatma, pencere/view sahipliği, ekran ve bazı frame/bounds metotları | Tam UIKit widget/event sistemi değil |
| Doku/sunum | Son 9 GLES import adı bağlandı; iki unit/env/query ve sunum state restorasyonu, 127 ARM çağrısı | SDK/GPU modellenmiş; gerçek piksel/format/swap doğrulaması açık |
| Model verisi / indexed draw | Vertex/color/normal/iki texture dizisi, güvenli fixed dönüşümü ve u8/u16 DrawElements; 95 ARM çağrısı | VitaGL girişleri yakalandı; modelin GPU görüntüsü ve performansı açık |
| GLES komut aktarımı | 19 renk/derinlik/sis/matris/malzeme/ışık importu; 100 ARM çağrısı | Backend girişleri yakalandı; GPU/ışıklandırma görüntüsü doğrulanmadı |
| Grafik | EAGLContext/layer, OES kaynak eşlemesi, RGB565/RGBA8 ve depth16 backend yolu | Host fixture ve ARM derleme; GPU görüntüsü cihazda doğrulanmadı |
| Çizim döngüsü | Özgün 1/60 saniyelik NSTimer/draw yolu için scheduler ve native sürücü | Özgün draw fonksiyonu Vita'da çalıştırılmadı |

## Önümüzdeki işler — önem sırası

### 1. İlk gerçek açılışı engelleyen iOS bağımlılıkları

Özgün 490 import adının **338'i** tablolarla eşleşiyor; **152 ad hâlâ eksik**. Gerekli 64 harici framework sınıfından **26'sı kısmi**, **38'i uygulanmamış**. Eşleşen adların hepsi doğru davranış/ABI ile doğrulanmış değildir. Bu sayılar tamamlanma yüzdesi değildir.

Eksik Apple/CoreFoundation/UIKit API'leri, C/C++ runtime bağlantıları, bazı GLES çağrıları ve sistem/dosya/ağ işlevleri gerçek davranışlarıyla ele alınmalı. `NSLocale`, `NSRunLoop`, çeşitli UIKit/medya sınıfları hâlâ eksik örneklerdir. NSBundle/tercih plist okuma ve gerekirse diğer veri formatları tamamlanmalı; oyun sürümü/ayarları uydurulmamalı.

Kısmi sınıflarda gereken eksik selector'lar da çözülmeli. ObjC `+initialize/+load`, kategori/protocol, C++ ömrü ve aggregate/SoftFP dönüş yolları özgün çağrı yerleriyle doğrulanmalı. Yalnız sınıfın adını eklemek bu işi bitirmez.

**İlk hedef:** Özgün constructor → main → AppController → EAGL surface → ilk çizim zincirinin ilerlediğini somut logla görmek. Yükleyici eksik zorunlu importlar varken geçişi durduruyor; şu an bu zincir çalışmış değildir.

### 2. Vita'da ARM yürütme ve bellek/cache doğrulaması

RBS1 ve relocation host kontrolleri mevcut; executable mapping, runtime slide, ARM/Thumb geçişleri, cache senkronizasyonu ve constructor'ların gerçek Vita koşullarında doğru çalıştığı henüz kanıtlanmadı. Native guest yürütme koruması bu kanıt yerine kaldırılmış sayılmaz. İlk cihaz logu alınmalı, gerçek hata/çökme noktası ölçülmeli.

### 3. Menünün gerçekten ve doğru çizilmesi

OES/EAGL kodu mevcut, fakat GPU'da texture/format/draw, ekran döndürme ve ölçek, GL state geri yükleme ve sürekli sunum test edilmeli. Stencil, bağlı storage'ın yeniden boyutlandırılması ve çoklu context gibi kapsamlar açık. İsmi bağlanmış grafik çağrılarının davranışları da incelenmeli. İlk çizim ile doğru/kararlı menü ayrı doğrulama adımlarıdır.

### 4. Dokunma ve Vita kontrolleri

Native UITouch/UIEvent/NSSet oluşturma ve özgün touch callback'lerine gönderme kaynak kodu eklendi; kimlik/phase/capture/cancel, çoklu dokunma ve CGPoint stret host testleri geçti. Kodun donanım ve özgün guest tarafında çalışması henüz doğrulanmadı. Gerçek cihazda dokunma yönü/konumu, event sırası ve oyun tepkisi test edilmeli. Native UIControl/gesture davranışı, multi-tap ve gamepad/analog eşlemeleri hâlâ açık. Ayrıntı: [TOUCH_INPUT_PROGRESS_2026-10-31_TR.md](TOUCH_INPUT_PROGRESS_2026-10-31_TR.md).

### 5. Ses

Ses köprüsü kaynakları var; müzik/efektlerin doğru kaynak dosyaları, decoder/format, buffer, callback ABI'si, başlatma/duraklatma ve senkronizasyon gerçek akışta kontrol edilmeli. Import adlarının eşleşmesi veya derleme sesin çalıştığını kanıtlamaz. Özgün sesin Vita'da duyulduğuna dair doğrulama yok.

### 6. Oynanış ve kararlılık

Menü sonrasında bölüm yükleme, hareket/uçuş, animasyon, çatışma, özgün düşmanlar, kamera, görevler, kayıt/yükleme ve uzun oturum test edilmeli. Bunları bu yolda baştan yazma hedefi yok; özgün ARM motorunu kullanmayı amaçlıyoruz. Motor çalışmadan özgün animasyon/combat sisteminin doğru portlandığı söylenemez. Performans, bellek kullanımı ve sızıntılar cihazda ölçülmeli.

### 7. Test paketi ve dağıtım

Paketleme betiği yazmak tek başına ana sorun değil. Sekiz mevcut VPK kabul koşulundan iki derleme koşulu sağlanıyor. Tüm importlar, runtime, cihaz RBS1/RX/cache, grafik/girdi/ses ve kaynak/lisans envanteriyle ilgili kalan altı koşul henüz doğrulanmadı. Upstream lisans durumu netleştirilmeli; özgün oyun dosyaları pakete/GitHub'a izinsiz eklenmemeli. Bu oturumda SELF/VPK üretilmedi.

## Bu raporla birlikte yapılan yeni düzeltme

Vita `ux0:` / `ur0:` / `app0:` yolları, dosya URL oluşturma ve geri çözme aşamasında mutlak yerel yol olarak desteklendi. Örneğin `ux0:data/.../clip space.bin`, `file:///ux0:data/.../clip%20space.bin` olarak roundtrip yapıyor. Remote file authority yerel yol sayılmıyor.

CFBundleGetMainBundle, CFBundleCopyBundleURL, CFBundleCopyResourcesDirectoryURL, CFURLCopyFileSystemPath ve CFURLGetFileSystemRepresentation mevcut NSBundle/NSURL/NSString koduna bağlandı. Get/Copy sahiplikleri, sınır kontrolleri ve gerçek geçici dosyadan veri okuma test edildi. Bu, kaynak dosyalarına erişim bağlantısındaki bir engeli çözer; native oyun boot'u anlamına gelmez. CFBundle subset yalnız main bundle / flat iOS app ve absolute local file URL içindir; relative/base URL çözümü, macOS bundle layout'u veya tam CFBundle metadata API'si sağlanmadı.

## Son kontrol sayıları

- 49 C/C++/ASM biriminin VitaSDK ARM ELF bağlantısı başarılı; güçlü undefined ELF sembolü yok.
- Yerel host ASan/UBSan testleri başarılı; yeni CFBundle testi gerçek import router'ını ve kaynak dosyası byte'larını kullanıyor.
- Üretim assembly'siyle 25 sentetik ARM/Thumb dispatch ve 6 üretim UIKit CGRect ABI emülasyon senaryosu başarılı.
- Özgün oyunun native Vita boot'u, menü ve cihaz testi: **doğrulanmadı**.

Tam eksik isim envanteri [REMAINING_BINDINGS_2026-10-31.json](REMAINING_BINDINGS_2026-10-31.json), yerel derleme kanıtı [BUILD_EVIDENCE_2026-10-31.json](BUILD_EVIDENCE_2026-10-31.json) içindedir.

**Ne kadar kaldı?** Oynanabilir Vita sürümüne kadar ciddi entegrasyon ve cihaz testi işi var. Henüz güvenilir gün veya yüzde tahmini verilemiyor. En yakın ölçülebilir hedef ilk native Vita çizimi; bundan sonra menü ve kontrollerin kabul testi. Eksik 152 adı aynı büyüklükte işler saymak yanıltıcı olur; tek bir kritik ABI hatası bile açılışı durdurabilir.

Yeni C++ katmanı ve özgün çağrı yerleri: [LEGACY_CPP_PROGRESS_2026-10-31_TR.md](LEGACY_CPP_PROGRESS_2026-10-31_TR.md).

Matematik ve sayı okuma: [MATH_ABI_PROGRESS_2026-10-31_TR.md](MATH_ABI_PROGRESS_2026-10-31_TR.md).

Yeni düğüm ve geometri kodu: [CONTAINERS_GEOMETRY_PROGRESS_2026-10-31_TR.md](CONTAINERS_GEOMETRY_PROGRESS_2026-10-31_TR.md).

CF dizi sahipliği ve runtime entegrasyonu: [CFARRAY_PROGRESS_2026-10-31_TR.md](CFARRAY_PROGRESS_2026-10-31_TR.md).

GLES state/ışık/matris köprüleri: [GLES_STATE_PROGRESS_2026-10-31_TR.md](GLES_STATE_PROGRESS_2026-10-31_TR.md).

Client arrays ve indeksli model çizimi: [CLIENT_ARRAYS_PROGRESS_2026-10-31_TR.md](CLIENT_ARRAYS_PROGRESS_2026-10-31_TR.md).

Doku, sorgu ve üretim sunum akışı: [TEXTURE_PRESENT_PROGRESS_2026-10-31_TR.md](TEXTURE_PRESENT_PROGRESS_2026-10-31_TR.md).

C metinleri ve sayı dönüşümü: [C_TEXT_PROGRESS_2026-10-31_TR.md](C_TEXT_PROGRESS_2026-10-31_TR.md).

C++ stream ve inline ABI: [CPP_STREAM_PROGRESS_2026-10-31_TR.md](CPP_STREAM_PROGRESS_2026-10-31_TR.md).

Zaman ve NSDate: [TIME_DATE_PROGRESS_2026-10-31_TR.md](TIME_DATE_PROGRESS_2026-10-31_TR.md).

Özgün başlangıç kodu ve gerçek kapanış kuyruğu: [OWNER_STARTUP_PROGRESS_2026-10-31_TR.md](OWNER_STARTUP_PROGRESS_2026-10-31_TR.md).
