# Eski C++ sayıdan metin üretme/iostream yolu

31 Ekim 2026. Yeni `legacy_streams.cpp` MIT lisanslı köprü kodudur. Iostream algoritmaları yeniden yazılmadı/kopyalanmadı; SDK'nın eski `_GLIBCXX_USE_CXX11_ABI=0` uygulaması kullanılır.

## Sekiz yeni import

ostringstream constructor/destructor, ostream int ve long ekleme, stringbuf→string, basic_ios widen, ios_base::Init constructor/destructor. Native exception'ın iOS SjLj frame'lerine kaçmaması için köprüler catch/fatal sınırı kullanır. Bu iOS C++ exception veya genel RTTI desteği değildir.

SDK ARM layout probu: ostringstream 180, stringbuf 40, ostream 140, basic_ios 136, ios_base 112, string 4 ve Init 1 bayt; signed long 32 bit, `out` openmode=0x10. ARM derlemesi bu şartları static_assert ile zorunlu kılar. Pointer/int-only girişlerde base/hard PCS fiziksel register düzeni aynıdır. String sonucu açık hidden-output adresinde placement-new ile oluşturulur.

## Owner ile ölçülen eşleşme

Executable SHA256: `3ba6337a23ab7e08f2e025ec9731c53a7990e8d611b59eac07ba00cb63acc629`.

- `0x416efc–0x416f08`: stream storage sp+0x40, mode=0x10; SDK constructor için 180-bayt alan bir sonraki local sp+0xf4'e kadar sığar.
- `0x416f14–0x416f28`: int insertion aynı complete-object adresini kullanır.
- `0x416f50–0x416f58`: stringbuf complete object+4; hidden string output sp+0xf4.
- `0x41d1f8–0x41d234`: virtual basic_ios offset'i vtable-12'den okunur; width basic_ios+8, fill_init +0x75 ve widen ASCII space çağrısı kullanılır.
- `0x452e14–0x452e34`: aynı out constructor ve signed long insertion yolu.
- `0xb6bc–0xb6c0`: ios_base::Init constructor başlangıçta çağrılır.

Bu statik owner incelemesidir. Nesnenin bütün eski uygulama/exception ayrıntılarının uyumunu ispatlamaz; ölçülen alanlar ve test yollarıyla sınırlıdır.

## Test

Host ASan/UBSan, gerçek import tablosundan çağrılarla Init yaşam döngüsü, 32-bit sınır değerlerinde int/long ondalık biçimlendirme, widen, stringbuf hidden result ve nesne çevresi canary'leri kontrol eder.

Tam Vita ARM ELF'inde **112 yeni üretim stream çağrısı** çalıştırıldı. Gerçek SDK C++ constructor, locale initialization, numeric formatting, stringbuf ve destructor kodları kullanıldı. İki giriş yığını hizasında 180-bayt alan canary'leri, stringbuf+4, native vtable'dan +44 virtual-basic_ios offset'i, sınır int/long değerleri, COW string ömrü ve Init bir-bayt alanının komşuları doğrulandı.

Original inline düzeni ayrıca replay edildi: basic_ios+8'e width=8, +0x74'e fill='0', +0x75'e fill_init=1 yazıldığında `123` tam `00000123` olur; insertion sonrası width sıfırlanır. Flags +12 alanında hex seçildiğinde signed32 -1 tam `ffffffff` olur; host long=64 bit varsayımı gizlice uygulanmaz. Her stream/string yok edildikten sonra heap, ilk locale warmup'ından sonraki seviyeye döner.

Fixture pthread_once için yalnız başarı döndürmez: gerçek SDK initializer callback'i ARM dönüş trampoline'i üzerinden bir kez çalıştırılır. OS mutex/attribute işlemleri tek-thread karşılıklarıdır; önceki heap/Clib/reent ve logger/GPU fixture sınırları geçerlidir. Native OS scheduling, fiziksel Vita veya orijinal oyun kodu yürütmesi kanıtı değildir.

## Durum

490 özgün importtan **332 ad eşleşir, 158 ad eksiktir**. Bu isim kapsamıdır, tamamlanma yüzdesi değildir. Özgün constructor/main/AppController zinciri henüz native Vita loader'da çalıştırılmadı. Genel exception/SjLj/RTTI, diğer C/POSIX, framework, dosya/thread ve ses boşlukları sürer. SELF/VPK oluşturulmadı; yürütme/release korumaları korunur.

## GitHub CI

Kaynak `87d26a99500b66fe68a15a17c171725279067661`, [Actions 41235453412](https://github.com/Gh0sTRX/iron-man-2-mach-o/actions/runs/41235453412): hesap ödeme/harcama limiti uyarısıyla job başlamadı, test adımı çalışmadı. Yerel doğrulamalar geçmiştir; yeni kaynak için CI başarısı iddia edilmez. Hesap ayarları değiştirilmedi ve engelli job yeniden başlatılmadı.
