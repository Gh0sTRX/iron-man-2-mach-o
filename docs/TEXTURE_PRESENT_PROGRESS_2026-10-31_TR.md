# Doku durumları, sorgular ve tam sunum akışı

31 Ekim 2026 — bağımsız Mach-O projesi. Yalnız kendi uyumluluk kodu/testleri değiştirildi; özgün oyun/IPA yüklenmedi.

## Yapılan

Eksik dokuz GLES importu bağlandı: `glTexEnvf`, `glTexEnvfv`, `glTexEnvx`, `glGetFloatv`, `glGetBooleanv`, `glGetString`, `glHint`, `glReadPixels`, `glCompressedTexImage2D`.

- Float texture-env scalar çağrısı iOS base-AAPCS sınırı kullanır. Enum değerleri integer, scale parametreleri float, fixed scale/color değerleri 16.16 olarak ele alınır. Vector enum, SDK'nın yalnız color/scale kabul eden vector girişine gönderilmez; doğru scalar enum girişine aktarılır.
- NULL vector, geçersiz target/mode, geçersiz scale, NaN veya integral olmayan enum reddedilir. Color vector bileşenleri 0..1 aralığına sıkıştırılır.
- Texture-env mode artık iki server texture unit için ayrı saklanır. Server unit aralığı denetlenir; GL_MAX_TEXTURE_UNITS katmanın desteklediği iki birimi bildirir.
- Float/bool sorguları client array/FBO değerlerinde geçici native pointer/type/handle durumunu açığa çıkarmaz; virtual integer durumundan dönüştürür. Diğer scalar/vector sorgular gerçek SDK girişine yönlenir. Bool scalar çıktısı bir bayttır.
- String/hint/readback/compressed upload, gerçek VitaGL'nin pointer/integer ABI girişlerini kullanır. Bu bağlantı doku formatlarının, codec'in veya okunan GPU pikselinin doğrulandığı anlamına gelmez.

## Sunum düzeltmesi

`gles_present` artık guest'in iki doku birimini ayrı korur. Sunum sırasında ikinci texture/client coordinate unit, fog, lighting, culling, stencil/depth/alpha/scissor/blend testleri kapatılır. Color write mask açılır; siyah clear color ve identity texture matrix kullanılır. Modelview/projection/texture matrix stack'leri, mode, viewport, framebuffer, unit0 texture binding/env mode, iki texture enable durumu, mask/clear/current color, client arrays ve active server/client unit geri yüklenir.

Böylece guest'in ışık/sis veya doku matrisi sunum dörtgenini karartıp/bükmez; guest'in yalnız bazı renk kanallarını yazdıran maskesi ekran sunumunu bozmaz. Bu akışın fiziksel GPU sonucu ve VitaGL swap yan etkileri ayrıca doğrulanmalıdır. Desteklenen tek context/iki unit yolu için CPU state restorasyonu test edildi; genel GLES conformance veya her extension desteği iddia edilmez.

## Özgün çağrılar

Executable SHA256 `3ba6337a23ab7e08f2e025ec9731c53a7990e8d611b59eac07ba00cb63acc629`.

- `0x28e4b8–0x28e4c8`: ReadPixels format/type/data son üç argümanı stack'te; stub `0x470c90`.
- `0x29a3ac–0x29a3c4`: CompressedTexImage2D height/border/imageSize/data stack'te; stub `0x470a8c`.
- `0x2dc2b0–0x2dc2c0`: TexEnvfv target r0, pname r1, float vector pointer r2.
- `0x2dc958–0x2dc968`: TexEnvf float bitleri r2'de; Vita hard-float'a doğrudan bağlanamaz.
- `0x3e30a8–0x3e30b4`: TexEnvx GL_TEXTURE_ENV_MODE=0x2200 için GL_MODULATE=0x2100 alır; enum fixed olarak ölçeklenmemelidir.

Bu statik owner analizi, özgün kodun çalıştırılması değildir.

## Doğrulama ve sınır

Tam Vita ARM ELF'inde **127 yeni üretim çağrısı** çalıştırıldı. Gerçek init, OES registry/attachment/status, texture/query köprüleri, EAGL present ve `gles_present` kullanıldı. Model backend, kaynak üretimi ve aktarım parametrelerini yakalar; gerçek SDK/GPU, console/OS ve önceki fixture allocator/lock karşılıkları kullanılır.

İki giriş yığını hizasında enum/float/fixed/vector davranışı ve hatalar, byte-sized bool/cache queries, 7/8 argümanlı readback/upload aktarımı ve bozulmayan buffer guard'ları kontrol edildi. Readback pikselleri sentetik test verisidir; compressed veri yalnız transport açısından sınandı.

Gerçek OES başlatma yolu üzerinden offscreen color target oluşturuldu. İki texture, fog/lighting/culling/stencil, özel mask/clear color ve dört matris içeren guest durumu, üretim sunumundan sonra tam eşitlik ile karşılaştırıldı. Sunum sırasında texture1 kapalı, mask açık, fog/light/testler kapalı, texture0 matrisi identity ve dörtgen/UV değerleri beklenen 960×544 yerleşiminde çıktı. Bu **CPU komut/state testi** olup rasterization, screenshot, fiziksel Vita veya özgün oyun yürütmesi kanıtı değildir.

Önceki ARM/host ASan/UBSan testleri ve tam VitaSDK ELF bağlantısı geçti. Native RX/yürütme ve VPK korumaları değiştirilmedi.

## Kalan

490 özgün importtan **314 ad eşleşir, 176 ad eksiktir**. Özgün `gl*` importları arasında artık eksik ad yoktur. Bunlar isim kapsamıdır; GLES'in bütün davranışları veya görüntü çalışıyor demek değildir. Doku format/decode, backend query/string capability uyumu, GPU/copy/swap/performance ve cihaz görüntüsü açık kalır. Framework/C++/sistem/ses bağımlılıkları ve özgün açılış çözülmeye devam edilmelidir.
