# Iron Man 2 özgün Mach-O açılış yolu — 30 Ekim 2026

**Kaynak:** Kullanıcının sahip olduğu orijinal, şifresiz iOS 1.0.0 ARMv6 Mach-O. Yürütülebilir SHA-256: `3ba6337a23ab7e08f2e025ec9731c53a7990e8d611b59eac07ba00cb63acc629`. Bu dosya depoya yüklenmez.

## Özgün giriş noktasındaki somut çağrılar

`llvm-objdump --macho --disassemble --dis-symname _main` kullanılarak doğrudan kaynak dosya üzerinde:

- `_main` başlangıcı: **0x00051aec** (ARM32).
- **0x00051ca0:** `bl _StratMainFunc`.
- **0x00051cb4:** `bl` Objective-C `_objc_msgSend` import stub.
- **0x00051cd0:** `bl` UIKit **`_UIApplicationMain`** import stub.
- `_UIApplicationMain`, özgün 490 dinamik importun içindedir ve native yükleyicide hâlâ karşılanmış değildir.

Böylece doğrudan kendi `main` fonksiyonunun UIKit'i kullanmadığı varsayımı geçersizdir. Ancak bu statik ayrıştırma, tüm runtime olay sırasının veya argümanların davranışını doğrulamaz. `UIApplicationMain` için sahte başarı döndüren stub kullanılmamalıdır.

## Orijinal Objective-C sınıfları

`__objc_classlist` içinde 25 yerel sınıf var; 10'u aşağıdaki ithal UIKit üst sınıflarına bağlanıyor:

| Özgün sınıf | Doğrulanan doğrudan üst sınıf | instance_start |
|---|---|---:|
| EAGLView | UIView | 36 |
| CatalogTableViewController | UIViewController | 92 |
| CatalogViewController | UINavigationController | 168 |
| GameInformationViewController | UIViewController | 92 |
| GameTableViewController | UIViewController | 92 |
| ReviewWriteViewController | UINavigationController | 168 |
| ScreenshotView | UIImageView | 40 |
| UIIconButton | UIButton | 126 |
| WhatsNewViewController | UINavigationController | 168 |
| GLLiveViewController | UINavigationController | 168 |

Ölçüm `class_ro_t.instance_start` ve `LC_DYSYMTAB` harici superclass/metaclass relocation kayıtlarından gelir. Tam liste `OWNER_UIKIT_PARENT_REQUIREMENTS_2026-10-30.json` içinde. Owner-only doğrulama aracı `tools/audit_owner_guest_superclasses.py --ipa OWNED.ipa --expected-json docs/OWNER_UIKIT_PARENT_REQUIREMENTS_2026-10-30.json`.

`AppController` metotları arasında `applicationDidFinishLaunching:`, `initDisplay`, `draw` bulunuyor. `AppDelegate` `applicationDidFinishLaunching:` ve `initWithDevice:` içeriyor. `EAGLView` `initWithFrame:`, `_createSurface`, `swapBuffers`, `layoutSubviews` ve dokunma olaylarını işliyor. Bunlar **orijinal metot isimleri**, davranışları henüz native Vita'da yürütülmedi.

## Bu turdaki native değişiklik

`uikit_hierarchy.c` içinde ölçülen boyutlar ve kalıtım ile ilgili UIKit sınıfları kaydediliyor. Kısmi `UIView` frame/hidden/tag, `UIViewController` view sahipliği sidecar'da tutulur; özgün UIKit'in gizli ivar alanlarına yazılmaz.

- `UIView`: 36 bayt
- `UIImageView`: 40 bayt
- `UIButton`: 126 bayt
- `UIViewController`: 92 bayt
- `UINavigationController`: 168 bayt

`UIResponder`, `UIControl` ve `UIWindow` için **kesin ivar ABI'si çözülmedi**. Kayıtları tam UIKit eşdeğerliği veya özgün kod çalıştırma kanıtı değildir. `UIViewController.view` tembel oluşturma gerektiriyorsa ve view atanmamışsa hata verir. `frame` getter'ının aggregate return ABI'si henüz yok.

## Sonraki gerçek boot engeli

1. Güncel 24 C/ASM nesnesi ile ELF bağlantısını ve ASan/UBSan host testlerini CI'da doğrula.
2. `_UIApplicationMain` girişini kullanıcıdan gelen app/ios argümanları ve delegate yaşam döngüsüne uygun gerçek event/bootstrap akışıyla bağla.
3. Gerekli UIView/UIWindow/EAGLContext/CAEAGLLayer API'leri, `EAGLView` surface, render ve touch bağlamasını orijinal çağrı yerleriyle doğrula.
4. Nesne sınıf kaydı, CFString, guest call ve bellek yürütme denetimlerinin gerçek Vita üzerinde geçtiğini logla.

Referans touchHLE Linux'ta menüye ulaşmıştır; native Vita yükleyicisi bu menüye henüz ulaşmadı. VPK üretilmedi.
