#pragma once
// Configuración propia de este juego (el resto de source/ es común a los ports iOS de la familia;
// se sincroniza con tools/sync_runtime.sh desde Ferrari-GT-Evolution-vita).
#define PORT_NAME "Iron Man 2"
#define PORT_SLUG "ironman2"
#define PORT_EXEC "IronMan2"           // CFBundleExecutable
#define PORT_ROTATION 1                // drawable vertical -> 1 landscape-right, 2 landscape-left, 0 vertical
#define PORT_STRETCH 0                 // 0 = conservar aspecto (816x544 con barras), 1 = estirar a 960x544
#define PORT_SYSTEM_VERSION "3.1.3"    // [UIDevice systemVersion] (MinimumOSVersion 3.0)
#define PORT_DEVICE_MACHINE "iPhone2,1" // hw.machine (3GS: calidad alta en Gameloft)
#define GLES_POOL_MB 24
#define PORT_ACCEL_FROM_STICK 0        // juego de acción: el stick no inclina
#define PORT_SCREEN_W 320
#define PORT_SCREEN_H 480
#define PORT_SCREEN_SCALE 1.0f
