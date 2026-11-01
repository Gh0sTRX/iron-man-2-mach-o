# İlk fiziksel cihaz doğrulaması: durum ve kurulum

Tarih: 1 Kasım 2026. Başlangıç HEAD'i `038be4b`; uygulama commit'i `9db014fddc6c5c5561999f90bb9ce0eafd5f46e4`. Yalnız elle seçilen GitHub tanılama paketi teslimatı: `c288dfef904726550e9882c520c8fd5b79c49698`.

**Fiziksel Vita testi yapılmadı.** Bu oturumun bulut ortamında Vita erişimi, cihaz adresi veya VPN bağlantısı yapılandırılmamış. Hiçbir cihaz raporu alınmadı. Özgün oyunun yürütme kilidi ve `gles_eagl_ready` worker bariyeri korunuyor. Menü/oynanış başarısı bildirilmemiştir.

## Kanıt sınıfları

| İddia / deney | Sınıf | Eldeki kanıt ve sınırı |
|---|---|---|
| Normal üretim ARM ELF'i | **derlendi** | VitaSDK ile link; güçlü çözümlenmemiş sembol yok. Yerel çalıştırma kanıtı değil. |
| Mach-O boyut/aralık ve cleanup davranışı | **derlendi**; ayrıca hostta davranış testi geçti | Gerçek `macho_load`, sentetik dosyalar ve hata enjekte edilen host VM tahsisleriyle ASan/UBSan altında çağrıldı. Vita tahsis API'si modellenmiştir. |
| Özgün `__TEXT`/`__DATA` uzaklığı ve tek slide | **derlendi**; ayrıca hostta davranış testi geçti | Tek image bloğu, ayrı import thunk bloğu; aradaki boşluk ve rebased veri kontrol edildi. Donanım yerleşimi kanıtı değil. |
| Mevcut ARM/Thumb ABI ve runtime köprüleri | **emülatörde test edildi** | 25 assembly dispatch, 6 UIKit ABI örneği, 37 runtime test grubu ve sentetik başlangıç replay testi geçti. Orijinal oyun bu adımda çalıştırılmadı. |
| VM tanılamasının kendi ARM/Thumb talimatları | **emülatörde test edildi** | Aynı C üreticisinden çıkan 6 çağrı/dönüş, data erişimi ve yama sonucu Unicorn'da kontrol edildi. Vita VM API'leri veya cache emüle edilerek başarı üretilmedi. |
| Ayrı grafik ve VM SELF/VPK'leri | **derlendi** | Kimlik/SFO, SELF başlığı, yalnız iki dosyalık paket içeriği ve ELF'te Mach-O başlatıcı sembollerinin yokluğu kontrol edildi. VitaShell kurulumu denenmedi. |
| VitaGL başlatma, GL kimliği, framebuffer ve piksel sonuçları | **doğrulanmadı** | Gerçek cihaz raporu yok; framebuffer/piksel sonucu henüz yok. GPU taklidiyle başarı üretilmedi. |
| RGB565/depth16 istekleri ve derinlik davranışı | **doğrulanmadı** | Cihaz deneyi OES biçim/boyut metadata sorguları ve derinlik örtüşme testi içeriyor. Yerel fiziksel bit hassasiyeti ayrıca doğrulanmış sayılmayacak. |
| Vita VM tahsisi, domain izin geçişleri ve cache tutarlılığı | **doğrulanmadı** | Hazırlanan cihaz deneyi henüz çalıştırılmadı. Per-page RX/W^X ve kapalı domain'de NX negatif testi yapılmıyor. |
| Fiziksel Vita'da test edildi sınıfında sonuç | **doğrulanmadı** | Bu sınıfa alınabilecek hiçbir yeni sonuç yok. |

## Ayrı kurulabilir paketler

| Paket | LiveArea başlığı | Uygulama kimliği |
|---|---|---|
| `build/device-delivery/im2-vitagl-diagnostic.vpk` | IM2 VitaGL Diagnostic | `IM2G00001` |
| `build/device-delivery/im2-vm-diagnostic.vpk` | IM2 VM Cache Diagnostic | `IM2V00001` |

