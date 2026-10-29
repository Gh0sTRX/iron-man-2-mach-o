# Özgün oyunun sabit metinleri native yükleyiciye bağlandı

Vita yükleyicisine ARM32 CFString sabitlerini gerçek NSString kimlikleri olarak tanıma desteği eklendi. Bu, özgün uygulamanın açılışta kullandığı metin ve sınıf adı sabitleri için gerekli bir Foundation adımıdır. Henüz Vita'da oyun açılışı veya menü kanıtı değildir.

## Uygulama

- `__cfstring` kayıtları symbol binding ve guest class doğrulamasından sonra okunur. ISA, bayrak, uzunluk, adres, sonlandırıcı ve payload sınırları denetlenir. Yalnız bu görüntüde görülen 0x7c8 ASCII ve 0x7d0 UTF-16LE biçimleri desteklenir; başka biçimler açıkça reddedilir.
- Sabitlerin özgün adresleri korunur. UTF-16 metni UTF-8 önbelleğine çözülür; `length` UTF-16 birimi sayar. Orijinal kayıt veya payload üzerinde değişiklik yapılmaz. Nesne kayıtları heap nesnesi gibi serbest bırakılmaz; retain/release/autorelease/copy sabitleri hayatta tutar.
- Her kayıt grubunun tamamı doğrulanmadan nesne kayıtları yayınlanmaz. Kimlik çakışmaları ve bozuk Unicode tüm grubu reddeder. Kayıt ve teardown yalnız başlangıç/test aşaması içindir; çalışan guest altında eşzamanlı değiştirme desteklenmez.
- `___CFConstantStringClassReference` gerçek NSString sınıfına bağlanır. `CFStringGetCString` UTF-8/ASCII için Foundation dönüşümünü kullanır. `CFRelease`/`CFRetain` yalnız runtime tarafından yönetilen nesneleri kabul eder; genel CoreFoundation desteği değildir. `NSClassFromString` kayıtlı sınıfları arar, bilinmeyen ad için nil döner.
- `%@` ve NSLog artık nesne adresinin +8 alanını körlemesine C-string saymaz. Kayıtlı metin çözücü kullanılır; desteklenmeyen nesne hata verir. NSString format üretimindeki mevcut 1024 bayt sınırı aşılırsa sessiz kırpma yerine hata verilir. Genel printf/variadic uyumluluğunun tamamlandığı iddia edilmez.

## Ölçülen sonuç

Özel owner executable SHA256: `3ba6337a23ab7e08f2e025ec9731c53a7990e8d611b59eac07ba00cb63acc629`.

Gerçek Mach-O'nun **165 sabitinin tamamı: 147 ASCII ve 18 UTF-16LE**, üretim decoder'ından geçti. 18 UTF-16 metnin çıktı baytları bağımsız Linux iconv dönüşümüyle karşılaştırıldı. Gerçek sınıf metotları, UTF-16 uzunlukları, retain/release/autorelease/copy ve kayıt baytlarının değişmemesi kontrol edildi. Test, yalnız ISA external binding'ini host sınıfıyla modeller; ARM komutu, constructor veya oyunun açılışını çalıştırmaz.

Asset içermeyen ASan/UBSan testleri Türkçe karakter/emoji, gömülü NUL, hatalı surrogate, yanlış ISA/bayrak/hizalama, taşan adres/uzunluk, eksik sonlandırıcı, çift kayıt, toplu geri alma ve `%@` nesne çözümlemesini kapsar. Üretim ARM dispatcher'ın 23 emülasyon senaryosu da geçti. VitaSDK ile 20 C/ASM biriminin tam ARM ELF'i bağlandı; güçlü unresolved ELF sembolü yok. Bu, iOS importlarının tamamının karşılandığı anlamına gelmez.

İsim sayımı **179/490 eşleşme; 311 eksik**. Bu sayı davranış/ABI doğruluğu veya tamamlanma yüzdesi değildir. UIKit başlangıcı, kalan Apple API'leri, grafik/ses/girdi ve gerçek Vita RX/cache doğrulaması açık. VPK/SELF üretilmedi. Linux touchHLE menü kanıtı ayrı rapordadır.

## Tekrarlama

```sh
bash tools/test_source.sh
docker exec im2-macho-build bash tools/build_loader_elf.sh
python3 tests/test_elf_imports.py --elf build/loader-full/im2_ios_loader
```

Owner metin testi yalnız yerel özel executable verilerek çalıştırılır; CI'da özgün oyun dosyası gerekmez:

```sh
build/host-runtime/test_cfstrings /owner/IronMan2.app/IronMan2
```

IPA, executable, metin payload'ları veya ekran görüntüleri GitHub'a eklenmedi. Yeni CFString modülü bağımsız MIT uygulamasıdır; touchHLE kaynak kodundan alınmadı.
