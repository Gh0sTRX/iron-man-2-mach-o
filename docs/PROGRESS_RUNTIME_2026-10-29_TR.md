# ARM ELF ve Objective-C çekirdeği

Bu çalışma bağımsız `iron-man-2-mach-o` deposundadır. Diğer port deposu değiştirilmedi. Önceki kesintiden kalan bağımsız prototip, güncel uzak dalı almadan önce Git stash içinde saklandı; güncel vendored yükleyici üzerine yazılmadı.

## Gerçek sonuçlar

- Son CI (`40951213667`, `de45e2a`) ARM derlemesine ulaşmadan sınıf sembolünün sonunu ararken başarısız oluyordu. Denetleyicide `b"\\0"` yerine gerçek NUL `b"\0"` aranıyor; mevcut sentetik pozitif/negatif testler geçti.
- Tanımsız `l_debug` çağrıları ve `ime_active` bildirimi düzeltildi; import makrolarında fonksiyon/data ayrımı korundu.
- 15 C/ASM birimi VitaSDK GCC 15.2.0 ile derlendi. Yeni tam ELF hedefinde vitaGL'nin ihtiyaç duyduğu C++ çalışma zamanı bağlandı. ARM ELF32 EABI5 bağlantısı başarılı; güçlü tanımsız sembol yok, 15 isteğe bağlı zayıf toolchain sembolü var.
- Yeni bağımsız `objc_runtime.c/h`: class/metaclass, class_ro/class_rw, superclass ilişkileri; selector interning; sınıfa göre instance/class method araması; retain/release; thread'e özel iç içe autorelease pool.
- NSObject kökü, kısmi NSString ve NSAutoreleasePool yükleyicinin data importlarına bağlandı. NSString'in 256 öğeli tekrar kullanılan halkası kaldırıldı. Bilinmeyen sınıflar için placeholder oluşturulmuyor. 64 dış sınıfın ikisi kısmen temsil ediliyor, 62'si hâlâ eksik: `objc_class_support.json`.
- ARM `objc_msgSend` girişleri argümanları koruyarak sınıfa özgü IMP seçiyor; normal/stret çağrı türleri eşleşmezse duruyor. Kayıtlı olmayan yerel oyun nesneleri henüz desteklenmiyor; eski genel selector dispatcher'a düşürülmüyor.
- Orijinal bundle yolu `ux0:data/ironman2_macho/IronMan2.app/IronMan2`; `_main` için argc/argv sağlandı. Şifreli Mach-O artık yalnız log yazıp devam etmiyor, reddediliyor.

## Çalıştırılan testler

`bash tools/test_source.sh`: mevcut yapısal ve sentetik parser kontrolleri, yeni gerçek C runtime davranış testleri ASan/UBSan ile geçti. Metaclass ilişkisi, kalıtım, class/instance ayrımı, bilinmeyen nesne/selector, 400 nesnenin ömrü, iç içe ve farklı thread pool'ları denendi.

`tools/build_arm_dispatch_test.sh` ve Unicorn 2.1.4: üretimdeki aynı `asm.S` makine koduyla 11 senaryo geçti. Normal/stret dispatcher argümanları, stack argümanları, 4/8 bayt guest stack hizalaması, ARM/Thumb geçişi, 4 ve 6 argümanlı callback, `r9` ve callee-saved register korunması. Bu testte lookup/IMP uçları sentetiktir; özgün oyunun method'ları veya gerçek Vita çalıştırılmadı.

`tools/build_loader_elf.sh`: bütün kaynaklar + ELF bağlantısı başarılı. SDK digest ve kaynak/ELF hash'leri `BUILD_EVIDENCE_2026-10-29.json` içinde. Varsayılan host ELF hard-float; konuk softfp ve variadic/float/struct ABI kapsamı tüm importlar için henüz doğrulanmış değildir.

## Açık engeller

Bu ortamda IPA, 7.480.208 baytlık özgün çalıştırılabilir ve RBS1 bulunamadı. Önceki 32.646 relocation / 243 classref ölçümleri bu oturumda yeniden ölçülmedi. Owner-image test betiği özel `--ipa --rbs1` girdileri ister; genel `unittest discover` bu CLI betiklerini çalıştırmak için uygun değildir.

Yükleyici henüz class/metaclass dışındaki bütün external relocation kayıtlarını uygulamıyor; bu bağımsız bir sonraki kod engelidir. Yerel oyun sınıflarının okunması, superclass/super-dispatch, CFString sabitleri, kalan iOS importları, UIKit lifecycle, grafik/ses/girdi ve özgün açılış çözülmedi. NSString'in mevcut format/encoding davranışları da tam Foundation uyumluluğu değildir.

Apple `objc4-437/runtime/objc-runtime-new.h` ABI araştırması için okundu: https://github.com/apple-oss-distributions/objc4/blob/objc4-437/runtime/objc-runtime-new.h . Apple uygulama kodu kopyalanmadı. Yeni çekirdek MIT olarak işaretlendi; eski vendored MetalSyntax kaynağının lisansı hâlâ belirsiz, projenin özel depo durumu korundu.

İki derleme kapısı kanıtla açıldı; altı import/runtime/cihaz/lisans kapısı kapalı. Cihaz RX/cache doğrulaması olmadan guest execution varsayılan kapalı. SELF/VPK oluşturulmadı. Probe workflow da yalnız ELF üretecek şekilde değiştirildi.

## İkinci düzeltme: bütün external relocation tablosu

Sınıf/metaclass dışındaki external relocation kayıtlarının atlanması düzeltildi. Bağımsız `macho_reloc.c` artık mutlak, external ARM_RELOC_VANILLA32 kayıtlarını planlıyor; C++ vtable+8 ve negatif addend değerlerini koruyor. Bütün kayıt adresleri, sembol indeksleri, türleri ve yinelenen hedefler önce doğrulanıyor; eksik import varsa hiçbir relocation slotu kısmen yazılmıyor. Hem __TEXT staging hem __DATA hedefleri destekleniyor. Bilinmeyen PC-relative/paired/scattered relocation biçimleri sessizce atlanmıyor, reddediliyor. RBS1/external hedef çakışması, sıralı RBS1 üzerinde ikili aramayla yazmadan önce reddediliyor. Orijinal dosya mevcut olmadığından gerçek 5.225 kaydın tamamına uygulama bu oturumda doğrulanmadı.

Yeni C relocation testleri ASan/UBSan ile geçti; mevcut kontroller ve tam ARM ELF bağlantısı tekrar geçti. Kaynak birimi sayısı artık 16. İlk runtime commit'inin kaynak CI'ı `40952934103` başarılıdır. Ayrı eski probe workflow'u ELF'yi başarıyla derledi fakat GitHub artifact kotası dolu olduğundan upload adımı başarısızdı (`40952934028`). Otomatik artifact yüklemesi kaldırıldı; ELF hash'i loga yazılıyor, hiçbir eski artifact silinmedi. SELF/VPK hâlâ üretilmiyor.
