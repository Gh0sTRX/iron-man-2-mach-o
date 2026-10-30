# Iron Man 2 iOS Mach-O — UIScreen / UIView açılış geliştirmeleri

**Tarih:** 30 Ekim 2026  
**Dal:** `experiment/ios-macho-loader`  
**Kapsam:** Orijinal oyunun ARMv6 Mach-O yükleyicisi; ayrı VitaGL yeniden yapım projesi değildir.

## Orijinal IPA üzerinden doğrulanan kanıt

Sahibi tarafından sağlanan `IronMan2_1.0.0_ios_2.2.1.ipa` dosyasının özgün ARM32 Mach-O'su analiz edildi.

- Yürütülebilir SHA-256: `3ba6337a23ab7e08f2e025ec9731c53a7990e8d611b59eac07ba00cb63acc629`.
- `MinimumOSVersion`: `2.2.1`.
- `UIStatusBarHidden`: `true`.
- `__objc_selrefs` içinde `mainScreen` `0x4e4648`, `bounds` `0x4e4644`, `makeKeyAndVisible` `0x4e4628`, `initWithFrame:` `0x4e4640`.
- Kaynakta `addSubview:`, `removeFromSuperview`, `setNeedsLayout`, `setNeedsDisplay`, `setBounds:` dahil ek selector referansları doğrulandı.
- 320×480 puanlık ekran geometrisi orijinal iPhone 2.2.1 dönemi için **uyumluluk varsayımıdır**; orijinal Mach-O tarafından gömülü bir UIKit ekran ölçüsü olarak doğrulanmış değildir.

Tekrar üretim: `python3 tools/audit_owner_uiscreen.py --ipa /path/to/owned.ipa`. Telifli çalıştırılabilir dosyalar depoya yüklenmedi.

## Kodlanan davranışlar

- `uikit_screen.c/h`: `+[UIScreen mainScreen]` ve 16 bayt `CGRect` döndüren `-[UIScreen bounds]`. Native IMP açık gizli dönüş parametresi ile `objc_msgSend_stret` protokolüne göre kuruldu. Geometri kurulumda belirleniyor; PS Vita piksel dönüşümü henüz yok.
- `uikit_application.c`: Orijinal `Info.plist` ile eşleşen gizli durum çubuğu varsayılanı.
- `uikit_bootstrap.c`: `AppController` başlangıç çağrılarını çevreleyen ve çıkışta boşaltılan gerçek Objective-C autorelease havuzu.
- `uikit_hierarchy.c/h`: `UIView.frame` ve `UIView.bounds` için 16 baytlık stret dönüş, `setBounds:`, `setNeedsLayout`, `setNeedsDisplay`, `addSubview:`, `removeFromSuperview`, `superview`, `window`. Alt görünüm sahipliği retain/release ile korunuyor; yinelenen ekleme fazladan retain etmiyor; reparent ve atalardan döngü oluşturma reddediliyor. Gerçek renderer olmadığı için layout/display dirty bayrakları temizlenmiyor.
- 23 native Objective-C class/metaclass çifti, 47 girişli import tablosu. VitaSDK CMake dosyasında 28 C/ASM birimi.
- `tests/test_uikit_screen.c`, güncellenen `tests/test_uikit_hierarchy.c`, `tests/test_uikit_bootstrap.c` ve `tests/test_uikit_application.c` host derleme/test betiğinde. Kaynak düzeyinde tablo/derleme kablolaması tutarlılığı kontrol edildi.

## Tamamlanmamış veya henüz doğrulanmamış

**Doğrulandı:** GitHub Actions [40979438904](https://github.com/Gh0sTRX/iron-man-2-mach-o/actions/runs/40979438904) başarılı tamamlandı. Yeni host ASan/UBSan testleri, 28 C/ASM kaynak biriminin VitaSDK ile ARM derlemesi, tam ELF linki, ELF ABI/import denetimi ve 23 ARM/Thumb Unicorn emülasyon testi geçti. Ayrıntı: `docs/CI_EVIDENCE_UIKIT_2026-10-30.json`. Ancak bu sonuç gerçek oyun kodunun native Vita loader'da çalıştığı veya PS Vita üzerinde açıldığı anlamına gelmez.

`UIApplicationMain` gerçek giriş olarak bağlanmış olsa da Vita event loop, `EAGLView` grafik yüzeyi, OpenGL ES rendering, kullanıcı girdisi, UIKit çizimi, ekran çözünürlüğü/orientasyon köprüsü ve kalan iOS framework/import uyumluluğu henüz tamamlanmadı.

64 özgün harici classref adından 19'u kısmen uygulanmış; 45'i henüz uygulanmamış. Bu değer **oyunun tamamlanma oranı değildir**. Orijinal uygulama native Vita loader üzerinde açılmış veya fiziksel PS Vita'da test edilmiş değildir. VPK/SELF üretilmedi.

## Sıradaki teknik eşik

1. Orijinal Mach-O'daki hâlâ eksik importların öncelikli olanlarını ve ABI'lerini çöz.
2. İlk yürütme için gerçek Vita bellek/RX/cache doğrulama planını uygula (VPK henüz yok).
3. Özgün `EAGLView` metotları (`_createSurface`, `swapBuffers`) için EAGLContext/CAEAGLLayer kurulum zincirini çöz.
4. Event ve render loop olmadan `UIApplicationMain` başarı döndürmesin.
