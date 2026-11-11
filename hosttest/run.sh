#!/bin/sh
# Arranca el loader de Iron Man 2 en Linux ARM32 softfp (Docker, armel
# emulado) para probar sin consola. Ver "/Volumes/TOSHIBA EXT/PSVITA Develop/Docs/Prueba-ARM32-sin-consola.md".
#
#   hosttest/run.sh build             compila (contenedor arm64 + gcc cruzado armel)
#   hosttest/run.sh run [segundos]    arranca el juego (por defecto 300 s)
#
# Sin ventana: HOSTTEST_TAPS="seg:x:y,..." y HOSTTEST_PAD="seg:mascara_hex:duracion,..." (psp2_fake.c).
# Ventana en vivo: HOSTTEST_WINDOW=1 (XQuartz con "Permitir conexiones de clientes
# de red" y `xhost +localhost`). Teclado/ratón en window_sdl.c.
# Logs del loader y capturas (frames/*.ppm): build/hosttest-data/.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
VITAINC="${VITASDK:-$HOME/vitasdk}/arm-vita-eabi/include"
DATA="$ROOT/build/hosttest-data"
IMAGE=ironman2-hosttest-cross
RUNIMAGE=ironman2-hosttest-run
VOLUME=ironman2-hosttest-build

case "${1:-}" in
build)
  # Imágenes de Skater Nation (mismo toolchain armel; vorbis va como stub en vorbis_fake.c).
  docker tag skaternation-hosttest-cross "$IMAGE"
  docker tag skaternation-hosttest-run "$RUNIMAGE"
  docker run --rm -e HT_DEFS -v "$ROOT":/port:ro -v "$VITAINC":/vitainc:ro -v "$VOLUME":/build "$IMAGE" sh -ec '
    mkdir -p /build/psp2inc
    cp -r /vitainc/psp2 /vitainc/psp2common /vitainc/vitasdk /vitainc/kubridge.h /build/psp2inc/
    cmake -S /port/hosttest -B /build/out -DHT_DEFS="${HT_DEFS:-}" -DPSP2_INCLUDE=/build/psp2inc \
      -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=arm \
      -DCMAKE_C_COMPILER=arm-linux-gnueabi-gcc -DCMAKE_ASM_COMPILER=arm-linux-gnueabi-gcc \
      -DCMAKE_LIBRARY_ARCHITECTURE=arm-linux-gnueabi >/build/configure.log
    make -k -C /build/out -j"$(nproc)" 2>&1 | grep -E "error|undefined|Linking|Built" || true
    test -x /build/out/ironman2'
  ;;
run)
  mkdir -p "$DATA"
  WINDOW=
  if [ -n "${HOSTTEST_WINDOW:-}" ]; then WINDOW="-e DISPLAY=host.docker.internal:0"; fi
  # Datos del juego (escribibles como en la Vita: el motor escribe .\\SaiSave.log en game_app/);
  # logs/saves/frames en build/hosttest-data.
  docker run --rm --platform linux/arm/v5 $WINDOW -e HOSTTEST_FRAME_S -e HOSTTEST_TAP_S -e HOSTTEST_TAPS -e HOSTTEST_PAD \
    -e ALSOFT_DRIVERS=null -e EGL_PLATFORM=surfaceless -e LIBGL_ALWAYS_SOFTWARE=1 \
    -v "$VOLUME":/build:ro -v "$ROOT/ux0_data/ironman2":/game -v "$DATA":/data \
    "$RUNIMAGE" sh -c '
      mkdir -p /data/ironman2 /ur0:/data && touch /ur0:/data/libshacccg.suprx
      for f in /game/*; do case "${f##*/}" in logs|saves|game_app|._*) ;; *) ln -sfn "$f" /data/ironman2/ ;; esac; done
      # main.c hace chdir al bundle: las rutas "ux0:data/..." pasan a ser relativas a game_app -> enlace ahí
      rm -rf /data/ironman2/game_app && mkdir -p /data/ironman2/game_app
      for f in /game/game_app/*; do ln -sfn "$f" /data/ironman2/game_app/; done
      ln -sfn /data "/data/ironman2/game_app/ux0:data"
      ln -sfn /data /ux0:data
      ln -sfn /data "/data/ironman2/ux0:data"   # el juego hace chdir(ux0:data/ironman2) y sigue con rutas "ux0:..."
      cd / && timeout '"${2:-300}"' /build/out/ironman2; echo "salida: $?"'
  echo "logs: $DATA/ironman2/logs  capturas: $DATA/frames"
  ;;
*)
  sed -n 2,11p "$0"; exit 2 ;;
esac
