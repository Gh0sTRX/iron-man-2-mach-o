# Topluluğa taşınabilir katkı planı — Iron Man 2 ARMv6 iOS → PS Vita

## Ne yapıyoruz?

Bu proje **decompiler çıktısından yeniden yazılmış oyun** değildir. Kullanıcının sahip olduğu, iPhone OS 2.2.1 dönemine ait özgün ARMv6 Mach-O üzerinde PS Vita için **yerel yürütme/uyumluluk katmanı (HLE)** geliştiriyoruz. iOS framework davranışlarını, Objective-C metaverisini, ARM ABI farklarını ve OpenGL ES çağrılarını kısmen yeniden uyguluyoruz.

[**touchHLE**](https://github.com/touchHLE/touchHLE) de orijinal erken iOS uygulama kodunu yürütmek için sistem framework'lerini yüksek seviyede uygular; ancak bu proje Vita üzerinde ARM'den ARM'e yerel yükleme, vitaGL grafik çevirisi ve cihaz kısıtlarıyla uğraşır. touchHLE'nin kaynak kodunu kopyaladığı veya bu projeyi resmi olarak desteklediği iddia edilmez.

## Oyundan bağımsız paylaşılabilecek parçalar

- Mach-O/LC_SEGMENT, LC_DYSYMTAB, external reloc/classrefs denetçileri
- RBS1 relocation ve bounded guest VM/ASLR doğrulama yöntemi
- ARMv6 iOS → Vita AAPCS çağrı köprüsü, register ve stret testleri
- Objective-C 2 class/metaclass, selector, ARC öncesi retain/release ve pool testleri
- Foundation/UIKit/EAGLContext/CAEAGLLayer **test edilebilir alt kümeleri**
- EAGL iOS OES → vitaGL FBO köprüsü (henüz tüm renderbuffer/depth/stencil durumlarını kapsamaz)
- Sembolik özgün IPA envanter/selector audit araçları ve **tamamen yapay** test fixtures
- Donanım test prosedürü, başarısızlık ve uyumluluk rapor formatı

Oyun bağımsız bir açık kaynak SDK/örnek yükleyici oluşturmak mümkün; aynı IPA için bile başka GPU, ses veya iOS API davranışı gerekebilir. **Tek tuşla IPA→VPK portu garantisi yoktur.**

## Yayına çıkarılmaması gerekenler

- Telifli Gameloft/Marvel çalıştırılabilir dosyası, oyun varlıkları, lisanslı içerik veya şifresi çözülmüş IPA'nın bütünü
- Orijinal oyundan çıkartılmış yüksek hacimli ikili kod parçaları
- İzin/licence kontrolü yapılmadan üçüncü taraf kaynak dosyaları

Kendi geliştirdiğimiz kod ve sentetik testleri ayrı incelemek, üçüncü taraf bağımlılıkların (ör. vitaGL) lisanslarına uymak, oyun sahipliği/yasal izinleri gözetmek gerekli. Hukuki koşullar ülke ve dağıtım biçimine göre değişebilir.

## Ölçülebilir kabul kriterleri

1. Bağımsız host ASan/UBSan + VitaSDK ARM ELF CI yeşil.
2. Başarısız/eksik iOS API yerine **fail-closed** davranış; yanlış sonuçla boot'u sürdürme yok.
3. En az bir gerçek Vita üzerinde, ayrı loglarla Mach-O relocate → ObjC bootstrap → EAGL surface → ilk gerçek frame zincirinin doğrulanması.
4. İkinci bir **hukuken edinilmiş** erken iOS uygulamasında hiçbir oyun-özel veri paylaşmadan aynı loader modüllerinin yeniden kullanılabilirliğinin gösterilmesi.
5. API ve ABI uyumsuzluk envanterinin, regresyon testleri ve kullanıcıdan bağımsız belgelendirmeyle sürdürülmesi.

**Şimdiki durum:** Host testleri, ARM derleme/ELF bağlantısı ve 23 ARM/Thumb fixture için CI kanıtı mevcut; fiziksel Vita ve orijinal guest yürütme kanıtı yok. Bu nedenle VPK dağıtımı hâlâ bloke.

Topluluk katkısını yalnızca tamamlanmış porttan sonra değil, taşınabilir araç ve CI testlerini temizleyerek daha erken de verebiliriz. Ancak private depoyu herkese açık hâle getirmek kullanıcı onayı olmadan yapılmamalıdır.
