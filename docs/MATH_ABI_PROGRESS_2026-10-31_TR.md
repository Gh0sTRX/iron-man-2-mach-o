# Matematik ve sayı okuma köprüsü

31 Ekim 2026. `math_bridge.c`, özgün IPA'nın eksik 23 libm/libc importuna gerçek VitaSDK işlevleri üzerinden davranış kazandırır. MIT lisanslı köprü; kütüphane algoritmaları kopyalanmadı.

Kapsam: `acos`, `asin`, `asinf`, `atan`, `atanf`, `atan2f`, `ceil`, `ceilf`, `coshf`, `exp`, `fmax`, `fmin`, `fmodf`, `frexp`, `log`, `log2`, `modf`, `roundf`, `sinhf`, `tan`, `tanhf`, `atof`, `strtod`.

Bütün işlevler açık `pcs("aapcs")` sınırı kullanır. iOS float/double argümanları ve sonuçları genel amaçlı yazmaçlardan aktarılır; köprünün içinden Vita hard-float kütüphanesi çağrılır. Bunlar doğrudan native hard-float işlev adresine bağlanmaz.

Tam bağlanmış Vita ARM ELF'in gerçek kodu Unicorn'da **56 çağrı** ile sınandı. Her işlev 4 ve 8 bayt hizalı başlangıç yığınında çalıştırıldı; VFP giriş yazmaçları NaN ile dolduruldu. r9/yığın korunması, float sonucu r0'dan ve double sonucu r0/r1'den okuma, frexp exponent/modf integral çıktı adresleri ve strtod bitiş adresi doğrulandı. Negatif yarım sayının sıfırdan uzağa yuvarlanması, negatif sıfır, bir argümanı NaN olan min/max ve acos domain hatası/EDOM da sınandı.

Testte özgün oyunun hiçbir kodu çalıştırılmadı. Heap, Vita Clib memcpy/memmove/memset, errno deposu ve tek iş parçacığı için newlib reent erişimi test karşılıkları kullanır; SDK'nın gerçek matematik/parsing kodu kullanılır. Vita TLS'si, fiziksel CPU/GPU ve oyunun tüm sayısal akışları doğrulanmış değildir. libc locale/dosya/zaman/variadic ve diğer eksikler bu eklemeyle kapanmış sayılmaz.

CI, `tests/test_arm_runtime_bridges.py` ile C++ ve matematik köprülerini birlikte sınar. Bu değişiklik SELF veya VPK üretmez; native guest yürütme korumasını açmaz.
