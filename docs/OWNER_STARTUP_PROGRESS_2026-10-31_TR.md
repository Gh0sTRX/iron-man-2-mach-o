# Özgün başlangıç kodu ve gerçek kapanış kayıtları

31 Ekim 2026 — bağımsız Mach-O dalı. `Iron-man-2-os` değiştirilmedi.

## Ölçülen sonuç

İlk kez bu tanılama yolunda özgün Iron Man 2 ARM başlangıç kodu çalıştırıldı:

- 297 statik başlangıç işlevinin tamamı geri döndü.
- Bu işlevlerin kaydettiği 359 kapanış callback'i gerçek üretim kuyruğuyla çağrıldı; kuyrukta 0 kayıt kaldı.
- Gerçek üretim external relocation planlayıcısı 5.225 kaydı uyguladı; 906 harici indirect slot bağlandı.
- Özgün kod; metin oluşturma/yıkma, ağaç düğümleri, stream Init, bellek ve matematik köprülerini çağırdı. Sayımlar [JSON kanıtında](OWNER_STARTUP_REPLAY_2026-10-31.json) bulunur.

Bu sonuç **Unicorn ARM emülatöründe**, derlenen VitaSDK uyumluluk ELF'iyle elde edildi. `_main`, menü ve oynanış çağrılmadı. Fiziksel Vita, GPU, ses, kontroller ve nonzero-slide RX/cache doğrulanmadı.

## Üretim düzeltmesi

Eski `__cxa_atexit` callback'i saklamadan başarılı dönüyordu. Yeni MIT lisanslı `exit_registry.c/.h`, `atexit` ve `__cxa_atexit` kayıtlarını ortak LIFO kuyruğunda tutar. Callback'ler kilit dışında gerçek `call_guest` ile çalışır; ARM/Thumb ve misafir r9 değişimi desteklenir. Kayıt, tüketilmeden önce kuyruktan çıkarılır; tekrar drain aynı callback'i ikinci kez çalıştırmaz.

Callback içinden yeni kayıt mümkündür. Eşzamanlı/özyinelemeli drain reddedilir. Kuyruk 4.096 kayıt, bir drain 65.536 callback ile sınırlıdır; taşma ve sonsuz yeniden kayıt sessizce yutulmaz. DSO kimliği saklanır fakat `dlclose`/seçimli `__cxa_finalize` uygulanmış sayılmaz. Hem misafir `exit(code)` hem `_main` dönüşü drain yapar; çıkış kodu korunur. `_exit`/abort gibi bütün sonlandırma yollarının sağlandığı iddia edilmez.

## Tanılama yolu ve sınırlar

`tools/replay_owner_startup.py`, yalnız kullanıcının özel Mach-O ve RBS1 girdileriyle çalışır. Girdiye mevcut katı preflight uygulanır. Orijinal adreslerde, sıfır slide ile belleğe eşlenir; üretim import router'ı, ABI thunk'ları, relocation planlayıcısı, constructor doğrulayıcısı ve `call_guest` kullanılır. Oyun dosyası diskte değiştirilmez.

Desteklenmeyen importlar ayrı tuzak adreslerine bağlanır. Bunlara kod veya veri olarak erişilirse test sembolün adıyla durur; sahte başarılı sonuç dönmez. Bu deney üretim yükleyicisinin tam import zorunluluğunu değiştirmez. Üretim native yürütme/RX ve VPK kapıları kapalı kalır.

Heap, OS mutex/TLS/zaman ve GPU girişleri mevcut regresyon fixture'ıdır. Tam `objc_init` bu fixture'da çalıştırılmadığı için 187 dinamik olarak bağlanmamış isim vardır; bu sayı gerçek kaynak envanterindeki 152 eksik isimle karıştırılmamalıdır. Henüz kaydedilmemiş sınıf/veri binding'leri de fixture sayısına dahildir. Ölçülen constructor/cleanup yolunda bu tuzaklara erişilmedi.

SjLj Register/Unregister normal akış placeholder'ları başlangıçta üçer, temizlemede birer kez çağrıldı. **Exception fırlatma/unwinding desteği doğrulanmadı.** Tam oyun motoru veya native açılış başarısı çıkarılamaz.

```sh
/path/to/python-with-unicorn tools/replay_owner_startup.py \
  --elf build/loader-full/im2_ios_loader \
  --macho /owner/IronMan2 --rbs1 /owner/rebase.bin \
  --output /owner/startup-report.json --drain-exit-callbacks
```

Depo kökünden çalıştırılır. Araç önce mevcut gerçek ARM regresyon fixture'ını çalıştırır; çıktı raporuna oyun kodu veya dosya içeriği eklemez. Girdi ve ELF hash'leri raporlanır.

## Kontroller

- Tüm host ASan/UBSan kontrolleri geçti; yeni test gerçek callback'ler, 8 thread ile kayıt, LIFO/tek çağrı, dinamik kayıt, kapasite ve döngü sınırlarını sınadı.
- 49 kaynak birimiyle VitaSDK ELF bağlantısı geçti; güçlü undefined sembol yok. SELF/VPK üretilmedi.
- ARM regresyonuna 15 gerçek registry çağrısı eklendi; sentetik ARM ve Thumb callback gövdeleri, iki yığın hizası ve r9 korunması geçti.
- Yeni özel dosyasız replay testi: sentetik ARM constructor gerçek import thunk'ını çağırıyor; hem eksik işlev yürütme hem eksik veri okuma tuzakları doğrulanıyor. Bu test CI akışına eklendi.

Kaynak isim envanteri 338/490 eşleşme ve 152 eksik isimdir; davranış doğrulaması veya tamamlanma yüzdesi değildir. Kaynak commit `1c1b830` için [GitHub CI işi](https://github.com/Gh0sTRX/iron-man-2-mach-o/actions/runs/41241376939) hesap ödeme/harcama limiti nedeniyle başlamadı; yerel test başarısı CI başarısı sayılmaz.

## Sıradaki somut engel

Statik constructor aşaması bu sınırları belirtilmiş deneyde geçti. `_main` öncesinde tam native sınıf/veri binding'leri, özgün ObjC metadata ve CFString kayıtlarının aynı deneyde kurulması gerekiyor. Ardından özgün UIApplicationMain/delegate zinciri isim veren tuzaklarla sınanabilir. Native Vita yürütme ve fiziksel cihaz kabulü ayrıca gereklidir.
