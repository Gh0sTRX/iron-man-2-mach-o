# VPK derleyicisine giden gerçek yol (29 Ekim 2026)

## Ayrım: paketleme araçları vs çalışan oyun

VitaSDK'nin `vita-elf-create`, `vita-make-fself` ve `vita-pack-vpk` araçları VPK paketlemesi için kullanılabilir. Fakat bunların kurulu olması, orijinal ARMv6 iOS Mach-O oyununu PS Vita'da çalıştırdığımız anlamına gelmez.

## Uygulanan bu tur
- `vendor/ios-loader-boilerplate/CMakeLists.txt`: toolchain seçimini CMake `project()` öncesinde zorla dahil etme hatası kaldırıldı; `-DCMAKE_TOOLCHAIN_FILE` ile doğru çapraz derleyici seçimi kontrol ediliyor.
- Nesne derleme hedefinden gereksiz link-kütüphanesi kontrolü ayrıldı.
- `tools/build_loader_objects.sh`: sadece ARM kaynak nesnelerini derler, VPK üretmez.
- `.github/workflows/loader-readiness.yml`: kaynak derlemesini bağımsız kontrol eder.
- `tools/check_vpk_release_gates.py`: doğrulanmış kanıt gerektiren yayın kapısı.
- `docs/vpk_release_evidence.json`: tüm kapılar başlangıçta `false`; başarı iddiası yok.

## Kalan somut adımlar
1. **VitaSDK nesne derlemesi:** CI çıktısını incele, gerçek compiler hatalarını düzelt.
2. **Tam ELF bağlantısı:** `kubridge_stub`, `vitaGL`, `vitashark`, `SceShaccCgExt`, freetype/png/zlib/bzip2 ve gerekli diğer VitaSDK stub'larıyla link et; tanımsız sembolleri sıfırla.
3. **Mach-O bellek yürütmesi:** kernel bridge ve önbellek senkronizasyonunu gerçek Vita üzerinde doğrula; `guest_exec_ready` kapısını yalnızca gerçek kanıtla aç.
4. **Orijinal iOS importları:** daha önceki ad denetiminde yaklaşık 322 eksik isim vardı; ad eşleşmesi bile doğru ABI ve davranışı kanıtlamaz.
5. **Objective-C:** 25 yerel sınıf, 243 classref yuvası, 482 selector referansı ve dış framework sınıflarının çözümlemesi.
6. **RBS1 ve constructor gerçek cihaz doğrulaması:** host simülasyonu yeterli değil.
7. **GLES, giriş, ses, kaynak dosyaları ve başlatma akışı:** oynanabilir boot testi.
8. **Paketleme:** yalnızca yukarıdakiler tamamlanınca SELF + VPK üret, `eboot.bin` ve içerik bütünlüğünü doğrula.

## Komutlar

```bash
bash tools/build_loader_objects.sh
python3 tools/check_vpk_release_gates.py --evidence docs/vpk_release_evidence.json
```

İkinci komut mevcut kanıt durumunda **başarısız olmalıdır**. Bu, yanlışlıkla tamamlanmış VPK iddiasını önlemek içindir.

**Doğrulama notu:** Bu turda GitHub'a kaynak ve betik değişiklikleri yazıldı. VitaSDK CI derlemesinin başarılı tamamlandığına veya PS Vita'da oyun açıldığına dair bir sonuç elde edilmedi.
