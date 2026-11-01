# İş parçacığı bekleme, iptal ve mutex düzeltmeleri

1 Kasım 2026 — bağımsız Mach-O projesi. Bu tur, özgün worker'ın ihtiyaç duyduğu bekleme/iptal bağlantılarını ve mevcut mutex köprüsünün hatalı durumlarını ele alır. **Worker'ın veya menünün fiziksel Vita'da çalıştığı iddia edilmez.**

## Gerçek kod değişiklikleri

Yeni MIT lisanslı `thread_lifecycle.c/.h`, `pthread_join`, `pthread_cancel`, `pthread_setcanceltype` ve `pthread_testcancel` importlarını gerçek pthread hizmetlerine bağlar. Yeni `pthread_mutex_trylock` mevcut Darwin mutex düzenini kullanır. İşlemler boş başarı döndürmez.

ABI değerleri eşit değildir:

| Anlam | Darwin/iOS | Pinned VitaSDK/PTE |
|---|---:|---:|
| Güvenli noktada iptal / deferred | 2 | 1 |
| Asenkron iptal | 0 | 0 |
| İptal edilmiş thread sonucu | pointer 1 | pointer -1 |
| EDEADLK | 11 | 45 |
| EAGAIN | 35 | 11 |
| ENOTSUP | 45 | 134 |

Darwin deferred=2 native SDK deferred sabitine çevrilir; önceki tür ters yönde çevrilerek döndürülür. Geçersiz Darwin türü 1 SDK'ya iletilmez. Join sonucu yalnız başarıda, tam dört byte olarak yazılır; iptal edilmiş sonuç Darwin token'ına çevrilir. NULL sonuç pointer'ı desteklenir; sıfır thread handle ESRCH döndürür. İlgili pthread hataları pozitif Darwin kodlarına çevrilir; genel libc errno uyumluluğu iddia edilmez. Yeni eşleme thread oluşturma ve mutex yollarında da kullanılır.

Mutex değişiklikleri:

- İlk lazy başlatma ortak native mutex ile korunur, imza atomik olarak yayımlanır. Normal Darwin statik initializer desteklenir; başka bilinmeyen imzalar normal mutex diye tahmin edilmez.
- Attr/init başarısızlığı geçerli imza yayımlamaz; lock/trylock geçersiz native handle'ı kullanmaz. Retry mümkündür.
- Kullanımdaki mutex'in destroy işlemi EBUSY döndürürse imza korunur. Ancak başarılı destroy sonrasında silinir.
- NULL/bozuk attr ve desteklenmeyen mutex tipi EINVAL döndürür. Darwin attr init/type hatalarının native mutex kullanımına taşınması engellenir.

## Kanıtlar

Host ASan/UBSan testinde gerçek pthread oluşturuldu, dönüş değeri join ile alındı; başka gerçek worker deferred cancellation noktasında iptal edildi. Cleanup handler tam bir kez çalıştı ve join Darwin canceled=1 sonucunu verdi. Kontrollü hata enjeksiyonu başarısız join/type çağrılarının çıktı belleğini koruduğunu ve deadlock/again/unsupported dönüşümlerini sınadı. Asenkron iptal türüne geçiş yalnız pending cancel yokken sınandı; karmaşık guest yığını iptal/unwind desteği sağlanmış sayılmaz.

VitaSDK ELF'i üzerinde **32 yeni ARM lifecycle** ve **34 ARM mutex** çağrısı geçti. Gerçek derlenmiş adaptörler, Vita PTE sabitleri, iki stack hizası, nullable/4-byte çıktılar, init/destroy hata yolları, init guard sırası ve retry sınandı. Bu ARM testlerinde OS scheduling/join/cancel/mutex işlemleri modellenmiştir; gerçek eşzamanlı Vita davranışı değildir.

Özgün kodun salt okunur çağrı yerleri: join `0x512b4` ve `0x51a60`; trylock `0x2f0c7c`; setcanceltype `0x4627b4` (r0=0, r1=NULL); testcancel `0x4627c4` / `0x4627d4`; cancel `0x462b98` / `0x462c98`. Bunlar bu fonksiyonların owner binary'de kullanıldığını gösterir; owner worker'ı çalıştırılmış sayılmaz.

Özel dosyayla regresyon, özgün 25 sınıf / 165 CFString / 297 constructor sonrası `_main` açılışının thread isteğine ulaştığını kontrol eder. Tanılama `pthread_create` önünde scheduler sınırında durur; özel oyuna sahte thread sonucu verilmez. Metadata class-reference hatası hâlâ raporlanır; yalnız açık `--probe-main` tanılaması bu hatayı tuzak adresleriyle taşıyabilir. Üretim kapıları değişmedi.

Kaynak isim kapsamı 344/490 eşleşme, 146 eksik isimdir. Eşleşme davranış veya tamamlanma yüzdesi değildir. Yeni kaynak sayısı 50'dir. Özel IPA/oyun byte'ları GitHub'a gönderilmez; SELF/VPK üretilmez.

## Kalan temel işler

`CFRunLoopRunInMode` hâlâ eksik. Mevcut NSTimer scheduler'ı başlatıldığı main thread'e aittir; worker döngüsüne sorgusuz taşınamaz. Worker'a ait TLS, kaynak/timer sahipliği ve gerçek run-loop bekleme/uyandırma davranışı kurulmalı. Darwin thread attr çevirisi, mevcut detach davranışı, guest cancellation/unwind ve gerçek Vita thread scheduling kabulü de açık işlerdir. Yeni API'ler bu kapsamların tamamlandığı anlamına gelmez.

Darwin sabitleri için [Apple pthread public header](https://github.com/apple-oss-distributions/libpthread/blob/main/include/pthread/pthread.h), native değerler için pinned SDK `pthread.h` / `sys/errno.h` incelendi. Apple uygulama kodu kopyalanmadı.

Ölçüm özeti: [THREAD_SYNC_EVIDENCE_2026-11-01.json](THREAD_SYNC_EVIDENCE_2026-11-01.json).

Kaynak commit `798059e`. [GitHub CI](https://github.com/Gh0sTRX/iron-man-2-mach-o/actions/runs/41259515906) **başarılı**: host testleri, VitaSDK bağlantısı, ARM/Thumb ve sentetik replay geçti. Owner ile özel dosya denemesi CI dışında, yerelde yapıldı.
