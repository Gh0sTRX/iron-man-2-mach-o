# Gerçek framework başlangıcıyla özgün constructor testi

1 Kasım 2026 (İstanbul). Yeni `--initialize-frameworks` seçeneği regresyon fixture'ının sentetik sınıflarını devralmaz: ELF veri/BSS durumu yeniden yüklenir, test heap/TLS/once kayıtları temizlenir ve gerçek `darwin_init` / `objc_init` çağrılır.

Sistem dili Türkçe, tercih dosyası bulunmayan yeni kurulum ve tek iş parçacıklı kernel mutex/thread kimliği açıkça modellenir. Genel dosya erişimi başarılıymış gibi yapılmaz; yalnız beklenen tercih dosyasına `stat` çağrısı ENOENT döndürür. Diğer yollar reddedilir. Kernel fixture'ları fiziksel OS veya thread scheduling testi değildir.

Özgün dosyayla ölçülen sonuç:

- Önceki fixture'da bağlanmamış 187 isim vardı; gerçek sınıf başlatıcısıyla 152 kaldı. Ek 35 isim mevcut sınıf/veri bağlantılarıdır; yeni 35 API uygulanmış sayılmaz.
- 297 özgün constructor ve kaydedilen 359 kapanış callback'i tamamlandı; kuyruk boşaldı.
- `_main`, menü, GPU ve fiziksel Vita çalışması henüz doğrulanmadı. Gerçek eksik isim envanteri 152 olarak değişmedi.

Sentetik replay testi eski modu, yeni framework modunu ve eksik kod/veri tuzaklarını sınar. Yeni modda üretim `class_imports` tablosundaki 67 class/metaclass/CFString veri kaydının adres ve türü kontrol edilir. Yeni moda ait sentetik test geçti. Üretim yükleyicisinin import/RX/VPK kapıları değiştirilmedi; VPK üretilmedi.

Önceki komuta `--initialize-frameworks` eklenir:

```sh
/path/to/python-with-unicorn tools/replay_owner_startup.py \
  --elf build/loader-full/im2_ios_loader \
  --macho /owner/IronMan2 --rbs1 /owner/rebase.bin \
  --output /owner/framework-startup.json \
  --initialize-frameworks --drain-exit-callbacks
```

Sayılar ve hash'ler: [FRAMEWORK_STARTUP_EVIDENCE_2026-11-01.json](FRAMEWORK_STARTUP_EVIDENCE_2026-11-01.json). Sıradaki iş özgün ObjC metadata/CFString kayıtlarını bu deneyde kurup `_main` yolunu sınamaktır. Yalnız framework başlangıcının geçmesi bu işlerin tamamlandığını göstermez.
