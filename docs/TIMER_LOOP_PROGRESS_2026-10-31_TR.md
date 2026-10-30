# Özgün NSTimer çizim yoluna native destek — 31 Ekim 2026

Özgün executable'ın SHA256'sı `3ba6337a23ab7e08f2e025ec9731c53a7990e8d611b59eac07ba00cb63acc629`. `-[AppController applicationDidFinishLaunching:]` içinde **0x505e8** adresindeki objc_msgSend için:

- Sınıf referansı: **0x4e4d6c**, NSTimer.
- Factory selector referansı: **0x4e4614**, `scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:`.
- Callback selector referansı: **0x4e4618**, `draw`.
- `r2=0x11111111`, `r3=0x3f911111`: little-endian IEEE754 double **1/60 saniye**.
- Stack'te target AppController (`r6`), selector draw, nil userInfo, repeats=1 hazırlanıyor.
- Özgün `-[AppController draw]`: **0x50820**. Timer'a bir parametre almayı şart koşup bu zero-argument selector'ı reddetmek özgün başlangıç yolunu bozardı.

Bunlar orijinal ARM komutları ve literal/selector tablolarından statik ölçümlerdir; bu oturumda özgün guest kod çalıştırılmadı. Depoya oyun kodu/payload eklenmedi.

## Eklenen kod

Bağımsız MIT `timers.c/h`, ana thread/default-mode için gerçek NSTimer selector scheduling sağlar. Uygulama tarafından kaydedilmiş target/selector kullanılır; döngüde tahmin edilen veya sabit bir draw fonksiyonuna atlanmaz. Zamanlayıcı target ve userInfo'yu retain eder. Invalidate scheduler sahipliğini ve target'ı bırakır; userInfo timer dealloc'a kadar korunur. Tek seferlik timer callback sonrasında invalid olur.

Owner factory'nin double argümanı iOS SoftFP'de r2/r3'tedir. Native Vita hard-float `double` prototipi kullanılmaz: iki uint32 word alınır, bitler double'a çözülür. `timeInterval` dönüşü de iOS r0/r1 düzeni için uint64 bitler olarak döner. Gecikmeler mikro saniyeye yukarı yuvarlanır; 1/60 saniye 16.667 µs olur. Sonlu olmayan veya bir yıldan uzun interval reddedilir; sıfır/negatif süre 100 µs'ye ayarlanır.

Runtime target metadata'sına method type encoding eklendi. Callback yalnız void dönüşlü, zero-argument veya tek object-argument metotsa çalıştırılır. Native callback doğru C prototipiyle, guest callback gerçek `call_guest` köprüsüyle çağrılır. Desteklenmeyen return/aggregate/parametre ABI'leri hata verir.

Scheduler 64 zamanlayıcıyla sınırlı ve yalnız init thread'inde çalışır. Tick başlangıcında due timer snapshot'ı retain edilir; callback sırasında iptal/ekleme snapshot'ı bozmaz. Kaçırılan tekrarlı tick'ler burst halinde çalıştırılmaz; bir turda timer başına bir çağrı yapılır, faz korunarak sonraki deadline hesaplanır. Geri giden saat, deadline taşması, reentrant pump ve cross-thread kullanım reddedilir. Her due batch'in ayrı autorelease pool'u vardır.

`uikit_loop.c/h` input polling, due timer çağrıları ve en fazla 10 ms bekleme ile çalışan native sürücüdür. Native UIApplicationMain, delegate launch sonrasında desteklenen scheduled timer yoksa açıkça durur; varsa launch pool'u boşaltıp bu sürücüye girer. Vita'da döngü uygulama ömrü boyunca sürer; testte kontrollü stop callback'i kullanılır. Bu sınırlı timer döngüsü, önceki koşulsuz "run loop not implemented" duruşunun yerine gelir.

## Gerçekte doğrulanan

- `tools/test_source.sh`: ASan/UBSan; owner 1/60 raw word'leri, native/guest callback seçimi, tek seferlik/repeating/manual fire, missed-tick coalescing, self/other cancellation, callback içinde yeni timer, target/info lifetime, per-turn pool, başarısız factory, thread/clock/capacity/overflow korumaları ve kontrollü loop beklemeleri.
- UIKit bootstrap testi launch pool'unun delegate hayattayken boşaltılmasını doğrular.
- Üretim ARM dispatcher/stack alignment assembly'si **25 senaryoda** emüle edildi. Yeni iki senaryo 4/8-byte guest stack hizalamalarında double word'lerini ve dört stack argümanını kontrol eder. Bunlar sentetik ARM IMP fixture'leridir; özgün NSTimer/oyun komutları değildir.
- **32 C/ASM birimi** VitaSDK ile tam ARM ELF'e bağlandı; güçlü undefined sembol yok. Linked import yönlendirme testi geçti.

## Açık sınırlar

Bu tam Foundation run loop veya UIKit event sistemi değildir. NSRunLoop nesneleri, CFRunLoopRunInMode, sockets/sources/modes, NSTimer fireDate/tolerance, background-thread timers ve touch dispatch uygulanmadı. `input_poll` donanımı örnekler; henüz UITouch/NSSet oluşturarak özgün EAGLView touch metotlarını çağırmaz. Döngü tarafından örnek alınan girdi oyuna iletilmiş sayılmaz.

Kalan importlar yüzünden gerçek native boot hâlâ engelli. Özgün timer factory/draw ve sunum zinciri Vita üzerinde çalıştırılmadı; görsel doğruluk veya fiziksel cihaz başarısı iddia edilmiyor. Altı runtime/cihaz/lisans VPK kapısı kapalı tutuldu; SELF/VPK üretilmedi. Linux touchHLE menü kanıtı ayrı referanstır.
