# Foundation kaynak erişimi — 30 Ekim 2026

Bağımsız Mach-O yükleyicisine, gerçek runtime sınıfları kullanan `foundation.c/h` modülü eklendi. Modül hem Vita derlemesine hem host davranış testlerine bağlıdır. Diğer port deposu değiştirilmedi; SELF/VPK üretilmedi.

## Çalışan kod

- **NSString:** UTF-8 creation/init, UTF-16 birimi cinsinden `length`, `UTF8String`, ASCII/UTF-8 C-string erişimi ve immutable copy/ownership. Overlong, surrogate, U+10FFFF üstü, bozuk/truncated UTF-8 reddedilir. `getCString:maxLength:encoding:` buffer küçükken veya encoding desteklenmiyorken NO döndürür ve buffer'a kısmi yazmaz. Önceki kayıtlı implementasyon buffer'ı kırpmasına rağmen YES döndürebiliyordu.
- **NSData:** `data`, `dataWithBytes:length:`, `dataWithContentsOfFile:`, `initWithBytes:length:`, `bytes`, `length`, `getBytes:length:` ve immutable `copy`. Byte verisi gerçekten kopyalanır; dosya gerçekten okunur. Eksik dosya ve dizin boş/sahte başarılı NSData'ya çevrilmez. Byte-count sınırı 64 MiB; mapped/no-copy/mutable varyantlar henüz desteklenmez.
- **NSBundle:** `mainBundle`, `bundlePath`, `resourcePath`, `pathForResource:ofType:` ve `pathForResource:ofType:inDirectory:`. App bundle yolu config'den gelir; kaynak yolu ancak dosya gerçekten mevcutsa döner. Relative resource/subdirectory alt kümesi desteklenir. Localization arama sırası, farklı bundle'lar, plist sözlükleri ve URL API'leri tamamlanmadı.
- Dosya okumaları runtime'da mevcut Darwin path mapper'a bağlanır. Bu, bütün tarihsel iOS sandbox path biçimlerinin doğru çevrildiği anlamına gelmez.
- NSString/NSData/NSBundle kayıtları gerçek ISA/metaclass ve method tablolarıyla kurulur. Native class data importları startup'ta bir kez hazırlanır; lookup'ta tekrar yazılmaz. NSObject kökü ve NSAutoreleasePool korunur.

Eski genel selector dispatcher'daki dummy bundle nesnesi ve Inotia'ya ait `1.1.41` sürüm dönüşü kaldırıldı. `objectForInfoDictionaryKey:` yerine tahmin edilmiş Iron Man 2 sürümü koyulmadı; gerçek plist desteği eklenene kadar bu selector desteklenmiyor. Bilinmeyen selector'lar sessiz başarıya çevrilmez.

## Testler

`tests/test_foundation.c`, üretimdeki aynı Foundation ve ObjC runtime C kodunu ASan/UBSan ile çalıştırır. Gerçek kayıtlı IMP'ler selector araması üzerinden de çağrılır; yalnız bağımsız yardımcı fonksiyon testi değildir.

Testler: Türkçe karakter/emoji UTF-16 uzunluğu; bozuk UTF-8; buffer/encoding hatasında değişmeden kalan çıktı; init ve copy ömrü; kaynak bellek değişince bozulmayan NSData; nil ve büyük boyut hataları; eksik dosya/dizin; gerçek geçici dosyanın binary okunması; gerçek bundle resourcePath/resource lookup; eksik kaynak; alt dizin; havuz drain sonrasında retained NSData'nın yaşaması. Test dosyaları sentetik ve geçicidir; özgün oyun varlığı kullanılmaz.

Tüm kaynak kontrolleri ve ASan/UBSan testleri geçti. Kaynak sayısı 19 C/ASM oldu; tam Vita ARM ELF bağlantısı ve bağlı ELF import-kind denetimi geçti. Önceki üretim assembly testleri de 23/23 geçti. Bunlar host/CPU-emülasyon ve derleme kanıtlarıdır; Vita cihaz veya özgün oyun testi değildir.

## Kapsam ve açık engeller

64 dış framework sınıfından NSString, NSAutoreleasePool, NSData ve NSBundle kısmen temsil ediliyor; 60 sınıf henüz uygulanmadı. NSObject ayrıca runtime köküdür. Tam Foundation/NSData/NSBundle eşdeğerliği iddia edilmiyor.

Özgün IPA/Mach-O/RBS1 bu ortamda hâlâ bulunmuyor. CFString sabitlerinin tanınması/adoption, UIKit başlangıcı, kalan iOS importları, plist/dictionary/URL API'leri ve aggregate return/initialize/load gibi runtime kapsamları açık. `stringWithFormat:` hâlâ önceki sınırlı Darwin variadic formatter'ını kullanır; bu turda tam printf/NSObject description eşdeğerliği sağlanmadı. Pool olmadan autoreleased factory çağrıları da tam Cocoa davranışı sayılmaz.

İlgili API davranışları için incelenen kaynak: Swift açık kaynak Foundation `NSData.swift` (https://github.com/swiftlang/swift-corelibs-foundation/blob/main/Sources/Foundation/NSData.swift). Swift/Apple uygulama kodu kopyalanmadı; yeni modül bağımsız MIT kodudur. Eski vendored upstream lisansı hâlâ ayrı açık konudur.

VPK yayın koşulları değişmedi: yalnız kaynak derlemesi ve ELF bağlantısı doğrulanmış durumda. Tam import/runtime, özgün boot ve fiziksel cihaz kapıları kapalıdır.
