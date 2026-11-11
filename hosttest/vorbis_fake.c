/* hosttest: la imagen armel no trae libvorbis (sin espacio en Docker para instalarla). Ningún .ogg abre:
 * prueba el camino "música ausente" de audio.c. Solo para hosttest. */
#include <vorbis/vorbisfile.h>

int ov_fopen(const char *path, OggVorbis_File *vf) { return -1; }
int ov_clear(OggVorbis_File *vf) { return 0; }
vorbis_info *ov_info(OggVorbis_File *vf, int link) { return 0; }
int ov_pcm_seek(OggVorbis_File *vf, ogg_int64_t pos) { return -1; }
ogg_int64_t ov_pcm_total(OggVorbis_File *vf, int i) { return 0; }
long ov_read(OggVorbis_File *vf, char *buf, int len, int be, int word, int sgned, int *bs) { return 0; }
