# Duvar saati, UTC takvim ve kısmi NSDate

31 Ekim 2026 — yeni `time_bridge.c/.h` bağımsız MIT lisanslı kod. Özgün IPA veya oyun dosyaları GitHub'a eklenmedi.

## Yapılan

Dört C importu: `CFAbsoluteTimeGetCurrent`, `time`, `difftime`, `gmtime`. Ayrıca somut `NSDate` class/metaclass kaydı ve sınırlı gerçek yöntemler eklendi. Native class tablosu 66 isim + CFString kaydı için 67 slot oldu; kayıt öncesi NSDate init bağlantısı eklendi.

Duvar saati gerçek SDK `gettimeofday` çağrısından gelir; process uptime ile takvim saati karıştırılmaz. CF/NSDate reference epoch 2001-01-01, C Unix epoch 1970-01-01'dir; fark 978307200 saniye. CF double dönüşü açık base-AAPCS kullanır. `difftime`, signed32 uç değerleri integer overflow olmadan çıkarır. `time` yalnız dört byte yazar; başarısızlıkta output'u korur.

SDK ARM layout probu time_t=4, timeval=8, tm=36 byte verdi. iOS/Darwin tm için 9 integer'a gmtoff ve zone pointer eklenerek 44-byte çıktı oluşturulur (gmtoff offset36, zone40). `gmtime` UTC için GMT/offset0 verir; sonuç statik borrowed depodur ve sonraki çağrıda değişebilir. Genel thread-safe takvim API'si değildir.

NSDate: date/fixed-epoch factories, init/epoch initializers, epoch/difference getters, compare/equality ve copy. ObjC double giriş/sonuçları integer-bit ABI ile uygulanır. Dahili değer reference epoch'ta saklanır; `1e-9` gibi küçük reference aralıkları Unix epoch eklenmesiyle kaybolmaz. Factory'ler autorelease pool gerektirir. Sınıf kısmi olup timezone, formatter, subclasses, archiving ve genel takvim/locale davranışını sağlamaz.

## Owner çağrıları

Executable SHA256: `3ba6337a23ab7e08f2e025ec9731c53a7990e8d611b59eac07ba00cb63acc629`.

- `0x3e434–0x3e444`: difftime iki signed32 argüman r0/r1; double sonuç r0/r1'den d6'ya alınır.
- `0x3e76c–0x3e774`: gmtime bir time_t pointer alır.
- `0x3e794–0x3e798`: time(NULL).
- `0x16a190`: CFAbsoluteTimeGetCurrent çağrısı.

Bunlar salt okunur statik analizdir; orijinal ARM oyun kodu çalıştırılmadı.

## Doğrulama

Host ASan/UBSan üretim import router'ı, zaman modülü ve gerçek managed runtime'ı bağlar; sadece gettimeofday kontrollü fixture'dır. Epoch/fraction, signed32 sınır farkı, time output guard, leap-day/2038 UTC alanları ve GMT zone, NSDate fabrikası/init/karşılaştırma/copy/autorelease ömrü, nanosecond reference precision ve clock hata yolları sınanır. Native errno numaralarının Darwin'e genel uyarlaması bu modülün kapsamı değildir.

Tam Vita ARM ELF'inde **56 yeni üretim çağrısı**: gerçek `objc_msgSend` assembly dispatch, double argüman/sonuçlar, NSDate sınıfı ve gerçek pool/autorelease temizliği kullanıldı. C saat/takvim girişleri, 44-byte UTC layout, 1970/2001 epoch'ları ve küçük aralıkların korunması iki yığın hizasında kontrol edildi. gettimeofday/TLS/key/mutex OS hizmetleri fixture'dır; pthread_once gerçek initializer callback'ini çalıştırır. Gerçek Vita RTC/saat ve thread scheduling doğrulaması değildir.

Tam VitaSDK ELF linki, güçlü undefined kontrolü ve diğer host/ARM regresyonları geçti. Native özgün boot/device/pixel doğrulaması açık; SELF/VPK oluşturulmadı ve yürütme/release korumaları korunur.

## Sınırlar ve durum

Yalnız finite NSDate aralıkları ve desteklenen concrete sınıf yolları kabul edilir. Saat erişim hatasında CF/NSDate tarafı açıklayıcı fatal hata verir; C time -1 döner. Hostta signed32 dışındaki native clock değeri EOVERFLOW ile reddedilir; pinned SDK time_t zaten signed32'dir, 2038 sonrası RTC kapsamı iddia edilmez. Localtime/mktime/strftime, NSDateFormatter ve timezone/locale entegrasyonu hâlâ açık.

490 importun **337 adı eşleşir, 153'ü eksiktir**; sayı tamamlanma yüzdesi değildir. Gerekli 64 harici sınıfın 26'sı kısmi, 38'i uygulanmamıştır. NSDate dahil eklenen sınıflar bütün Foundation/UIKit davranışlarıyla tamamlanmış sayılmaz.

## GitHub CI

Kaynak `b448184449cc0eb34c916e9992d151f643bc8e2a`, [Actions 41237030060](https://github.com/Gh0sTRX/iron-man-2-mach-o/actions/runs/41237030060): hesap ödeme/harcama limiti uyarısıyla iş başlamadı, test adımı yoktur. Sonuçlar yerel managed ortamdan alınmıştır; yeni kaynak için CI başarısı iddia edilmez. Hesap ayarları değiştirilmedi ve engelli iş yeniden başlatılmadı.
