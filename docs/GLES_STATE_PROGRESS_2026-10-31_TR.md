# VitaGL grafik komutları: iOS ARM çağrı uyumluluğu

31 Ekim 2026. `gles_float.c` bağımsız MIT lisanslı köprü kodudur; VitaGL algoritması kopyalanmadı. Mevcut SDK'nın gerçek grafik giriş noktaları kullanılır.

## Yapılan

19 özgün import bağlandı:

- Renk/derinlik/alpha: `glClearColor`, `glClearColorx`, `glClearDepthf`, `glDepthRangef`, `glAlphaFunc`.
- Sis: `glFogf`, `glFogfv`.
- Matris/dönüşüm: `glFrustumf`, `glOrthof`, `glRotatef`, `glScalef`, `glTranslatef`, `glLoadMatrixf`, `glMultMatrixf`.
- Malzeme/ışık: `glMaterialf`, `glMaterialfv`, `glLightf`, `glLightfv`, `glLightModelfv`.

Ondalıklı sayı alan işlevler açık `pcs("aapcs")` köprüleriyle iOS genel yazmaç/yığın argümanlarını Vita hard-float argümanlarına dönüştürür. Pointer/integer-only işlevler SDK girişlerine doğrudan bağlanır; float dizileri değiştirilmez.

SDK'da scalar `glLightf` girişi yoktur. Özgün oyunun kullandığı attenuation parametreleri tek float depolanarak `glLightfv` yoluna aktarılır. Scalar çağrıda dört değer isteyen pname'ler reddedilir: tek float adresinden dört float okunmasına izin verilmez. `GL_LIGHT0..GL_LIGHT7` aralığı, NULL vector ve negatif attenuation kontrol edilir. SDK'nın desteklemediği spot parametreleri hata olarak kalır; spot ışığı desteği icat edilmedi. Geçerli ambient/diffuse/specular/position vector'ları SDK'ya gider.

Uyumluluk hataları mevcut GLES ilk-hata yuvasını paylaşır; `glGetError` hatayı alıp temizler. Eski, yalnız sıfır yazan `glGetLightxv` işlevi ve import kaydı kaldırıldı. Özgün 490 import arasında bu işlev yoktur; yeni bir ışık sorgulama desteği iddia edilmez.

## Özgün çağrılarla karşılaştırma

Executable SHA256: `3ba6337a23ab7e08f2e025ec9731c53a7990e8d611b59eac07ba00cb63acc629`.

- Orthof `0x16a4d8–0x16a4f8`, frustum `0x16a564–0x16a584`: ilk dört float r0..r3; son iki float `[sp]` ve `[sp+4]`.
- ClearColor `0x182dfc–0x182e0c`: dört float sözcüğü r0..r3.
- Fogf `0x2dbb68–0x2dbb74`: enum r0, float bitleri r1.
- Lightf `0x2dc538–0x2dc548`: light r0, pname r1, float bitleri r2. `0x2dc6b4` literal'i `0x1209` (`GL_QUADRATIC_ATTENUATION`).
- Materialf `0x2dc874–0x2dc884`: face r0, pname r1, float bitleri r2; ilgili literal `0x1601` (`GL_SHININESS`).

Bu salt okunur statik owner incelemesidir; özgün oyun kodu çalıştırılmadı ve dosyası yüklenmedi.

## Doğrulama ve sınır

Tam bağlı Vita ARM ELF'inde **100 üretim GLES state/error çağrısı** Unicorn ile sınandı. 4 ve 8 bayt hizalı başlangıç yığını, r9/yığın korunması, VFP girişleri NaN ile zehirlenmişken gerçek scalar köprüler, altı argümanlı çağrıların yığın aktarımı, scalar/vector ışık dönüşümü, matris pointer'ları, negatif fixed renk sözcükleri ve ilk-hata davranışı doğrulandı. Geçersiz ışık, NULL vector, negatif attenuation ve yanlış scalar/vector pname yollarının backend'e ulaşmadığı kontrol edildi.

Test gerçek köprü kodunu çalıştırır, **VitaGL girişlerinde çağrıları yakalar**. VitaGL shader/GPU çalıştırması, görüntü veya ışıklandırmanın görsel doğruluğu bu testin kanıtı değildir. Native GPU durumunun beklenen resme dönüştüğü, context/presentation ve diğer state geri yüklemeleri cihazda sınanmalı. Eski 32 string, 56 math, 948 container, 18 CoreGraphics ve 118 CF/runtime ARM çağrısı da geçti.

## Kalan

490 importtan 302 ad eşleşir; **188 ad eksik**. Bu ad kapsamıdır, tamamlanma yüzdesi değildir. GLES'te hâlâ color/normal array pointer, indexed draw, compressed texture, readback, state/string queries, hint ve tex-env float/vector/fixed çağrıları eksiktir. Client-array fixed dönüşümü ve blit/state restoration daha fazla çalışma ister. Framework/C++/sistem/ses boşlukları da sürer.

Özgün native açılış ve fiziksel Vita doğrulanmadı. Yürütme/paketleme korumaları değiştirilmedi; SELF veya VPK yok.

## GitHub CI

Kaynak commit `3dd18636160af234348dee98cb218558545c916e`, [Actions 41230008835](https://github.com/Gh0sTRX/iron-man-2-mach-o/actions/runs/41230008835): hesap ödeme/harcama limiti uyarısı nedeniyle job başlamadı; hiçbir test adımı çalışmadı. Yeni kaynak için CI başarısı iddia edilmez. Bu rapordaki doğrulamalar yerel managed ortam sonuçlarıdır; hesap ayarları değiştirilmedi ve engelli job yeniden başlatılmadı.
