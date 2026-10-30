# Dünkü çalışmadan devam — 31 Ekim 2026

GitHub dalı `experiment/ios-macho-loader`, yerel `454b33b` kaydından **134 commit** ilerideydi. Yerel değişiklik yokken `4482eae` üzerine fast-forward yapıldı. Yeni URL/dosya, UIKit hierarchy/application/bootstrap/UIScreen ve EAGL katmanları korundu. Son üç çalışma relocation signed-addend overflow koruması, owner class-import kapsam raporu ve literal/portable symbol matcher idi. `4482eae` CI sonucu başarılı: https://github.com/Gh0sTRX/iron-man-2-mach-o/actions/runs/41125865875 . Güncellenmiş baseline host testleri ayrıca yerelde geçti.

## Bulunan sonraki somut engel

`gles.c` eski OES framebuffer çağrılarında tek bir özel FBO'yu her kimlik için bağlıyordu. Renderbuffer bağlama, silme, storage ve attachment çağrıları `g_glNoop`'a gidiyordu. Bu, bir önceki turda eklenen gerçek EAGLContext sınıfının özgün oyunun surface kurulumunu doğru temsil etmesini engelliyordu.

Özgün executable'ın hash'i `3ba6337a23ab7e08f2e025ec9731c53a7990e8d611b59eac07ba00cb63acc629`. `-[EAGLView _createSurface]` adresi `0x528c4`. ARM BL hedefleri Mach-O indirect import stub tablosuyla eşleştirildi:

| Çağrı VM adresi | Import |
|---|---|
| 0x52978 | glGenRenderbuffersOES |
| 0x52988 | glBindRenderbufferOES |
| 0x529c0 | glDeleteRenderbuffersOES |
| 0x52a04 | glGenFramebuffersOES |
| 0x52a14 | glBindFramebufferOES |
| 0x52a2c | glFramebufferRenderbufferOES |
| 0x52a8c | glGetError |
| 0x52b80 | glRenderbufferStorageOES |
| 0x52b98 | glFramebufferRenderbufferOES |

Bu statik kontrol çağrıların kodda bulunduğunu gösterir; dalların çalışma sırası veya native guest çalışması kanıtı değildir. Orijinal kod/metin/asset depoya eklenmedi.

## Bu devam oturumundaki kod

- Bağımsız MIT `gles_oes.c/h`: ayrı framebuffer/renderbuffer ad alanları, her biri 64 kayıt sınırı; gerçek backend handle eşlemesi, binding saklama, oluşturma/silme, boyut ve storage bilgisi, color/depth attachment ve drawable seçimi.
- Başarısız FBO grubu ayırması önceki kaynakları geri bırakır, çıktı adlarını değiştirmez. Gerçek GPU framebuffer yoksa başarı üretmez. Yanlış hedef/ad/format/ölçü GL hatası verir; backend hatası sonrası sahte complete dönmez.
- Renk GPU texture'ı RGBA8 veya RGB565; derinlik VitaGL DEPTH_COMPONENT16 renderbuffer yoluna gider. Depth belleği VitaGL'nin lazy framebuffer/depth mekanizmasına dayanır; cihazda doğrulanmadı. Color/depth boyutları uyuşmuyorsa completeness reddedilir.
- EAGL drawable color-format sözlüğü okunur: RGB565 gerçekten RGB + UNSIGNED_SHORT_5_6_5 texture ayırır; varsayılan RGBA8'dir. Bilinmeyen/bozuk renk adı reddedilir. Retained backing özelliği (açık false değeri dahil) NSNumber/preservation desteği gelene kadar reddedilir; tüm layer özellikleri tamamlanmış değildir.
- EAGL storage aşamasının FBO yaratılmasından önce gelebilmesine izin verilir; sunumda ayrıca bağlı color renderbuffer'a ait gerçek, complete FBO aranır. Sunum öncesi tekrar storage ayırma kaldırıldı. Blit sonrası guest framebuffer binding'i geri yüklenir.
- `glGetIntegerv` OES binding sorguları guest adlarını döndürür. `glGetError` köprü hatalarını bildirir. Gerçek ARM ELF import tablosu yeni işlevlere yöneltilir; eski `g_glNoop` kaldırıldı.

## Doğrulama ve sınırlar

`tools/test_source.sh` ASan/UBSan ile eski ve yeni testleri geçti. Yeni CPU/backend fixture testi resource kimliklerini, farklı FBO binding'lerini, RGBA8/RGB565/depth storage'ı, boyut uyuşmazlığını, eksik attachment'ı, başarısız allocation/attachment'ı, silmeyi ve sunum seçimini sınar. EAGL testi RGB565/default RGBA8 ile başarısız storage sonrası sunum engelini doğrular. Mock GPU callbacks yalnız host testi içindir.

VitaSDK ile **30 C/ASM birimi** tam ARM ELF'e bağlandı; güçlü undefined sembol yok. Linked ELF içindeki OES importlarının gerçek stateful wrapper adreslerine gittiği doğrulandı. Mevcut üretim assembly testi 23 ARM/Thumb senaryosunda geçti. Bunlar özgün EAGLView'in ARM kodunun native çalıştığı veya GPU çıktısının doğru olduğu anlamına gelmez.

Owner class-import raporu 64 gerekli sınıf sembolünden 21'ini bildirimlerde görüyor (218 slot); genel import ad sayımı 208/490 eşleşme, 282 eksik. Bu sayılar ABI/davranış doğrulaması veya tamamlanma yüzdesi değildir.

Yeni OES subset yalnız üretilmiş adlara bind eder; implicit name creation, stencil, texture attachment API'si, çoklu EAGL GPU context izolasyonu ve bağlı storage'ın yeniden boyutlandırılması yok. Reallocation açıkça hata verir; silinmiş renderbuffer tüm sahip olunan FBO'lardan ayrılır. GL nesnelerinin tüm referans/lifetime kuralları henüz taklit edilmez. Yalnız render thread üzerinde seri kullanım hedeflenir. Sunum rotasyonu, viewport varsayımı 320×480 ve görüntü doğruluğu fiziksel Vita üzerinde test edilmemiştir.

Native Vita menüsü hâlâ yok. UIApplicationMain event/render loop, kalan importlar, native guest çağrıları, RX/cache ve cihaz grafik/ses/girdi doğrulaması açık. Altı VPK kapısı kapalı tutuldu; SELF/VPK üretilmedi. Ayrı Iron-man-2-os deposu değiştirilmedi.
