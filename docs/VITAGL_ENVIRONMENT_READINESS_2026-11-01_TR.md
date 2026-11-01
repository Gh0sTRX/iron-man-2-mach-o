# VitaGL ortam doğrulaması — 1 Kasım 2026

Bu sayfa `b7a0d41` aşamasının tarihsel kaydıdır. Sonraki ayrı SELF/VPK hazırlığı, VM deneyi ve güncel cihaz durumu için [DEVICE_TEST_STATUS.md](DEVICE_TEST_STATUS.md) dosyasına bakın. Fiziksel doğrulama hâlâ yapılmadı.

Gerçek Vita/GPU hazırlığı **henüz doğrulanmadı**. Bulut ortamında erişilebilir fiziksel Vita bağlantısı veya cihazdan alınmış tanılama sonucu bulunmuyor. Bu nedenle özgün oyunun `gles_eagl_ready` sınırı aşılmadı; sahte başarı döndürülmedi.

## Doğrulanan küçük adım

`b7a0d411b1fb00c702da99f4d52c3ad750b55722` commit'i, oyun dosyası gerektirmeyen ayrı bir **yerel Vita tanılama ELF'i** ekliyor. Normal yükleyicide bu seçenek kapalıdır. Aynı üretim `gles_init`, OES köprüsü ve SDK VitaGL kütüphanesiyle derlenir; özgün oyunu yüklemez.

Üretimdeki `gles_eagl_ready`, VitaGL başlatma sonucunu ve OES kayıt nesnesinin varlığını kontrol ediyor. Tek başına gerçek çizim/piksel doğrulaması sağlamıyor. Bu fonksiyon ve özgün worker'ın emülasyon içindeki koşulsuz durdurma bariyeri değiştirilmedi.

Tanılama cihazda çalıştırıldığında sırasıyla:

1. Başlangıç durumunu ve shader derleyici dosyasının varlığını kaydeder. Dosya varlığı, derleyicinin başarıyla yüklendiği anlamına gelmez.
2. Gerçek üretim `gles_init` yolunu çağırır; GL kimliklerini ve doku sınırını sorgular.
3. Üretim OES yolu üzerinden 320×480 RGB565 renk ve depth16 tamponu kurar; framebuffer bütünlüğünü ve yerel bağlanmayı kontrol eder.
4. 16×16 alanda yeşil temizleme rengi, ardından kırmızı bir test üçgeni üretir. İki aşamada da `glFinish` ve `glReadPixels` ile farklı renkleri doğrular; GL hatalarını kontrol eder.
5. Oluşturduğu tamponları siler ve temizleme hatalarını da denetler.

Bu kod **cihazda çalıştırılmadı**. Başarılı GPU sonucu üreten bir taklit test yapılmadı. Yapısal kapsam testi yalnız tanılamanın özgün oyuna geçmemesini, gerçek API çağrılarını içermesini ve bariyerin korunmasını denetler.

## Derleme ve test kanıtı

- Sabitlenmiş VitaSDK Docker imajında normal yükleyici ve tanılama ELF'i bağlandı; çözümlenmemiş güçlü sembol yok.
- Tanılama girişinin makine kodunda `gles_init`, `gles_eagl_ready`, `glDrawArrays` ve `glReadPixels` çağrıları kontrol edildi.
- ASan/UBSan kaynak testleri ve tanılama kapsam kontrolü geçti.
- [GitHub Actions 41308891545](https://github.com/Gh0sTRX/iron-man-2-mach-o/actions/runs/41308891545), aynı kaynak commit'inde başarılı. CI sonucu derleme ve dosyasız test kanıtıdır; fiziksel GPU testi değildir.
- Normal yükleyici ELF'inin SHA-256 değeri önceki dosya sistemi/grafik sınırı kanıtıyla aynı kaldı. Tanılama işareti yalnız ayrı tanılama ELF'inde bulunuyor.
- SELF, VPK ve `eboot.bin` üretilmedi. Özgün IPA veya oyun varlıkları yayımlanmadı.

Derleme komutu: `bash tools/build_vitagl_environment_probe.sh` (VitaSDK ortamında).
Çıktı: `build/vitagl-environment-probe/im2_vitagl_environment_probe`.
Bu ELF, VitaShell üzerinden doğrudan kurulabilir bir paket değildir. Bu adımda paketleme yapılmadı; cihazda başlatma yolu ve cihaz erişimi ayrıca sağlanmalıdır.

## Sonraki kanıt ve güvenli ilerleme koşulu

Cihazda çalıştırılınca aşama ve sonuçlar `ux0:data/ironman2_macho/vitagl_environment_probe.json` dosyasına yazılacak. İlk beklenen kanıt; gerçek cihaz kaynağı doğrulanmış bu rapor, GL kimlikleri, framebuffer durumu, iki farklı piksel sonucu, GL hata kontrolleri ve temizleme sonucudur. Başlatma takılırsa son yazılmış aşama da araştırma için kullanılabilir.

Raporun `physical_hardware_confirmed:false` ve `execution_origin:unattested_native_runtime` alanları bilinçlidir: program kendi başına fiziksel cihazda mı emülatörde mi çalıştığını bağımsız biçimde kanıtlayamaz. Bu alanı elle değiştirmek hiçbir bariyeri açmaz.

Gerçek cihaz sonucu gelmeden `gles_eagl_ready` için başarı varsayılmayacak. Sonuç doğrulanırsa sonraki ayrı adım, özgün EAGL çağrısını aynı yerel bağlamda sınırlı olarak denemektir. İş parçacığı/GL bağlam sahipliği, özgün import ve metadata eksikleri, çalıştırılabilir bellek ve önbellek koşulları hâlâ ayrıca doğrulanmalıdır. Menü, oynanış veya tam port başarısı bildirilmemiştir.

Makine tarafından okunabilir derleme/kapsam kanıtı: [VITAGL_ENVIRONMENT_READINESS_2026-11-01.json](VITAGL_ENVIRONMENT_READINESS_2026-11-01.json).
