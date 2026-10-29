# Yerel Objective-C sınıfları ve açılış doğrulaması

Bu turda yalnız bağımsız Mach-O deposu değiştirildi; VPK/SELF oluşturulmadı.

## Kodda yapılanlar

- `objc_guest.c` kaynak görüntüdeki rebased 32-bit `class_t`, `class_ro_t` ve 12 baytlık method listelerini sınırlandırılmış okuyucuyla çözümlüyor. Sınıf ve metaclass adresleri korunuyor; guest metadata üzerine yeni sınıf kopyası yazılmıyor. Registry kendi ek kayıtlarını tutuyor.
- Sınıflar superclass bağımlılık sırasına göre ekleniyor. Bilinmeyen dış parent, döngü, çakışan isim/adres, hatalı ISA/metaclass ilişkisi, instance boyutu, kod dışındaki IMP, bozuk stride/count ve sonlanmayan string reddediliyor. Başarısız grupta kayıtlar geri alınıyor; canlı nesnesi olan sınıflar kaldırılmıyor.
- `__objc_classlist` yükleyicide importlar bağlandıktan sonra bu okuyucuya gönderiliyor. Kategoriler ve C++ ivar destructor/constructor gerektiren sınıflar henüz desteklenmediğinden açıkça reddediliyor. Orijinal görüntü bu ortamda olmadığı için gerçek 25 sınıf bu turda okunmadı.
- Method araması host/guest implementasyonunu ayırıyor. Guest sınıflar için allocation özgün ISA kimliğini koruyor. `objc_msgSendSuper2` aramayı dinamik receiver'ın parent'ı yerine mevcut implementasyon sınıfının parent'ından başlatıyor; class/metaclass zincirleri ayrılıyor.
- Dört Objective-C assembly importu yeni `F_ABI` türüyle doğrudan bağlanıyor. Genel import thunk'ı artık bunların guest stack'ini peşinen kopyalamıyor. Dispatcher native IMP için hizalama köprüsüne, guest IMP için özgün register/stack frame'i koruyan doğrudan dala gidiyor. Gerçek ELF import tablosu ayrıca denetleniyor.
- Son release sırasında nesne, `dealloc` zincirinin selector/super aramasında görülebilir kalıyor; sıfır referansta retain/double release reddediliyor. Native allocation yolunda guest dealloc `call_guest` üzerinden çağrılıyor, depolama zincir döndükten sonra bırakılıyor. Bu guest dealloc yolunun özgün oyun üzerinde testi yapılmadı.
- Constructor adreslerinin tamamı immutable bir kopyada kontrol ediliyor: ARM/Thumb hizalaması ve gerçek `__text` aralığı. Sonraki constructor hatalıysa ilk constructor bile çalıştırılmıyor; bir constructor listeyi değiştirirse onaylanan snapshot kullanılıyor. Hata artık main'e taşınıyor.
- `_main`/`_SDL_main` kod bölümünde doğrulanıyor. Belirsiz imzalı `_startClet` fallback'i ve yükleyicinin sahte açılış izlenimi veren bağımsız render döngüsü kaldırıldı; guest main dönerse sonucu döndürülüyor.

## Gerçek test sonucu

- 18 C/ASM kaynak birimi + tam Vita ARM ELF bağlantısı başarılı. Güçlü tanımsız ELF sembolü yok; opsiyonel weak toolchain sembolleri cihaz kanıtı değildir.
- Host C testleri ASan/UBSan ile geçti: sınıf/metaclass, guest/native kalıtım, özgün adres kimliği, metadata hataları, atomik kayıt rollback'i, ownership/dealloc görünürlüğü, constructor sırası ve snapshot'ı, external relocation.
- Üretimdeki `asm.S` ile 23 Unicorn ARM/Thumb senaryosu geçti: normal/stret ve Super2 yolları, guest/native hedef, stack/register argümanları, hizalama, r9 ve callee-saved register'lar.
- Gerçek bağlanmış ELF'de dört dispatcher importunun `IM2_IMPORT_ABI_ENTRY=2` olduğu ve C importlarının normal köprü türünde kaldığı doğrulandı. CI'a eklendi.

Host guest-metadata testi `-no-pie` ile çalışır; böylece dış host sınıf adresleri ARM32 listesinde temsil edilebilir. Metadata alanları 32-bit kelimeler olarak okunur; testteki nesne allocation düzeni host pointer genişliğini kullanır. Bu, Vita'nın bellek veya nesne yürütme testi değildir. ARM dispatcher testindeki lookup uçları sentetiktir; orijinal oyunun method'ları yürütülmemiştir.

## Sınırlar ve sonraki engeller

Özgün Mach-O/IPA/RBS1 yerelde yok. Framework sınıflarının 62'si hâlâ eksik; iki sınıf yalnız kısmi native davranışa sahip. UIKit lifecycle, bütün özgün importlar, CFString sabitleri ve runtime `+initialize`/`+load`, category/protocol davranışları tamamlanmadı. Guest aggregate return encoding'leri `SIZE_MAX` ile desteklenmiyor olarak işaretlenir; çağrı anında tahmin ederek stret seçilmez. Yeni guest kayıt kodu tek başına tam ObjC uyumluluğu değildir.

Varsayılan guest yürütme kapısı hâlâ kapalı. Gerçek Vita RX/cache, özgün constructor ve oyun boot kanıtı yok; upstream lisans belirsizliği sürüyor. Sadece iki derleme kapısı açık, altı release kapısı kapalıdır.

## CI ve analiz aracındaki ek düzeltme

Yerel sınıf/ABI commit'i `b9d0f3c` GitHub CI'dan başarıyla geçti: https://github.com/Gh0sTRX/iron-man-2-mach-o/actions/runs/40956243220 .

Sonraki incelemede ana `scan_macho.py` aracında da gerçek NUL yerine iki karakterli `\\0` arandığı bulundu. Segment/dylib/sembol adlarının okunması düzeltildi; command envelope, alt komut boyutları, string ve dynamic undefined aralıkları doğrulanıyor. Genel nlist sayısı korunurken LC_DYSYMTAB import sayısı ayrı raporlanıyor. Sentetik testte iki generic kayıt (biri debug), bir gerçek dinamik import olarak ayrılıyor. Özgün dosyanın 490 importu bu oturumda yeniden ölçülmüş değildir.
