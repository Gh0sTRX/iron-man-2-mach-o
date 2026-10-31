# Özgün C++ metin sistemi: ilk uyumluluk katmanı

31 Ekim 2026. Yeni kod MIT lisanslıdır; std::string uygulaması yeniden yazılmadı veya kopyalanmadı. Mevcut VitaSDK libstdc++ kütüphanesinin eski COW ABI'si kullanılıyor. Özgün oyun/IPA dosyaları depoya eklenmedi.

## Yapılan

`legacy_string.cpp` 24 özgün importu bağlıyor: metin constructor/copy/destructor, append/assign/insert/resize/reserve/push_back/index, find/compare/substr; eski COW leak/mutate/rep-destroy ve boş temsil verisi; inline destructor'ların atomik referans sayacı.

Darwin `size_t` isim kodlaması `m`, Vita'nınki `j`. Köprü 32-bit argümanları açıkça dönüştürüyor. ARM derlemesi string nesnesinin tek 4-bayt sözcük olduğunu static_assert ile şart koşuyor. `_S_empty_rep_storage` gerçek SDK veri adresine bağlanıyor; işlev thunk'ı oluşturulmuyor. `substr` gizli sonuç adresi alıyor; beş argümanlı `insert` yığındaki son argümanı kullanıyor. Yeni koddan çıkan native C++ exception'lar köprü içinde yakalanıp açıklayıcı fatal kayıtla durduruluyor. Bu, iOS SjLj exception unwinding desteği değildir.

## Özgün dosyada karşılaştırılan çağrılar

Executable SHA256: `3ba6337a23ab7e08f2e025ec9731c53a7990e8d611b59eac07ba00cb63acc629`.

- `0x4162dc–0x4162e0`: string'in tek pointer'ı okunup uzunluğu karakter verisinden 12 bayt önce alınıyor.
- `0x4162fc–0x41630c`: substr için r0 sonuç deposu (`sp+4`), r1 kaynak string, r2 başlangıç, r3 uzunluk; çağrı stub'ı `0x470408`.
- `0x4522f8–0x45230c`: insert uzunluğu kaynak string'in data-12 alanından okunup `[sp]` konumuna yazılıyor; r3 sıfır başlangıç; stub `0x4704b0`.
- `0x1f09c–0x1f0b8`: refcount data-4 üzerinde atomik -1; son sahipte rep-destroy, allocator adresi r1; stub `0x470468`.

Bunlar özel owner dosyasının salt okunur statik incelemesidir; özgün oyun kodunun çalıştırılması değildir.

## Test ve sınırlar

Host ASan/UBSan: COW paylaşımı ve writable detach, sahiplik/destruction, ikili sıfır/UTF-8, arama/npos, substring, kapasite ve değişiklik işlemleri, gerçek boş-rep adresi, native exception'ın guest'e kaçmaması.

Unicorn: Tam bağlanmış Vita ARM ELF içindeki üretim köprüsü ve SDK COW kodu, üretim `ml_align_call` yolu üzerinden **32 çağrı** ile çalıştırıldı. 4 ve 8 bayt hizalı giriş yığını, r9 korunması, gizli sonuç, beş argümanlı insert, atomik sayaç ve 12-bayt COW başlığı doğrulandı. Testte heap ve Vita Clib memcpy/memmove/memset işlevleri sınırlı test karşılıklarıdır; bellek işlemleri Vita sistem çağrısına gitmez. Bu cihaz testi değildir.

Tam Vita ARM ELF bağlantısı ve güçlü undefined sembol kontrolü geçti. CI bu testleri oyun verisi olmadan tekrarlar.

Hâlâ açık: iostream/stringbuf ve yerel/exception/RTTI uyumluluğu, diğer C/C++ importları, framework/sistem/grafik/ses boşlukları. SDK'nın yeni COW davranışının özgün oyunun bütün inline kullanım yollarıyla uyumu entegrasyonda ayrıca izlenmeli. Özgün oyun native Vita loader'da açılmadı; fiziksel Vita testi veya VPK yok.