Kullanıcıya teslim edilen dosyalar GitHub artifact üzerinden indirilerek `build/device-delivery/` altında doğrulandı. [GitHub indirilebilir çıktı](https://github.com/Gh0sTRX/iron-man-2-mach-o/actions/runs/41313074796/artifacts/13085473291) iki VPK ve `SHA256SUMS` içerir (30 gün saklama). Yerel yeniden derleme çıktıları ayrıca `build/device-diagnostics/` altında bulunur.

Teslim edilen dosyaların SHA-256 değerleri:

```text
7cc46cc194bc5b963d8053cb9b6b605a23b1832f81571ab3b20c3094ab55ef39  im2-vitagl-diagnostic.vpk
ae9106b924a34dd5921b830f16ec6094ab89d690b217fa6e0af1bb754fea3177  im2-vm-diagnostic.vpk
```

Her paket yalnız `eboot.bin` ve `sce_sys/param.sfo` içerir. IPA, orijinal Mach-O, RBS1 ve oyun assetleri yoktur. Grafik tanılaması yalnız mevcut üretim GLES/OES/array kodunu ve kendi test girişini bağlar; oyun yükleyicisini, Objective-C başlangıcını veya `call_guest` kodunu bağlamaz. VM tanılaması ayrıca bağımsızdır.

Yerel VitaSDK ortamında açıkça paket üretmek için:

```sh
bash tools/build_device_diagnostics.sh
```

Varsayılan push/CI yalnız ELF derler; bu paketleme betiğini çağırmaz. Mevcut workflow_dispatch içindeki `device_diagnostics` seçeneği varsayılan false'tur. Yalnız elle true seçildiğinde kontrollerden sonra paketler ve `SHA256SUMS` GitHub artifact olarak teslim edilir. Normal oyunun VPK release kriterleri ve kanıt dosyası değiştirilmedi. VPK yeniden üretildiğinde ZIP zaman damgaları gibi nedenlerle dosya hash'i değişebilir; dağıtılan dosyanın hash'ini yeniden kontrol edin.

## VitaShell ile ilk test

1. Zaten homebrew çalıştırabilen fiziksel Vita'da VitaShell'i açın. Mevcut bir homebrew uygulaması açılmıyorsa önce bu koşulu çözün; bu tanılama için firmware, sistem dosyası veya kernel eklentisi değiştirmeyin.
2. Paket hash'lerini bilgisayarda doğrulayın. VitaShell'in mevcut USB veya FTP aktarımını kullanarak iki VPK'yi örneğin `ux0:download/` altına kopyalayın. Bu bulut oturumu cihaza bağlanmış değildir.
3. Önce yalnız `im2-vitagl-diagnostic.vpk` dosyasını VitaShell ile kurun. Görünen başlık ve kimliğin yukarıdakiyle aynı olduğunu kontrol edin. Aynı tanılama kimliği önceden kuruluysa bunun o tanılama uygulamasının güncellemesi olacağını dikkate alın.
4. **IM2 VitaGL Diagnostic** uygulamasını bir kez açın. Test sonrasında LiveArea'ya dönebilir; bu dönüş tek başına başarı değildir. VitaShell ile `ux0:data/ironman2_macho/vitagl_environment_probe.json` dosyasını kopyalayın. Önceden rapor varsa bu denemeden önce onu bilgisayara arşivleyin; yeni denemenin raporuyla karıştırmayın.
5. `shader_compiler_file_missing` sonucu çıkarsa durun ve raporu paylaşın. Uygulama `ur0:/data/libshacccg.suprx` veya `ur0:/data/external/libshacccg.suprx` varlığını kontrol eder; kendisi shader derleyicisi kurmaz. Kullanıcının ayrıca yetkilendirmesi olmadan sistem dosyası/eklenti kurulumu yapılmaz.
6. Grafikten bağımsız olarak ikinci VPK'yi kurup **IM2 VM Cache Diagnostic** uygulamasını açın. Yalnız kendi küçük ARM/Thumb talimatlarını çalıştırmayı dener. Uygulama hata nedeniyle kapanırsa son rapor aşaması tanı için gereklidir. `ux0:data/ironman2_macho/vm_execution_probe.json` dosyasını kopyalayın.
7. İki JSON ile birlikte **testin fiziksel PS Vita'da yapıldığını kullanıcı ayrıca doğrulamalı**; cihaz modeli, firmware, deneme zamanı ve hangi paket hash'inin kullanıldığı belirtilmeli. Varsa hata ekranı/görüntü de eklenebilir. Emülatör raporu fiziksel kanıt sayılmaz.

Tanılamaları kaldırmak için yalnız bu iki LiveArea uygulamasını kaldırmak yeterlidir; raporlar yukarıdaki veri klasöründe ayrıca kalabilir. Bu klasör başka proje verilerini de barındırabileceği için tamamını silmeyin.

## Raporları yorumlama

Grafik raporu: `stage`, `failure`, `ready_before_init`, `ready_after_init`, `vendor/renderer/version`, OES storage metadata, native framebuffer binding/status, temizleme/üçgen/depth pikselleri, GL hata sorguları ve cleanup sonucu birlikte değerlendirilir. Başarı için yeşil temizleme, kırmızı üçgen ve kırmızı yakın üçgenin mavi uzak üçgeni örtmesi ayrı ayrı gerçek `glReadPixels` sonuçlarıyla sağlanmalıdır. Kod hata üretirse `gpu_checks_passed` false kalır.

RGB565 ve depth16 üretim köprüsüne gerçek istek olarak gider. OES sorgusu köprünün kayıtlı format/boyutunu doğrular; fiziksel depolama hassasiyetinin bağımsız sorgusu değildir. İncelenen VitaGL kaynaklarında bazı GL bit sorguları sabit değer döndürüyor. Bu nedenle rapor `native_format_precision_verified:false` tutar; başarılı piksel/derinlik testi dahi bunu true yapmaz.

VM raporu: yaklaşık 5 MB'lık **tek** VM image bloğunda özgün `0x1000`/`0x4b9000` adres uzaklığı korunur. API kodları tahsis, base, domain kapatma/açma, iki cache senkronizasyonu ve serbest bırakma için ayrı kaydedilir. Kendi ARM kodu önce 42, Thumb kodu 43 döndürmelidir. Aynı adreslerde kod 77/78'e yamalanır ve tekrar senkronize edilir; ikinci çağrılar bu değerleri döndürmelidir. Ayrı ARM talimatları image içindeki veri uzaklığından önce `0x13579bdf`, sonra `0x2468ace0` okumalıdır.

SDK `sceKernelAllocMemBlockForVM` için en çok 16 MiB, `sceKernelOpenVMDomain` için tüm VM bloklarını yürütülebilir yapma, `sceKernelCloseVMDomain` için yürütmeyi kapatma ve `sceKernelSyncVMDomain` için ilgili aralığın cache senkronizasyonunu tanımlar. Deney bu API'leri yalnız kendi sürecinde kullanır; kubridge veya kernel ayarı değiştirmez. Kaynak: sabitlenmiş SDK'nın `psp2/kernel/sysmem.h` başlığı; [VitaSDK public header](https://github.com/vitasdk/vita-headers/blob/master/include/psp2/kernel/sysmem.h). API dönüşü tek başına donanım garantisi değildir. Kapalı domain'de bilerek talimat çalıştırıp çökme üretme ve per-page izin ayrımı bu deneyin kapsamında değildir.

Her iki rapor da `physical_hardware_confirmed:false` ve `execution_origin:unattested_native_runtime` yazar. Bu alanlar programın kendi donanım kaynağını bağımsız kanıtlayamaması nedeniyledir; elle true yapılmamalıdır. Kullanıcının cihaz teyidi ayrı kanıttır ve hiçbir JSON düzenlemesi üretim bariyerlerini açmaz.

## Gerçek hatalar ve değişen dosyalar

- `source/macho.c`, yeni `source/macho_layout.h`: 32-bit ALIGN taşması giderildi. Yuvarlama, dosya uçları, VM uçları, overlap ve span hesabı 64 bitte yapılır; sıfır boyut, hizasız VM adresi ve SDK limitini aşan tahsisler başlamadan reddedilir. Segmentler iki bağımsız bloğa ayrılmadı.
- `macho.c`: dosya, geçici text, image/thunk blokları ve registry hata yolları ortak cleanup'a toplandı. Sembol tablolarının borç aldığı ham dosya başarılı/gated mapping boyunca bilinçli olarak tutulur. Registry rollback başarısızsa sarkan kimlik bırakmamak için image tutulur ve yürütme kapalı kalır. Tutulan image üzerine tekrar yükleme reddedilir.
- `source/asm.S`, `tests/arm_dispatch_fixture.S`: referansı kalmadığı kontrol edilen sıfır döndüren legacy unimplemented thunk/log kodu kaldırıldı; kullanılan ABI alignment/dispatch kodu korunuyor. Bayat split-RX/fallback yorumları düzeltildi.
- `tests/test_macho_load_bounds.c`, `tests/host_vita/*`, `tools/test_macho_bounds.sh`: gerçek loader'a bozuk segment/dosya girdileri, VM allocate/base hataları ve ham dosya/text/RBS malloc hataları uygulanır. Başarısız denemeden sonra yeni deneme yapılabilirliği, kaynakların bırakılması ve kapalı yürütme kontrol edilir. Metin aramaları bu testlerin yerine kullanılmaz.
- `tests/vitagl_environment_probe/*`, vendor CMake: ayrı tanılama girişinden gerçek üretim grafik kodu; OES format/boyut metadata ve gerçek depth occlusion testi eklendi.
- `tests/vita_exec_probe/*`, `tests/test_vm_probe_code.*`: cache'siz eski deney yerine bounded kendi kodu, domain/cache/cleanup kayıtları ve ARM/Thumb yama deneyi. Hostta tüm yazma aralıkları tarandı; CPU emülasyonu ayrı sınıflandırıldı.
- `tests/device_diagnostics/CMakeLists.txt`, `tools/build_device_diagnostics.sh`, `tests/check_device_packages.py`: ayrı başlık/kimliklerle yalnız açık çağrıda paketleme ve içerik denetimi.
- `tools/build_vm_environment_probe.sh`, `tools/test_source.sh`, `tests/check_vitagl_probe_scope.py`, `.github/workflows/loader-readiness.yml`: varsayılan akışa yalnız derleme/host/CPU testleri eklendi; tanılama VPK üretimi eklenmedi.

Yerel testler ve GitHub Actions geçti:

- İlk kaynak doğrulaması: [41312798523](https://github.com/Gh0sTRX/iron-man-2-mach-o/actions/runs/41312798523); bağımsız VM ELF derlemesi: [41312798578](https://github.com/Gh0sTRX/iron-man-2-mach-o/actions/runs/41312798578).
- Son kaynak/workflow commit'inin varsayılan push akışı: [41313074473](https://github.com/Gh0sTRX/iron-man-2-mach-o/actions/runs/41313074473), başarılı; tanılama paketleme adımı **skipped**.
- Aynı commit'in açıkça seçilmiş elle çalıştırması: [41313074796](https://github.com/Gh0sTRX/iron-man-2-mach-o/actions/runs/41313074796), başarılı; tanılama paketleme ve artifact teslimi başarılı. İndirilen dosyalar `sha256sum -c SHA256SUMS` ile doğrulandı.

Bu başarılar cihaz kurulumu veya native GPU/VM çalıştırması değildir. Makine tarafından okunabilir kayıt: [DEVICE_TEST_BUILD_EVIDENCE.json](DEVICE_TEST_BUILD_EVIDENCE.json).

## Sonraki küçük adım

İlk engel cihaz sonucu eksikliği. Önce yukarıdaki iki paketin raporlarını ve kullanıcının fiziksel cihaz teyidini almak gerekiyor. Hata olursa son aşamadaki API/çizim hatasına odaklanan küçük düzeltme yapılacak. Başarılı olurlarsa bile VM deneyi üretim image/thunk yazma ve cache yolunu bütünüyle doğrulamış sayılmaz; üretim entegrasyonu ve EAGL worker bağlam sahipliği ayrı kontrollü adımlar gerektirir. `IM2_ALLOW_UNVERIFIED_VM_EXEC` etkinleştirilmedi.
