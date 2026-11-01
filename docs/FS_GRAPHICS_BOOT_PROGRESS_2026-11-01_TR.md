# Özgün worker: oyun klasöründen EAGL hazırlık kontrolüne

1 Kasım 2026 — bağımsız Mach-O projesi. Worker'ın `chdir` engeli çözüldü; özel owner uygulama diziniyle yapılan ayrı emülatör denemesi artık **EAGL/OpenGL arka ucunun hazır olma kontrolüne** ulaşıyor. **Menü, çizilmiş kare veya fiziksel Vita başarısı değildir.**

## Dosya yolları

Yeni MIT lisanslı `fs_path.c/.h`, process-wide sanal çalışma klasörü sağlar. Native host/SDK process cwd'si değiştirilmez. İlk cwd mevcut DATA_PATH'tir; iOS absolute yolları veri köküne, relative yollar geçerli cwd'ye yönlendirilir. Açık `ux0:`, `ur0:` ve `app0:` yolları korunur.

`chdir`, çözümlenen yolun gerçek `stat` sonucu directory olduğunu kontrol edip ancak başarıda cwd'yi değiştirir. Eksik dosya, directory olmayan hedef ve stat hatası eski cwd'yi korur. Yol kesilmez; sınır veya output kapasitesi hatası açık hata üretir. `getcwd` buffer'lı ve `NULL,0` malloc-owned biçimleri destekler. `darwin_path`, fopen, mkdir ve mevcut Foundation path mapper aynı cwd'yi kullanır. Cwd lexically normalleştirilir; Unix symlink/physical-realpath ve silinmiş cwd'nin tam POSIX davranışı sağlanmış sayılmaz.

Host ASan/UBSan testi gerçek geçici directory/file byte'larıyla relative okuma, başarısız chdir, `file/..` reddi, absolute/root/mount yolları, getcwd sınırları/sahipliği ve başka pthread'in ortak cwd değişimini gördüğünü doğruladı. Native host cwd'sinin değişmediği de kontrol edildi. **25 ARM cwd/path çağrısı** gerçek native stat-mode layout'u, output koruması ve fopen/mkdir yönlendirmesini sınadı; ARM OS filesystem'i fixture'dır.

## Grafik açılışındaki sonraki iki bağlantı

`setOpaque:` / `isOpaque`, UIView sidecar'ına ve CALayer/CAEAGLLayer native durumuna eklendi. UIView default YES, CALayer default NO kullanır; Darwin BOOL low-byte kuralı korunur. UIView değişiminde display isteği işaretlenir. Guest UIView instance boyutu değiştirilmedi. Bu **durum/sorgu desteğidir; GPU saydamlık çıktısı doğrulanmadı.** Host testlerine ek olarak 8 UIView ve 21 CALayer ARM kontrolü eklendi.

Ardından özgün worker `_printf` çağrısında duruyordu. Yeni `console_printf.c/.h`, mevcut Darwin packed-word formatlayıcısını strict modda kullanır, çıktıyı iki geçişle boyutlandırır ve gerçek stdout'a `fwrite` ile tam byte sayısını yazar. Native printf'e doğrudan bağlanmadı: iOS ARMv6 double/64-bit varargs yerleşimi farklıdır. Embedded NUL dahil çıktılar korunur; short write/OOM ve 1 MiB sınırı hatayla sonuçlanır. `%n`, wide/unsupported ve bozuk biçimler tahmini çıktı üretmek yerine reddedilir. Genel libc printf uyumluluğu iddia edilmez; mevcut legacy formatlayıcının eski kullanıcıları korunur.

Host testleri integer/double/64-bit/object biçimleri, exact byte/NUL, çıktı hataları ve desteklenmeyen biçimleri sınadı. **9 ARM printf çağrısı** register/stack sınırını geçen packed argümanları doğruladı. Owner tanılamasının stdout'u modellenen bir log sink'idir; fiziksel konsol veya ekran çıktısı değildir.

## Özgün dosyayla ölçülen ilerleme

```sh
/path/to/python-with-unicorn tools/replay_owner_startup.py \
  --elf build/loader-full/im2_ios_loader \
  --macho /owner/IronMan2.app/IronMan2 --rbs1 /owner/rebase.bin \
  --app-dir /owner/IronMan2.app --output /owner/worker-fs.json \
  --initialize-frameworks --register-metadata --probe-main --probe-worker
```

`--app-dir`, özel uygulama klasörüne read-only gerçek stat kontrolü bağlar. Native stat yapısında yalnız st_mode modellenir; kalan metadata sıfırdır. Sadece bundle içi ve belirtilmiş veri kökü directory'si karşılanır; bilinmeyen yollar ve bundle dışına çıkan resolve sonuçları reddedilir. Tercih dosyası hâlâ açık fresh-install absent-file fixture'ıdır. Genel dosya I/O veya game asset okuma backend'i sağlanmış sayılmaz.

Özgün worker `0x51984`, ayrı stack ve modellenmiş pthread-key/kimlik ile, main durdurulmuşken çalıştırıldı. Gerçek çıkarılmış app directory'sinin varlığı doğrulandı; chdir ve opaque mesajları geçildi. Worker'ın konsol çağrısı da tamamlandı. Son sınır `context_set_current` içinden çağrılan **`gles_eagl_ready`** oldu. Tanılama bu noktada durur; production `gles_init`/GPU ortamı bulunmadan sahte ready sonucu verilmez.

Özgün worker eşzamanlı scheduling, gerçek pthread TLS/libc reent ayrımı veya native GPU ile çalışmış sayılmaz. Main sınıf-reference hatası hâlâ kayıtlıdır; yalnız opt-in tanılama bunları trap token'larıyla taşır. Native production import/metadata/RX ve VPK kapıları değiştirilmedi. Bu turda orijinal game asset içeriği yüklenip çizilmedi; directory stat ile gerçek byte okuyan sentetik host testi ayrı kanıtlardır.

53 kaynak birimiyle ELF bağlantısı ve host/ARM/Thumb/sentetik worker testleri geçti. Kaynak isim envanteri **349/490 eşleşme, 141 eksik isim**; tamamlanma yüzdesi değildir. IPA/oyun byte'ları ve özel konsol metni GitHub'a eklenmedi; SELF/VPK oluşturulmadı.

Sıradaki ana iş gerçek VitaGL/EAGL başlatma, worker graphics thread davranışı ve cihaz RX/cache kabulüdür. Eksik diğer API'ler, gerçek asset I/O, run-loop source'ları, scheduling, ses/girdi ve oynanış doğrulaması da açık kalır.

Ölçüm: [FS_GRAPHICS_BOOT_EVIDENCE_2026-11-01.json](FS_GRAPHICS_BOOT_EVIDENCE_2026-11-01.json).

Kaynak commit `9a75c434f1d6cb459af36ffc8eb27b386fed89f9`. [GitHub CI](https://github.com/Gh0sTRX/iron-man-2-mach-o/actions/runs/41305723253) **başarılı**. Özel owner dosyası CI'a gönderilmedi; onun yürütme kaydı yerel ve ayrıdır.
