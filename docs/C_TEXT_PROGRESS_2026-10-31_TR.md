# C metinleri ve unsigned64 sayı dönüşümü

31 Ekim 2026. Yeni `libc_text.c` MIT lisanslı uyumluluk tablosudur; metin algoritmaları yeniden yazılmadı veya kopyalanmadı. Mevcut VitaSDK C kütüphanesi kullanılır.

## Kapsam

10 yeni import: `atol`, `strcasecmp`, `strncasecmp`, `strlcpy`, `strcoll`, `wcscmp`, `wcscpy`, `wcslen`, `wcstombs`, `__floatundidf` (Mach-O adı `___floatundidf`).

SDK derlemesinde long, size_t ve wchar_t'nin 32 bit olduğu static_assert ile şart koşulur. C wide karakterler 32 bittir; Foundation unichar/UTF-16 ile karıştırılmaz. Pointer/integer-only metin işlevleri doğrudan gerçek SDK girişlerine bağlanır. Unsigned64→double yardımcı işlevi, unsigned argüman ve double sonuç için açık iOS base-AAPCS sınırı kullanır; VFP sonuç yazmacına yanlış bağlanmaz.

## Test

Tam Vita ARM ELF'inde **56 yeni gerçek SDK/helper çağrısı** çalıştırıldı. İki yığın hizasında stack/r9 korunması, case/count karşılaştırmaları, 0/1/5/11/16 kapasiteli strlcpy ve taşmayan buffer guard'ları, 32-bit Unicode/emoji wide copy/length, ASCII wcstombs kapasite/sonlandırma kuralları ve NULL-output uzunluk sorgusu kontrol edildi.

`atol` için işaretli 32-bit sınırlar, `__floatundidf` için 0/1, 2^32+1, 2^53+1, 2^63 ve UINT64_MAX doğru double yuvarlamasıyla ve zehirlenmiş VFP sonuç yazmacıyla sınandı. Bunlar gerçek SDK kodu ve kendi çağrı köprüsüdür; önceki fixture heap/Clib/OS karşılıkları geçerlidir. Grafik modeli bu metin işlevlerinin yerine geçmez.

Tam VitaSDK ELF bağlantısı ve host ASan/UBSan regresyonları geçer. Özgün oyun veya fiziksel Vita yürütmesi değildir.

## Sınırlar ve durum

Native C-library locale davranışı kullanılır. C-locale ASCII dönüşümü sınandı; başka locale'ler, locale emülasyonu ve Darwin errno numarası uyarlaması bu modülle çözülmedi. Caller geçerli sonlandırılmış metin/buffer sağlamalıdır. `atol` taşma davranışı için yeni bir sözleşme icat edilmedi; yalnız representable signed32 değerleri doğrulandı.

Bu turda grafik/doku/sunum grubundaki 9 bağlantının ardından 10 temel C bağlantısı daha eklendi. Güncel 490 importun 324'ü ad olarak eşleşir, **166'sı eksiktir**. Tamamlanma yüzdesi değildir. Framework/C++/dosya/thread/ses ve gerçek native boot/device doğrulaması devam eder. SELF/VPK oluşturulmadı; yürütme ve release korumaları korunur.

## GitHub CI

Kaynak `b8d28e9d87d4213d5ddc79a2661c03c94da6c310`, [Actions 41234125101](https://github.com/Gh0sTRX/iron-man-2-mach-o/actions/runs/41234125101): hesap ödeme/harcama limiti uyarısıyla iş hiç başlamadı; test adımı yoktur. CI başarısı iddia edilmez. Yukarıdaki sonuçlar yerel managed ortamdan alınmıştır. Hesap ayarları değiştirilmedi ve engelli iş yeniden başlatılmadı. Bu kaynak, önceki doku/sunum düzeltmesini de içerir.
