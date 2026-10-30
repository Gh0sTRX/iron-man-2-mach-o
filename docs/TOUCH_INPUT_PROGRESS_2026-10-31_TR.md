# Native dokunma zinciri ve UIKit ARM ABI düzeltmesi

Özgün owner SHA256: `3ba6337a23ab7e08f2e025ec9731c53a7990e8d611b59eac07ba00cb63acc629`.

## Ölçülen özgün arayüz

EAGLView metotları: touchesBegan:withEvent: 0x53f24, touchesMoved:withEvent: 0x539f4, touchesEnded:withEvent: 0x534c4, touchesCancelled:withEvent: 0x52fa8; touchesChangedWithEvent: 0x52d28. ARM incelemesinde NSSet fast-enumeration, UIEvent.allTouches, UITouch.phase ve CGPoint locationInView: için objc_msgSend_stret görülüyor (örn. 0x530d4/0x530e8). Oyun kodu depoya eklenmedi; bu adresler statik ölçümdür, native guest çalışması kanıtı değildir.

## Çalışan kaynak kodu

Bağımsız MIT sets.c/h immutable NSSet üyelik/sahiplik ve enumeration sağlar. NSString/NSNumber/NSSet değer eşitliği, diğer uygun nesnelerde identity, count/anyObject/member/contains/allObjects/copy ve NSArray köprüsü mevcut. Arbitrary custom equality/hash, mutable set ve tüm Cocoa API'leri sağlanmaz; unsupported equality tahmin edilmez.

touches.c/h gerçek UITouch/UIEvent nesneleri oluşturur. Donanım kimliği bir temas boyunca aynı nesneye bağlanır. Began/Moved/Stationary/Ended/Cancelled, önceki/current konum, view/window ve monotonic process-time kaynaklı timestamp tutulur. CGPoint getter'ları explicit 8-byte stret IMP'idir; double timestamp dönüşü iOS r0/r1 için raw integer bits kullanır.

Input görünür key window'ın mevcut UIView frame/bounds ve translation hiyerarşisinde hit-test edilir. En üstteki uygun view seçilir; ilk hedef temas boyunca capture edilir. View gizlenir, interaction kapanır, penceresi değişir veya view kaldırılırsa cancel gönderilir. Siyah kenarda başlayan temas, parmak kalkana kadar oyuna yeni begin üretmez. Aktif capture view dışına taşarken devam eder. Çoklu dokunma UIView.multipleTouchEnabled ile kontrol edilir. UIControl action processing, affine transforms, custom hitTest override'ları ve gesture recognizers tamamlanmadı; desteklenen yol özgün EAGLView'in UIKit touch metotlarıdır.

Her event tüm mevcut/sonlanan touch nesnelerini owned NSSet'te tutar; phase/view grupları özgün registered IMP'lerine gider. Guest target gerçek call_guest köprüsüne, native target doğru C void prototipine yönlendirilir. Yeni yaratılan sentetik game draw/touch fonksiyonuna atlanmaz. Callback'ler autorelease pool içindedir. Snapshot teardown ve retained event/touch ömürleri test edilir. En fazla altı front-touch temas hedeflenir; VitaSDK'nin raw report dizisi sekiz eleman tanımladığı için native adaptörde kopyalama öncesi ayrıca kapasite guard'ı vardır.

SceTouchPeek hiç yeni sample vermediğinde parmak bırakma uydurulmaz. Poll hatasında cancellation denenir ve sürücü hata verir. UI loop turn ayrıca input/IME callback'leri için bir outer pool kullanır. Gamepad/analog eşlemeleri henüz yapılmadı. Tap count şimdilik tek temas için 1'dir; multi-tap clustering yoktur. Görüntü dönüşümü mevcut landscape-right FBO UV eşlemesinin tersidir; aynı display_geometry.h sabitleri renderer ile paylaşılır. Koordinat/rotasyonun fiziksel Vita doğrulaması yapılmadı.

## Bulunup düzeltilen diğer gerçek ABI sorunları

UIView initWithFrame/setFrame/setBounds native IMP'leri CGRect'yi hard-float HFA olarak alabiliyordu. iOS ARMv6 bunları r2/r3 + stack'ten gönderiyor. Üç IMP ARM altında base AAPCS pcs("aapcs") ile sabitlendi. Yeni test, üretimdeki uikit_hierarchy.c'yi bağımlılık fixture'leriyle cross-compile edip gerçek IMP'leri production ml_align_call üzerinden Unicorn'da çalıştırır. VFP arg register'ları bilerek NaN ile zehirlenir; altı senaryoda doğru frame/bounds core word'lerden okunur. Bu test yalnız annotation'ı veya kopya metodu değil gerçek üretim CGRect girişlerini çalıştırır; OS/class allocator bağımlılıkları sentetiktir.

BOOL argümanlarının sadece düşük byte'ı anlamlıdır. UIView hidden/interaction/multiple, UIApplication status/idle ve NSTimer repeats yolları buna göre daraltıldı. UIApplication.setIdleTimerDisabled özgün launch'ta 0x50530 çağrısıyla isteniyor; state/getter ve native loop'ta gerçek sceKernelPowerTick bağlantısı eklendi. Güç yönetimi fiziksel cihazda doğrulanmadı.

## Testler ve kalan sınır

- Host ASan/UBSan: NSSet dedup/equality/ownership, native ve original-style guest callback köprüsü, CGPoint stret, kimlik/phase, stationary suppression, önceki konum, capture, cancel, default single/multitouch, siyah kenar, altı temasın aynı turn'de değişmesi, event lifetime, reentrancy/thread/clock/bounds guard'ları.
- 37 C/ASM kaynak biriminin full Vita ELF bağlantısı başarılı. Mevcut 25 ARM/Thumb dispatch senaryosu ve 6 üretim UIKit CGRect ABI senaryosu başarılı.
- Bu testler özgün EAGLView ARM metotlarını veya Vita donanımını çalıştırmış değildir. Gerçek oyunun native boot'u hâlâ çözülmüş sayılmaz; gerekli import/ABI, full runtime, GPU/ses/cihaz doğrulaması açık. SELF/VPK oluşturulmadı.
