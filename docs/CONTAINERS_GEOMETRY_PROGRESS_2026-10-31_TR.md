# Özgün C++ düğümleri ve CoreGraphics değerleri

31 Ekim 2026 — bağımsız Mach-O projesi. Yeni kod MIT lisanslıdır. C++ liste/ağaç algoritmaları kopyalanmadı veya yeniden yazılmadı; mevcut VitaSDK libstdc++ uygulamasına uyumluluk köprüleri yazıldı. Özgün dosyalar depoya eklenmedi.

## 8 C++ importu

`legacy_containers.cpp`, eski `std::_List_node_base` hook/unhook/transfer ile kırmızı-siyah ağaç increment/const increment/decrement/insert/erase işlevlerini SDK'ya bağlar. ARM derlemesi liste başlığını 8 bayt, ağaç başlığını 16 bayt ve alan offsetlerini static_assert ile şart koşar. Ağaç insert'in tek baytlık bool argümanında üst bitler temizlenir. Düğüm payload'ı, allocator/deallocator ve koleksiyon eleman sayısı özgün çağırana aittir. Köprü bu alanları üstlenmez.

SDK'nın const increment bildirimi header'da yokken eski uyumluluk sembolü arşivde mevcuttur; doğrulanmış pointer→pointer imzasıyla açıkça bağlanır. Boş liste aralığının transfer'i baştan atlanır. Geçersiz iterator veya bozuk pointer'lar için genel bellek doğrulaması sağlanmaz; özgün STL sözleşmesi geçerlidir.

Host ASan/UBSan testleri dört farklı sırada 127 elemanla toplam **508 ekleme ve 508 silme** çalıştırır. Her işlemden sonra parent bağlantısı, kırmızı düğümün çocukları, eşit siyah yükseklik, siyah root, sıralama, en küçük/en büyük bağlantıları ve payload korunması kontrol edilir. Mutable/const/reverse iterator'lar ve iki liste arasındaki aralık taşıma da sınanır.

Tam bağlanmış Vita ARM ELF'inde üretim köprüleri ve SDK algoritmaları **948 gerçek ARM çağrısıyla** çalıştırıldı. Ham 8/16-bayt guest düzenleri, her işlem sonrası ağaç invariant'ları, payload guard'ları, üç farklı silme sırası ve bool üst bitleri kontrol edildi. 4 ve 8 bayt hizalı giriş yığını, r9 ve yığın korunması da sınandı. Düğüm algoritmalarının bellek işlemleri gerçek ARM kodudur; fixture çevresindeki heap/Clib/TLS karşılıkları önceki runtime testleri için kalır. Bu özgün oyun veya cihaz yürütmesi değildir.

## 5 CoreGraphics importu

`cg_geometry.c`, gerçek `CGPointZero` ve `CGRectZero` verileri ile `CGAffineTransformMakeRotation`, `CGAffineTransformMakeScale` ve `CGRectOffset` işlevlerini ekler. iOS ARM32 CGFloat=float düzeni ve `pcs("aapcs")` kullanılır. Struct sonuçlarının gizli output pointer'ı ile, CGRect argümanının genel yazmaç/yığın arasında bölünmesi korunur.

**18 gerçek ARM çağrı testi**: sıfır/pozitif/negatif açı, pozitif/negatif/sıfır ölçek, pozitif/negatif dikdörtgen kaydırma; NaN ile zehirlenmiş VFP girişleri, output guard'ları ve iki yığın hizası. Global sıfır değerleri işlev thunk'ı yerine veri olarak okunur.

Bu işlem hesaplamaları, UIView affine transform desteği, dönen görüntü, menü çizimi veya ışıklandırma çözümü değildir. CGContext/image/GPU ve view transform entegrasyonu açık kalır.

## Özgün executable ile karşılaştırma

SHA256: `3ba6337a23ab7e08f2e025ec9731c53a7990e8d611b59eac07ba00cb63acc629`.

- `0x116a8–0x116c0`: 12-bayt liste düğümü allocate edilir; oyun payload'ı +8'e yazılır; hook stub `0x470528`.
- `0x3ef438–0x3ef448`: list transfer'den önce boş aralık atlanır; stub `0x470540`.
- `0x956c–0x9598`: 24-bayt tree node allocate; payload +16/+20; insert argümanları r0..r3; stub `0x4705d0`.
- `0xa67b0–0xa67c4`: header root +4, left +8, right +12; count header dışında +16. Bu düzen köprüye gömülü node-count yazılmamasını gerektirir.
- `0x15e788–0x15e790`: rotation r0 çıktı adresi ve r1 float açı; stub `0x470204`.
- `0x15e8c8–0x15e8d8`: scale r0 çıktı, r1/r2 float ölçekler; stub `0x470210`.
- `0x1630c8–0x1630ec`: CGRectOffset r0 çıktı, rect r1/r2/r3 ve stack, dx/dy stack; stub `0x4702f4`.

Bunlar owner dosyasının salt okunur statik incelemesidir; oyun kodunun çalıştırılması değildir. Otomatik ARM testleri sentetik girişler kullanır.

## Kalan sınır

490 özgün importtan 277 ad eşleşir, 213 ad eksiktir. Bu sayılar davranış garantisi veya tamamlanma yüzdesi değildir. 39 framework sınıfı hâlâ uygulanmamıştır; mevcut 25 sınıf kısmi kapsamlıdır. iostream/RTTI/SjLj exception, framework/dosya/sistem/GLES açıkları sürer. Özgün oyun native Vita loader'da açılmadı; cihaz testi/VPK yok. Yürütme ve paketleme korumaları gevşetilmedi.
