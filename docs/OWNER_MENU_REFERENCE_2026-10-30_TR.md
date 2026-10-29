# Özgün ana menüye ulaşıldı — Linux referans çalıştırması

**Görüntü özgün Iron Man 2 1.0.0 oyununun gerçek çalıştırmasından alınmıştır. Kullanılan çalışma zamanı Linux x86_64 üzerinde touchHLE v0.3.0'dır; Vita yükleyicimiz bu menüyü henüz çalıştırmamıştır.**

## Gerçek yapılan işlem

Önceki Drive yedeği normal bağlantı üzerinden tekrar indirilebildi. `gdown` çerez önbelleği hatası `--no-cookies` ile çözüldü; ağ politikası aşılmadı. 248.613.763 bayt IPA ve içindeki 7.480.208 bayt Mach-O hash'leri bilinen özgün 1.0.0 referanslarıyla eşleşti. Dosyalar depo dışındaki özel çalışma klasöründe tutulur.

Özgün app bundle açıldı. touchHLE v0.3.0 kaynakları değiştirilmeden Linux için derlendi. Mesa llvmpipe yazılımsal OpenGL ve Xorg dummy ekranında özgün app çalıştırıldı. Önce "Touch the screen to continue" ekranı, ardından gerçek fare/dokunma girdisiyle "Main Menu" açıldı. New Game, Continue, Options ve More Games düğmeleri gözle görüldü. Menü ekranı framebuffer'dan 480×320 olarak kaydedildi; çizilerek taklit edilmedi.

Özel çalışma çıktıları:

- `/workspace/im2-current/macho-menu-reference-2026-10-30/menu.png`
- Aynı klasörde `title.png` ve `manifest.json`.

Ekran görüntüleri, özgün IPA/executable/asset ve RBS1 GitHub'a yüklenmedi. Depoda yalnız hash, sayısal sonuçlar, test/analiz kodu ve bu rapor bulunur. Uzun süreli oyun kararlılığı, ses veya gameplay doğrulanmadı. Referans denemesi süreyle sınırlandırıldı; kapanışta SDL hata diyaloğu görüldü. Menü görüntüsü kapanıştan önce alınmıştır.

## Vita projesine kazandırdığı somut kanıt

1. Özgün girdiler yeniden kullanılabilir durumda. Eksik dosya engeli bu ortamda kalktı.
2. Güncel özgün dosya ölçümü tekrarlandı: ARMv6, cryptid=0, 490 dinamik import, 5.225 external relocation, 1.696 indirect kayıt.
3. Zombie Infection'ın güncel prep aracıyla 33.267 öğeli RBS1 üretildi. Önceki 32.646 adayından farklı bir generator/sürüm kullanıldı; eşdeğer veya Vita'da doğrulanmış sayılmıyor.
4. Gerçek dosyayla preflight ve 11 bozuk girdi testi geçti. 401 yerel indirect yuva relocation içinde; 906 dış indirect yuva dışında. 5.225 external relocation ile RBS1 hedef çakışması sıfır.
5. Preflight aracı native loader'ın kontrol ettiği encryption/external encoding/bounds/duplicate/overlap kontrolleriyle genişletildi. Asset içermeyen sentetik regresyon testi CI'a eklendi.
6. Import-name audit artık startup'ta bağlanan gerçek class/metaclass data importlarını da görüyor: 490 addan 175'i mevcut binding adlarıyla eşleşiyor, 315 ad eksik. Bu yalnız ad sayımıdır; eşleşen her API'nin doğru davranış/ABI'ye sahip olduğunu kanıtlamaz.

## Değişmeyen sınır

Bu referans, özgün oyunun bir iOS HLE ortamında menüye ulaşabildiğini gösterir. Native Vita loader'ın eksik Apple API/runtime davranışlarını, UIKit başlangıcını veya gerçek Vita RX/cache gereksinimlerini çözmüş sayılmaz. Vita oyun boot'u, menüsü ve fiziksel cihaz doğrulaması hâlâ yok; VPK oluşturulmadı. Bir sonraki native çalışmada referans menü akışı API davranışlarını karşılaştırmak için kullanılabilir.
