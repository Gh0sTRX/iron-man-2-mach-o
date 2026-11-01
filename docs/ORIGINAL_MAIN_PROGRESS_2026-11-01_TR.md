# Özgün giriş koduna ve çalışma thread'ine ulaşma

1 Kasım 2026 — bağımsız Mach-O dalı. `Iron-man-2-os` değiştirilmedi.

Bu aşamada özgün `_main`, `UIApplicationMain` ve oyunun uygulama açılış kodu Unicorn ARM tanılamasında çalıştırıldı. 25 özgün sınıf, 165 CFString sabiti ve 297 constructor kayıt/çalıştırma aşamasından geçildi. **Menü, fiziksel Vita, GPU görüntüsü, ses ve oynanış doğrulanmadı.**

## Bulunan ve düzeltilen gerçek engeller

1. Sınıf okuyucusu ve runtime 64 metotla sınırlıydı; özgün `ReviewWriteViewController` 65 metot içeriyor. İki katmanda ortak, denetimli 128 metot sınırı kullanıldı. Son metodu gerçekten lookup eden pozitif test ve sınırı aşan listeyi reddeden negatif test var. Diğer metadata/ABI kontrolleri kaldırılmadı.
2. Özgün giriş `+[NSAutoreleasePool new]` kullanıyor. NSObject `+new`, dinamik `+alloc` ardından dinamik `-init` çağıracak şekilde eklendi. Guest/native override'lar, nil alloc ve +1 sahiplik korunur; guest çağrıları gerçek `call_guest` kullanır.
3. Özgün açılış `+[UIColor blackColor]` ve görünüm arka planını kullanıyor. Kısmi UIColor; immutable siyah/beyaz/şeffaf RGBA değerleri ve autorelease ömrü sağlar. UIView arka planı retain eder, değiştirirken veya yok edilirken release eder ve çizim isteğini işaretler. **Renk durumunun saklanması GPU'da çizildiği anlamına gelmez.** Genel UIColor renk uzayları/CGColor/API desteği yok.
4. Sonraki engel `setExclusiveTouch:` idi. BOOL low-byte davranışı, getter ve aynı pencerede yeni dokunuşların rakip görünümlere gitmesini engelleyen gerçek yönlendirme eklendi. İki farklı görünümde her iki edinme sırası, normal çoklu alıcı ve döndürülmüş koordinat hit-test'leri sınandı.
5. Mevcut pthread köprüsünde malloc/attr/create hatalarının temizliği eksikti. NULL giriş ve OOM kontrolü, attr hatalarında doğru temizleme ve pthread_create başarısızlığında context'in serbest bırakılması eklendi. Darwin attr çevirisi ve gerçek thread scheduling ayrıca açık işlerdir; bu değişiklik bunları tamamlamaz.

## Tanılama sınırı

Yeni `--register-metadata`, üretim guest sınıf kaydını, CFString kaydını ve class/super reference doğrulayıcısını kullanır. Reader, veri okumalarını eşlenen segmentlerle ve metot yürütme adreslerini özgün `__text` bölümüyle sınırlar. Kategoriler hâlâ reddedilir.

`--probe-main` açıkça seçilirse, çözümlenmemiş sınıf referansı hataları raporda tutulur ve yalnız bu ayrı tanılama yolunda bounded `_main` denemesi yapılır. Desteklenmeyen kod/veri/sınıf adresleri isim veren tuzaklardır. **Bu mod bütün sınıf referansları doğrulandı veya bütün importlar bağlandı demek değildir.** Üretim yükleyicisinin tam import, metadata, RX/cache ve VPK kapıları değiştirilmedi.

Sistem dili, bulunmayan yeni-kurulum tercih dosyası, tek-thread mutex/TLS kimliği ve deterministik monoton saat açık fixture'lardır. Native Vita OS veya gerçek zaman testi değildir. Özgün kodun `_pthread_create` çağrısında tanılama isim ve thread giriş adresiyle durur; scheduler olmadan sahte thread başarısı dönmez. Thread köprüsünün gerçek trampoline/guest callback yolu ayrı sentetik ARM testinde çalıştırılır.

```sh
/path/to/python-with-unicorn tools/replay_owner_startup.py \
  --elf build/loader-full/im2_ios_loader \
  --macho /owner/IronMan2 --rbs1 /owner/rebase.bin \
  --output /owner/main-probe.json --initialize-frameworks \
  --register-metadata --probe-main
```

`--probe-main`, metadata ve framework seçeneklerini gerektirir. Özgün dosya diskte değiştirilmez; oyun kodu, IPA veya asset bu commit'e eklenmez. Hatalı metadata `_main` çağrılmadan durur. Sentetik girişin 42 döndürdüğü ve taşan CFString verisinin reddedildiği testler var. Owner ile gerçek sonuçlar ve sınırlar [ORIGINAL_MAIN_EVIDENCE_2026-11-01.json](ORIGINAL_MAIN_EVIDENCE_2026-11-01.json) dosyasında.

## Kontroller ve kalanlar

Tam VitaSDK ELF bağlantısı ve yerel host ASan/UBSan testleri geçti. Yeni üretim ARM senaryoları: 17 `+new`, 55 renk/arka plan/exclusive ve 14 pthread çağrısı. Gerçek thread trampoline'i context'i serbest bırakıp orijinal ABI callback'ini çağırıyor; r9 korunuyor. OS thread yaratma/scheduling testlerde modellenmiştir.

Kaynak envanteri 339/490 isim eşleşmesi, 151 eksik isim; gerekli harici sınıflar 27 kısmi / 37 uygulanmamış durumunda. Bunlar tamamlanma yüzdesi değildir. En yakın işler özgün worker kodunun izlenmesi, Darwin thread/attr/TLS/senkronizasyon uyumu ve grafik başlatma zincirinin tamamlanmasıdır. Worker girişinin statik kod penceresinde `CFRunLoopRunInMode` ve `pthread_join` çağrıları da görüldü; ikisi de eksik envanterindedir. Bu inceleme worker yürütme kanıtı değildir. Gerçek Vita RX/cache ve cihaz kabulü ayrıca gereklidir. SELF/VPK üretilmedi.

Uygulama kodu `c732350`, ayrı CGRect fixture düzeltmesi `7ba2959`. İlk CI tam yükleyiciyi derledi, fakat yeni renk bağımlılıkları ayrı CGRect fixture'ında eksikti; bu iki bağımlılık beklenmeyen kullanımda trap veren test girişleriyle tamamlandı. Son [GitHub CI işi](https://github.com/Gh0sTRX/iron-man-2-mach-o/actions/runs/41258624835) **başarılı**: tüm host kontrolleri, VitaSDK ELF bağlantısı, ARM/Thumb/CGRect ve sentetik metadata/main testleri geçti. Owner dosyası CI'a gönderilmedi; özgün `_main` yürütme kanıtı yerel özel dosya denemesidir.
