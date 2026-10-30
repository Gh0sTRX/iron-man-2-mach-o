# NSNumber ve yeni kurulum dil tercihleri — 31 Ekim 2026

Özgün owner Mach-O'nun selector tablosunda `standardUserDefaults`, `objectForKey:`, `numberWithBool:`, `intValue` ve `floatValue` adları bulunuyor. `AppleLanguages` da özgün CFString sabitleri arasındadır. Bunlar statik envanterdir; bu turda orijinal ARM kodu çalıştırılmadı. Native portun menüsü/boot'u hâlâ cihaz üzerinde doğrulanmadı.

## Sağlanan kod

Bağımsız MIT `numbers.c/h`, gerçek immutable NSNumber kimliği ve integer/BOOL içeriği sağlar. Bool, signed/unsigned 32/64-bit factory'ler; integer/BOOL getter'ları, copy/equality, objCType ve ondalık stringValue/description desteklenir. Float/double getter'ları integer bit kalıbı döndürür; iOS SoftFP r0/r1 yerine Vita hard-float d0 kullanma hatasına düşmez. `numberWithBool:` yalnız argümanın düşük byte'ını kullanır; r2'nin üst byte'ları BOOL NO'yu YES yapamaz. Floating-point constructor'lar, generic hashing veya tam CFNumber/NSNumberFormatter davranışı uygulanmadı.

`defaults.c/h`, **yeni kurulum için salt okunur NSUserDefaults** sağlar. `standardUserDefaults` singleton'ının gerçek immutable sözlüğünde `AppleLanguages` → gerçek NSArray → NSString dil kodu vardır. objectForKey/arrayForKey/stringForKey/dictionaryRepresentation ve yaşam döngüsü uygulanmıştır. Bilinmeyen anahtar nil verir; yazma/synchronize metotları kaydedilmez.

Native başlangıç `sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG, ...)` ile gerçek Vita dil seçimini sorar. VitaSDK 0..19 dil enum'u BCP47 koduna çevrilir; örneğin 19→tr, 18→en-GB, 10→zh-Hant. Kritik enum değerleri SDK derlemesinde static_assert ile sabitlenir. Sorgu başarısız/kimlik bilinmiyorsa İngilizce varsaymak yerine başlangıç açıkça durur. Bu kod yolu cihazda henüz çalıştırılmadı; host testi provider eşlemesini ve verilmiş dilin doğru tutulmasını doğrular.

Yalnız tek sistem dili temsil edilir; iOS'un tam ordered language preferences/locale/region sistemi değildir. NSLocale veya ülke tahmini eklenmedi. Native uygulama tercihi yolu `ux0:data/ironman2_macho/Library/Preferences/com.gameloft.IronMan2.plist` mevcutsa, gerçek plist parser gelene kadar fresh-domain kurulumu reddedilir. Var olan tercihler silinmez, boşmuş gibi gösterilmez ve bu turda dosyaya yazılmaz. Dosya sistemi sorgusundaki ENOENT dışı hatalar da reddedilir.

## Grafik köprüsüne etkisi

EAGL drawable sözlüğündeki `EAGLDrawablePropertyRetainedBacking` artık gerçek NSNumber boolValue içeriğiyle kontrol edilir. **NSNumber NO** kabul edilir; **YES** preservation sözleşmesi doğrulanana kadar reddedilir. NSString gibi yanlış türler reddedilmeye devam eder. RGB565 + NSNumber NO birleşimi üretim EAGL IMP'leriyle test edildi. Önceki OES raporundaki "false dahil her retained-backing alanı reddedilir" sınırı bu düzeltmeyle daraltıldı; fiziksel GPU doğruluğu iddia edilmez.

## Test ve kanıt

- `tools/test_source.sh` ASan/UBSan: signed/unsigned sınırlar, bool byte truncation, integer equality/copy/ownership, float/double getter bitleri, decimal string, yeni kurulum dil listesi, yanlış/gömülü NUL anahtarlar, immutable snapshot ömrü, gerçek dosya varsa başlatmayı reddetme ve unsupported setters.
- EAGL testinde gerçek `+[NSNumber numberWithBool:]` IMP'i, autorelease pool ve gerçek sözlüklerle NO/YES/yanlış tür ile RGB565/varsayılan RGBA8 davranışları kontrol edildi. GPU işlemleri host testinde enjekte edilen fixture callback'leridir.
- **34 C/ASM birimi** VitaSDK ile tam ARM ELF'e bağlandı; güçlü undefined sembol yok. Linked import yönlendirme kontrolü ve mevcut **25** ARM/Thumb assembly senaryosu geçti.
- Owner class bildirim envanteri **24/64**, genel import adı eşleşmesi **211/490** (279 eksik). Bunlar tamamlanma yüzdesi veya eşleşen API'lerin davranış/ABI onayı değildir.

## Kalan hedef

Önce kalan gerçek başlangıç bağımlılıklarını ve selector/ABI gereksinimlerini çözerek native Vita açılışına ulaşmak gerekiyor. Ardından menü/grafik, UITouch ve kontroller, ses, oynanış ve kararlılık cihazda doğrulanmalı. İlk fiziksel boot logu olmadığı için kalan süre/güvenilir yüzde verilmiyor. Native timer çizim döngüsü mevcut olsa da tam UIKit/CFRunLoop ve oyun çalışması kanıtı yok. Altı runtime/cihaz/lisans kapısı açık değildir; SELF/VPK üretilmedi. Özgün IPA veya oyun asset'leri GitHub'a eklenmedi.
