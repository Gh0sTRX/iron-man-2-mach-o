# Worker zamanlayıcıları ve özgün worker giriş denemesi

1 Kasım 2026 — bağımsız Mach-O projesi. Bu turda main thread'e bağlı zamanlayıcı sınırı kaldırıldı ve özgün worker'a ayrı bir tanılama girişi eklendi. **Tam eşzamanlı oyun yürütmesi, menü veya fiziksel Vita başarısı değildir.**

## Üretim kodu

NSTimer artık her pthread için ayrı bir kuyruk, monoton saat geçmişi ve reentry durumu tutar. Timer yalnız kendi thread'inden kullanılabilir; worker'ın poll/cancel işlemleri ana kuyruğu etkilemez. Thread çıkışındaki TLS destructor, kalan zamanlayıcıları callback çalıştırmadan iptal eder. Timer nesnesi owner thread'den uzun yaşarsa scheduler'ı referansla canlı tutar; başka thread'deki son release eski TLS belleğine erişmez. Thread'e ait autorelease pool'ların genel exit cleanup'ı bu değişikliğin kapsamı değildir.

Yeni `runloop.c/.h`, **default-mode / timer-only** `CFRunLoopRunInMode` ve `kCFRunLoopDefaultMode` bağlantılarını sağlar. Mode sabiti gerçek managed NSString'dir ve borrowed data global olarak sunulur. Native clock/wait callback'leri Vita process saati ve delay hizmetlerine bağlanır.

Özgün worker çağrısı `0x51a30` incelendi: mode r0, double low/high r1/r2 ve Boolean r3'tedir. Literal çiftinin süresi **1 mikro saniye**dir (`0xa0b5ed8d`, `0x3eb0c6f7`). iOS ARMv6 packed düzenine göre integer-word adapter yazıldı; native double/VFP veya fazladan AAPCS padding kullanılmaz.

Desteklenen davranış:

- Boş current-thread default mode, Finished (1) döndürür.
- Kuyruktaki zamanlayıcılar gerçek native/guest callback yoluyla çalışır; kalan süreye ve sıradaki timer'a göre beklenir. Süre dolunca TimedOut (3) döner.
- Timer callback'i **source handled** değildir. `returnAfterSourceHandled=true` bile timer nedeniyle HandledSource (4) üretmez.
- Başka modlar, nested run-loop, geçersiz/çok büyük süreler, geri giden saat ve wait hataları açıkça reddedilir.

Mach port, network/reachability source'ları, perform-block, CFRunLoopStop ve NSRunLoop sınıfı tamamlanmış sayılmaz. Desteklenmeyen source registration'a sahte başarı verilmez. Asenkron guest cancellation/unwind davranışı ve gerçek Vita scheduling ayrıca açıktır.

## Testler

Host ASan/UBSan testi gerçek iki worker pthread'i ve ana kuyruğu aynı anda kurdu. Worker timer'ları yalnız kendi callback'lerini çalıştırdı; main kuyruğu korunmuş kaldı. Thread exit, repeating timer'ı temizledi; owner'dan uzun yaşayan timer sonradan başka thread'de güvenle bırakıldı. Cross-thread timer invalidation reddedildi. Timeout, 1us giriş, timer-only sonuçlar, mode/nested/clock/wait hata yolları da geçti.

VitaSDK ELF'inde **26 yeni ARM çağrısı**: packed r1/r2 ABI, gerçek mode globali, gerçek guest timer callback'i, r9 korunması, iki stack hizası, timeout ve sahiplik temizliği geçti. ARM testinin saat/bekleme/OS thread kimliği fixture'dır; fiziksel Vita testi değildir.

## Özgün worker kodu

Yeni `--probe-worker`, main `_pthread_create` isteğinde durduktan sonra worker girişini ayrı stack ve modellenmiş pthread-key değerleri/kimliğiyle çağırır. Main devam ettirilmez; OS/libc reent ve bazı fixture'lar ortaktır. Bu bir concurrent scheduler değildir. Guest mutex/join/semaphore/cancel işlemlerinde contention veya wake-up sonucu tahmin edilmez; tanılama isim veren sınırda durur.

```sh
/path/to/python-with-unicorn tools/replay_owner_startup.py \
  --elf build/loader-full/im2_ios_loader \
  --macho /owner/IronMan2 --rbs1 /owner/rebase.bin \
  --output /owner/worker-probe.json --initialize-frameworks \
  --register-metadata --probe-main --probe-worker
```

Özgün dosyada 25 sınıf, 165 CFString ve 297 constructor aşaması geçti. Main'in istediği `0x51984` worker girişi bu ayrı denemede çalıştı; CFBundle/CFURL ile kaynak dizini hazırlama çağrılarını geçip **`chdir` eksik bağlantısında** durdu (`LR=0x5127c`). Worker'ın tamamlandığı veya özgün CFRunLoop çağrısına ulaştığı iddia edilmez. Yeni run-loop davranışının kanıtı yukarıdaki host/ARM testleridir.

Sentetik worker 42 döndüren örnekte ayrı stack/TLS girişinden geri döndü; ikinci örnekte mutex kilidine gelince worker synchronization boundary ile durdu. Eksik sınıf referansları hâlâ raporlanır; yalnız açık tanılama seçeneği bunları tuzaklarla taşıyabilir. Üretim native import/metadata/RX ve VPK kapıları değiştirilmedi.

Kaynak isim kapsamı 346/490 eşleşme, 144 eksik isimdir; bu tamamlanma yüzdesi değildir. 51 kaynak birimi derlendi; SELF/VPK üretilmedi. Oyun/IPA byte'ları GitHub'a eklenmedi.

Sıradaki somut iş, özgün `chdir` ve relative asset yollarını Vita dosya düzenine gerçek dizin kontrolüyle uyarlamaktır. Sonraki worker senkronizasyonu, desteklenmeyen run-loop source'ları ve gerçek cihaz grafik/girdi/ses kabulü de kalır.

Ölçüm: [RUNLOOP_WORKER_EVIDENCE_2026-11-01.json](RUNLOOP_WORKER_EVIDENCE_2026-11-01.json).

Kaynak commit `5d6c8d9694ded2b4c35692f3ec966f5ae34c5baa`. [GitHub CI](https://github.com/Gh0sTRX/iron-man-2-mach-o/actions/runs/41302291233) **başarılı**: host ASan/UBSan, VitaSDK, ARM/Thumb ve sentetik worker/replay testleri geçti. Owner dosyasıyla deneme yerel ve ayrıdır; dosya CI'a gönderilmedi.
