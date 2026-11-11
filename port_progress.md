# Iron Man 2 (iOS 1.0.3) → PS Vita — progreso

Base: runtime iOS común (copia maestra en `Ferrari-GT-Evolution-vita`, `tools/sync_runtime.sh`) + lo propio de
Iron Man 2 (`source/port_config.h`, `source/patch.c`). IPA: `Iron Man 2_v.1.0.3.ipa` (thin armv6, `cryptid 0`,
MinimumOSVersion 3.0). Motor: **Irrlicht 1.4.2** (GLES1) + **gameswf** (menús Flash `ironmanmenu.bin`, texturas
PVR `.t`), audio por OpenAL con música IMA ADPCM (`.www` = WAV) decodificada por el juego.

## Estado (2026-11-11)

Hosttest ARM32 (`hosttest/run.sh`, Docker armel): **llega a la pantalla de título** (logos Marvel/Gameloft →
"Touch the screen to continue"), rotación y aspecto correctos (816x544 con barras). **Falta la corrida #1 en
la consola.**

- [x] Repo `psvita-toolkit init --ipa` (IRONMAN02, slug `ironman2`), runtime de Ferrari sincronizado,
      `cxx_imports.c` generado (46 símbolos C++, 0 sin equivalente).
- [x] Imports: 0 sin implementar. Agregados al runtime (también en el maestro Ferrari, sin commit allá):
      `setlocale`, `localeconv`, `strcoll`, `tmpnam`, `system`, `CFAllocatorGetDefault`,
      `MPMediaLibraryDidChangeNotification`.
- [x] VFP short vectors (FPSCR.LEN=3): 162 bloques de `CMatrix4` de Irrlicht (47 funciones) → NEON con 9
      traducciones nuevas en `macho.c` (`vmul/vmla s24|s28` y `vadd s24,s24,s20`).
- [x] Rebase (`tools/ios_prep.py`): desplazamientos PIC (`ldr rX,[pc,#]` ... `add rY, pc, rX`) se rebaseaban
      como punteros → punteros dobles `0x30xxxxxx` en los inicializadores estáticos (0x6220, 0xEEC4). Ventana
      de uso 12→48 instrucciones, corte en `pop {pc}`, y r4–r8/r10/r11 se siguen a través de `bl`.
      71 palabras menos (33,468), todas verificadas como PC-relativas.
- [x] 297 inicializadores, `main()` → `UIApplicationMain(AppController)` → NSTimer 60 Hz `mainTimer_onUpdate`;
      hilo de juego (0x52AF8) + hilo de audio Vox (0x2EDA34).
- [x] Fallos de `fopen` siempre al log (`fopen(...) falló`).
- [x] LiveArea con arte del juego (icono del IPA, título).

## Hosttest: arreglos que NO aplican a la Vita

- `darwin.c` compilado con `time_t` de 32 bits (glibc con `_TIME_BITS=64` escribía 16 bytes en el `timeval`
  de 8 de iOS: `System::CurrentTimeMillis` → pc=0). En VitaSDK `timeval` ya es de 8 bytes.
- Contexto GL solo para el primer hilo del juego (el hilo de audio se lo robaba y `glGenTextures` devolvía 0
  → textura nula en `gameswf::bitmap_info_ogl::set_texture`). vitaGL no tiene contexto por hilo.
- PVRTC (`ironmanmenutga3/4.t`) da error en Mesa; en la Vita lo soporta GXM.
- `HOSTTEST_TAP_S`: a ~2 fps un toque de 0,5 s empieza y termina en el mismo frame.

## Datos (no se commitean)

`tools/stage_data.sh` arma `ux0_data/ironman2/` (~964 MB: bundle, rebase/binds, nib.txt, nibs/, controls.txt)
y `python3 -I tools/deploy_data.py` lo sube a `ux0:data/ironman2/`.

## Corrida #1 en consola: qué mirar

1. Arranca y muestra logos + título (si no: `ux0:data/ironman2/logs/ironman2_NNN.log` + `.psp2dmp`).
   El log debe decir `VFP vector -> NEON: 162 bloques` y `binds: 6013 slots, 0 imports sin implementar`.
2. Texturas del menú Flash (incluidas las PVRTC) se ven bien; colores correctos.
3. Tocar la pantalla pasa del título al menú; los botones del menú responden.
4. Música del menú (`m_title_menu.www`) y efectos.
5. fps (línea `PERF` del log).
6. Entrar a una misión: anotar dónde queda el joystick virtual y los botones del HUD para `controls.txt`.

## Corrida #2 en consola (menú → elegir misión)

- Llega al menú y a la selección de traje. Al confirmar, el watchdog (8 s sin present) mató el proceso:
  en el dump el hilo principal estaba ocioso en el run loop y el hilo del juego (0x52AF8) en `fopen`
  cargando la misión — no era un cuelgue. El watchdog ahora solo salta si tampoco hay aperturas de
  archivo (`darwin_io_ops`) y loguea el último archivo abierto.
- LiveArea: borrado el "Touch the screen to continue" de `bg0.png` y `pic0.png`.

## Corrida #3 en consola (ingame, nivel 1 Expo)

- Ingame a 35-40 fps estable; el tiempo es del juego (`resto (juego)` ~22 ms), no de GL.
- Tirones: Vox reabre `sfx_*.www` desde ux0 en cada disparo → quedan en RAM (16 MB en total).
- Carga en 20% (~67 s): Vox abre los ~780 `.www` dos veces cada uno; el `fopen` de newlib hace getstat
  (`__is_dir`) + `sceIoOpen` y cada búsqueda en el directorio del bundle cuesta decenas de ms (sin relación con
  el tamaño). `mf_open` (`darwin.c`): un solo `sceIoOpen`, archivos ≤256 KB leídos enteros y la reapertura
  inmediata desde RAM (`fopencookie`); los grandes van directo a `sceIo` con buffer de 64 KB. `PORT_GL_DIAG 0` (sin trazas/capturas TGA). Build `--preset release`
  (sin `l_debug` de cada fopen).
- Controles: `CAMERA x y r` nuevo (stick derecho = swipe de cámara). `controls.txt` con joystick (49,254),
  ataque (388,250) y pausa (24,26) sacados de los toques del log sobre `frame_lit_2.tga`. Falta ubicar
  misil/escudo/unibeam (captura en combate).

## Pendiente

- [ ] `controls.txt` con el joystick virtual (`STICK x y r`) y botones del HUD: falta ver el HUD en partida.
- [ ] `NSDateFormatter` (`setDateFormat:`/`stringFromDate:`) y `-[NSData initWithContentsOfURL:]` devuelven nil
      (solo los usa el envío de info de Gameloft, sin red).
- [x] `glGetIntegerv(GL_MAX_PALETTE_MATRICES_OES / GL_MAX_VERTEX_UNITS_OES)` → 9 / 3 (SGX535); antes
      GL_INVALID_ENUM. Solo en este port: subirlo al maestro cuando termine el trabajo GLES2 en curso allá.
- [ ] Subir al maestro (Ferrari) lo genérico: ya copiados allá (sin commit) imports nuevos, traducciones
      VFP→NEON, `ios_prep.py` y log de `fopen`; falta `gles.c` (paleta) y el hosttest como plantilla.
- [ ] Trofeos.
