# Model çizimi: client array ve indeksli çizim yolu

31 Ekim 2026 — bağımsız Mach-O projesi. Yeni `gles_arrays.c/.h` MIT lisanslıdır. Özgün oyun dosyaları depoya eklenmedi.

## Yapılan

Yeni `glColorPointer`, `glNormalPointer` ve `glDrawElements` importları eklendi. Vertex/texcoord pointer, enable/disable, client texture unit, DrawArrays ve GetPointer/GetIntegerv yolları aynı client-state deposuna bağlandı. Bu, yalnız üç isim eklemenin ötesinde eski çizim hazırlığının değiştirilmesidir.

- Vertex, color, normal ve iki texture-coordinate array için özgün pointer/type/size/byte stride/enable durumu korunur.
- GL_FIXED 16.16 verileri çizimden önce float'a çevrilir. Byte stride doğru uygulanır; stride/4 yuvarlaması kaldırıldı. Hizasız fixed öğeler ve u16 indeksler memcpy ile okunur.
- DrawArrays yalnız `[first,first+count)` aralığını okur. DrawElements u8/u16 indeksleri çözer, yalnız referans verilen köşeleri dönüştürür; indeksler ve sıraları değiştirilmez.
- Disabled array verisi okunmaz. Dönüşümsüz desteklenen tipler SDK'ya kendi pointer/type/stride değerleriyle aktarılır.
- GetPointerv/GetIntegerv, backend'e bağlanan geçici float pointer/type yerine özgün client durumunu döndürür.
- Realloc başarısızlığında eski allocation kaybedilmez; o istek için backend draw yapılmaz. Boyut/enum/count ve pointer-aritmetik taşmaları kontrol edilir. Yeni scratch kapasitesi sıfırlanır.
- Sunumun ezdiği color/normal/iki texcoord/vertex pointer ve enable durumu artık birlikte geri yüklenir; blit sırasında ikinci client texcoord array kapatılır. Bütün server-side grafik state'inin restorasyonu bu değişiklikle tamamlanmış sayılmaz.

## Sınırlar

Bu yol client RAM pointer'ları içindir; VBO/EBO offsetleri desteklenmez. Caller referans verilen veriler ve indeksler için geçerli bellek sağlamalıdır. API allocation boyutunu bilmez; genel bir pointer/buffer erişilebilirlik denetleyicisi değildir.

İki texture unit, en çok 65.536 vertex index alanı ve 1.048.576 indeks desteklenir. Bütçe aşımı GL_OUT_OF_MEMORY verir; u32 indeks ve desteklenmeyen tip/unit/cap reddedilir. Tek context ve dışarıdan seri client-state kullanımı varsayılır. CPU dönüşüm maliyeti ve VitaGL'nin GPU kopyalama/performance davranışı cihazda ölçülmelidir.

## Özgün çağrılar

Executable SHA256: `3ba6337a23ab7e08f2e025ec9731c53a7990e8d611b59eac07ba00cb63acc629`.

- `0x175188–0x175198`: VertexPointer size=4, type=GL_FLOAT (`0x1752bc` literal=0x1406), stride=0, pointer r3.
- `0x1751b4–0x1751c4`: ColorPointer dört argüman r0..r3.
- `0x28b118–0x28b12c`: DrawElements mode=GL_TRIANGLES, count üçgen sayısının üç katı, type=GL_UNSIGNED_SHORT (`0x28b188` literal=0x1403), indices r3.
- `0x2dbfd8–0x2dbfe8`: NormalPointer type/stride/pointer r0/r1/r2.

Bunlar statik owner incelemesidir; oyun ARM kodunun yürütüldüğü iddia edilmez.

## Doğrulama

ASan/UBSan host testi gerçek dönüşüm modülünü backend fixture ile bağlar. Fixed vertex/color/normal/iki texture seti, 10-bayt interleaved stride, hizasız u16 indeks, repeated/sparse indeksler, float/byte passthrough, durum sorguları/restoration ve enum/count/arithmetic hata yolları sınandı. mmap ile erken vertex'lerin sayfası erişilemez yapılıp yalnız referans verilen sayfa okunur bırakıldı; hem DrawArrays hem DrawElements geçer. Realloc hata enjeksiyonu eski allocation'ın korunmasını ve sonraki isteğin toparlanmasını doğrular.

Tam Vita ARM ELF'inde **95 yeni üretim client-array çağrısı** çalıştırıldı: gerçek guest import router target'ları, CPU registry/converter/native adapter kodları ve GLES error slot kullanıldı. İki yığın hizasında dönüştürülen vertex/color/normal/iki texture verisi, u8/u16 indexed draw, GetPointer/GetIntegerv, draw sayacı ve restoration kontrol edildi. Native VitaGL pointer/draw girişleri yakalanarak değerleri okundu; rasterization veya GPU kopyalama çalıştırılmadı. Mevcut test ortamının heap/Clib/OS karşılıkları kullanıldı.

Önceki 25 dispatch/6 UIKit CGRect IMP ve 32 string/56 math/948 container/18 CG/118 CF/100 GLES-state ARM çağrısı da regresyon kontrolleridir. Tam VitaSDK ELF linki güçlü undefined sembol olmadan geçer. Fiziksel Vita görüntüsü veya özgün native boot kanıtı yoktur.

## Durum

490 importtan **305 ad eşleşir, 185 ad eksiktir**. Bu ad kapsamıdır; tamamlanma yüzdesi değildir. Compressed texture, readback, float/bool/string queries, hint, tex-env çağrıları ve daha geniş sunum/server-state restorasyonu grafik tarafında hâlâ açıktır. Framework/C++/sistem/ses açıkları sürer. Yürütme/paketleme korumaları değiştirilmedi; SELF/VPK üretilmedi.
