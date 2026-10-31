# Özgün CoreFoundation dizi yolu ve nesne ömrü

31 Ekim 2026. Yeni modüller MIT lisanslıdır; Apple/touchHLE kaynak uygulaması kopyalanmadı. Owner executable/IPA depoya eklenmedi.

## Ölçülen ihtiyaç ve uygulama

Özgün executable SHA256: `3ba6337a23ab7e08f2e025ec9731c53a7990e8d611b59eac07ba00cb63acc629`.

`0x1791b8–0x1791cc` çağrısında `CFArrayCreateMutable` varsayılan allocator değeri, capacity=0 ve callbacks=NULL alır; stub `0x470174`. Diğer özgün çağrılar: append `0x1792b8` → `0x470168`; count `0x178c8c` → `0x470180`; at-index `0x178cb0` → `0x47018c`; remove-all `0x17903c` → `0x470198`. Bu statik incelemedir, oyun yürütmesi değildir.

Bu ayarlarda değerler retain/release edilmeden saklanır. Normal NSArray sahiplik davranışını kullanmak özgün nesne ömrünü değiştirir. `cfarrays.c` bu nedenle bağımsız, non-retaining bir depolama yolu sağlar. Yeni importlar:

- `CFArrayCreateMutable`, `CFArrayAppendValue`, `CFArrayGetCount`, `CFArrayGetValueAtIndex`, `CFArrayRemoveAllValues`.
- `kCFAllocatorDefault`: NULL allocator değerini içeren gerçek, salt okunur pointer verisi; işlev thunk'ı değildir.

Create +1 nesne döndürür; Get ödünç değer döndürür. Aynı pointer birden fazla kez saklanabilir; değerler incelenmez veya dereference edilmez. Clear/destruction yalnız diziye ait depoyu yönetir. Dizi nesnesi mevcut managed ObjC runtime üzerinden retain/release edilir. Özel somut `__IM2CFMutableArray` sınıfı kullanılır; NSArray veya uygulanmış bir harici framework sınıfı sayılmaz.

Önceden `objc.c` içinde bulunan `CFRetain`/`CFRelease` sınırı davranışı korunarak `cf_memory.c` dosyasına taşındı. Böylece aynı üretim import yönlendirmesi ve nesne ömrü kodu, SDK/GUI bağımlılığı olmadan birlikte test edilir. Bu iki mevcut import yeni çözülmüş bağlantı sayısına eklenmedi.

## Sınırlar

Yalnız ölçülen default allocator + capacity=0 + NULL callbacks yolu desteklenir. Özel allocator, pozitif/negatif capacity veya callback tabloları açıklayıcı fatal hatayla reddedilir. Dinamik dizi en çok 65.536 değer tutar; aşım reddedilir. Mutasyonun dışarıdan seri yürütülmesi gerekir.

Bu genel CFArray/NSArray toll-free bridging, CFTypeID, custom callback veya değiştirilemez CFArray desteği değildir. Desteklenmeyen türler, ölü diziler ve indeksler reddedilir. İki özel ObjC erişim metodu tam NSArray davranışı sağlamaz. NULL callback dizileri opak değer ve NULL saklayabilir; Get değeri sahiplenmez.

## Doğrulama

Host ASan/UBSan: üretim `imports.c` router'ı, CF dizi modülü, ortak CF lifetime modülü, Foundation ve gerçek managed runtime birlikte bağlanır. Pool olmadan Create/Get, tekrar eden değerler, non-retaining nesne ömrü, ölmüş değerlerden sonra clear, opak pointer'lar, NULL/self değerler, 1.025 elemana büyüme, temizleme/yok etme ve ordinary Foundation nesnelerinde retain/release doğrulandı. 11 ayrı hata yolu child process içinde kontrol edildi; genel testler de geçti.

Unicorn: tam bağlı Vita ARM ELF'inin **118 üretim CF/runtime çağrısı** çalıştırıldı. Gerçek sınıf kayıt, CFArray, CFRetain/Release ve managed object kodları kullanıldı. İki yığın hizasında, duplicate/dangling/raw değerler, büyüme, +1/borrowed kuralları ve dizi/nesne silindikten sonra heap'in başlangıç kayıt seviyesine dönmesi doğrulandı. Yalnız heap/Clib bellek işlemleri ve OS lock/TLS erişimleri test karşılıklarıdır; Vita mutex scheduling, allocator ve cihaz davranışı doğrulanmış değildir.

Önceki 32 C++ string, 56 math, 948 container, 18 CoreGraphics ARM çağrısı ve 25 dispatch/6 CGRect IMP vakası da geçti. Tam Vita ARM ELF bağı başarılı; güçlü undefined ELF sembolü yok.

## Durum

490 özgün importtan 283 ad eşleşiyor; **207 ad eksik**. Bu ad kapsamıdır; uygulama doğruluğu veya tamamlanma yüzdesi değildir. 25 kısmi/39 eksik harici framework sınıfı sayısı değişmedi. Native özgün boot, fiziksel Vita, grafik/ses/oynanış doğrulaması açık; SELF/VPK üretilmedi ve yürütme korumaları değiştirilmedi.
