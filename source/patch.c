// Parches de este juego. Cada parche se agrega con evidencia (log/dump + desensamblado con
// tools/annotate.py) y su dirección del Mach-O.
#include "imports.h"
#include "log.h"
#include "macho.h"
#include "patch.h"
#include "port_config.h"

const import_entry patch_imports[] = {
    {"", 0, 0},
};
TABLE_END(patch)

void patch_init(void) {
    // Logo de Gameloft en el menú (QA 12/11/2026: el juego se cerraba). MenuBase::MyFSCommand atiende
    // "LaunchIGP" -> Game::SetRestoreIGP(1) y "LaunchGLLive" -> Game::SetGll(1), que abren el IGP / Gameloft
    // Live (UIWebView + servidores de gameloft.com ya muertos). Únicos llamadores: NOP a los dos bl.
    static const uint32_t nop = 0xe1a00000; // mov r0, r0
    if (macho_read_text(0x12e108) == 0xebfda913 && macho_read_text(0x12e134) == 0xebfda738) {
        macho_patch_text(0x12e108, &nop, 4);
        macho_patch_text(0x12e134, &nop, 4);
    } else {
        l_error("patch: MyFSCommand no coincide, IGP/GLLive sin parchear");
    }
    // Game::DoFrame (0x998f4): espera activa `while (now - last <= 24) now = CurrentTimeMillis();` -> 40 fps
    // fijos (PERF de la QA: 25,0 ms casi siempre). El delta medido se pasa a FrameUpdate, así que la lógica va
    // por tiempo y no por frame. Se baja el umbral a PORT_MIN_FRAME_MS - 1.
    uint32_t cmp = 0xe3550000 | (PORT_MIN_FRAME_MS - 1); // cmp r5, #imm
    if (macho_read_text(0x998f4) == 0xe3550018)
        macho_patch_text(0x998f4, &cmp, 4);
    else
        l_error("patch: DoFrame no coincide, límite de 40 fps sin tocar");
}
