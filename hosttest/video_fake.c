/* hosttest: sin FFmpeg armel. video_play no reproduce: prueba el camino "el video terminó" de uikit.c. */
#include "../source/log.h"
void video_init(void) {}
void video_play(const char *name) { l_info("hosttest: video %s salteado", name); }
