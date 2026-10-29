# Foundation liste ve sözlükleri

Native yükleyiciye kısmi NSArray, NSMutableArray, NSDictionary ve NSMutableDictionary sınıfları eklendi. Bunlar gerçek nesne/içerik saklar ve ömür yönetimine katılır; boş placeholder sınıfları değildir. Oyun veya menü Vita üzerinde çalıştırılmış değildir.

## Özgün dosyayla bağlantı

Owner executable'ın `__objc_selrefs` tablosunda `arrayWithObjects:`, `dictionaryWithObjectsAndKeys:`, `addObject:`, `objectAtIndex:`, `objectForKey:`, `removeObjectAtIndex:`, `setObject:forKey:`, `count` ve `countByEnumeratingWithState:objects:count:` adları bulundu. Bu statik bir çağrı envanteridir; çağrıların çalışma sırasını veya hepsinin açılışta kullanıldığını kanıtlamaz. Dört sınıfın da özgün classref envanterinde birer yuvası vardır.

## Sağlanan davranış

- Listeler içerikleri retain eder. İndeksle erişim, son öğe, sayım, ekleme, değiştirme, silme, bağımsız değiştirilebilir kopya ve immutable snapshot desteklenir. Immutable nesnelere değişiklik metotları kaydedilmez.
- Sözlükler desteklenen NSString anahtarlarını bayt uzunluğu ve içerikle karşılaştırır; anahtarı kopyalar, değeri retain eder. Eşdeğer ama farklı adresli anahtarlar aynı değeri bulur. UTF-8/UTF-16 literal anahtarları ve gömülü NUL içeren anahtarlar desteklenir. Yinelenen anahtarda son değer kalır.
- Kopyalama/deallocation ve autorelease pool gerçek içerik ömrünü korur. Hatalı toplu kurulumda geçici nesneler bırakılır. Sınır dışı erişim, nil ekleme veya desteklenmeyen anahtar hata verir.
- Hızlı dolaşım iOS ARM32'nin 32 baytlık NSFastEnumerationState düzenini kullanır; öğeler listede değer, sözlükte anahtardır. Mutation adresi ve generation doğrulaması vardır. `_objc_enumerationMutation` hata bildirir. Genel Objective-C exception sistemi sağlanmış değildir.
- Mevcut `ml_align_call` köprüsü hizalanmamış guest stack'ten 16 word kopyalar. Bu yüzden nil ile biten variadic factory'ler **en fazla 17 liste öğesi veya 8 sözlük çifti** kabul eder. Daha uzun diziler sessizce okunmaz; hata verir. Pointer/count API'lerinin ayrı sınırı 65.536'dır. Variadic kapsamın tamamlandığı iddia edilmez.
- Genel NSObject anahtar hashing/equality, plist, collection subclass factory'leri, eşzamanlı mutation, döngülerin otomatik toplanması veya bütün Cocoa collection API'leri desteklenmez. NSUserDefaults henüz eklenmedi.

## Test ve derleme

`tools/test_source.sh` gerçek kayıtlı metotları ASan/UBSan altında çağırır: kopyalar, retained içerik, key copy/equality, gömülü NUL, mutation, boş/nested collection, factory/pool ömrü, yanlış anahtar/nil, indeks ve variadic sınır hataları test edilir. Hata yolları ayrı child process'te kontrollü callback ile doğrulanır.

Özel owner testi (`build/host-runtime/test_cfstrings /owner/IronMan2.app/IronMan2`) artık özgün **165 literal'i bir listeye ve sözlüğe** de koyar. Hepsinin metin anahtarıyla erişimi, ömürleri ve kayıt baytlarının değişmemesi denetlenir. 18 UTF-16 çıktısı bağımsız iconv ile karşılaştırılır. Guest ARM komutu çalıştırılmaz; dosya/asset CI'a veya GitHub'a eklenmez.

VitaSDK ile 21 C/ASM biriminin tam ARM ELF bağlantısı başarılıdır. Güçlü tanımsız ELF sembolü yoktur. Gerçek import tablosunda dispatch/normal C köprü ayrımı ve üretim assembly'sinin mevcut 23 emülasyon senaryosu geçti. Host collection testleri özgün ARM collection çağrıları veya fiziksel Vita çalışması sayılmaz.

İsim kapsamı 490 importta **184 eşleşme, 306 eksik**. Gerekli 64 framework sınıfından 8'i kısmen temsil edilir; 56'sı uygulanmamıştır. Bu sayımlar tamamlanma yüzdesi veya uyumluluk kanıtı değildir. UIKit açılışı ve native menü hâlâ açık; SELF/VPK oluşturulmadı.
