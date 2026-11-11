/* hosttest: lo que el runtime de Ferrari usa y psp2_fake.c (de Rayman 2) no tenía. Solo para hosttest. */
#include <pthread.h>
#include <semaphore.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <GLES/gl.h>

int sceClibPrintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(stderr, fmt, ap);
    va_end(ap);
    return r;
}
long long sceIoLseek(int fd, long long off, int whence) { return lseek(fd - 1, (off_t)off, whence); }
int sceSysmoduleLoadModule(unsigned id) { (void)id; return 0; }

/* mutex: id = puntero a pthread_mutex_t (recursivo, como SCE_KERNEL_MUTEX_ATTR_RECURSIVE) */
int sceKernelCreateMutex(const char *name, unsigned attr, int init, void *opt)
{
    pthread_mutex_t *m = malloc(sizeof *m);
    pthread_mutexattr_t a;
    (void)name; (void)attr; (void)opt;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(m, &a);
    while (init-- > 0) pthread_mutex_lock(m);
    return (int)((unsigned)(uintptr_t)m >> 1); /* > 0 */
}
#define MTX(id) ((pthread_mutex_t *)(uintptr_t)((unsigned)(id) << 1))
int sceKernelLockMutex(int id, int n, unsigned *timeout) { (void)timeout; while (n-- > 0) pthread_mutex_lock(MTX(id)); return 0; }
int sceKernelUnlockMutex(int id, int n) { while (n-- > 0) pthread_mutex_unlock(MTX(id)); return 0; }
int sceKernelDeleteMutex(int id) { pthread_mutex_destroy(MTX(id)); free(MTX(id)); return 0; }

extern sem_t **s_semas;
int sceKernelPollSema(int id, int need)
{
    sem_t *s = s_semas[id - 0x50000000];
    while (need-- > 0)
        if (sem_trywait(s)) return 0x800201A4; /* SCE_KERNEL_ERROR_SEMA_ZERO */
    return 0;
}

/* GL_FIXED de GLES 1.1 que el GL de escritorio de Mesa no exporta (Feanor dibuja por software) */
#define FX(decl) decl { fprintf(stderr, "[HOSTTEST] %s sin implementar\n", __func__); }
FX(void glAlphaFuncx(GLenum f, GLfixed r)) FX(void glClearDepthx(GLfixed d)) FX(void glClipPlanex(GLenum p, const GLfixed *e))
FX(void glDepthRangex(GLfixed n, GLfixed f)) FX(void glFogx(GLenum p, GLfixed v)) FX(void glFogxv(GLenum p, const GLfixed *v))
FX(void glFrustumx(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f))
FX(void glLightModelxv(GLenum p, const GLfixed *v)) FX(void glLightxv(GLenum l, GLenum p, const GLfixed *v))
FX(void glLineWidthx(GLfixed w)) FX(void glMaterialx(GLenum f, GLenum p, GLfixed v)) FX(void glMultMatrixx(const GLfixed *m))

/* sqlite3 (ios5.c, Core Data de otros juegos): sin base */
int sqlite3_open_v2(const char *f, void **db, int fl, const char *v) { (void)f; (void)fl; (void)v; *db = 0; return 1; }
#define SQ(name) int name() { return 1; }
SQ(sqlite3_close) SQ(sqlite3_prepare_v2) SQ(sqlite3_step) SQ(sqlite3_finalize) SQ(sqlite3_bind_blob) SQ(sqlite3_bind_text)
SQ(sqlite3_column_blob) SQ(sqlite3_column_bytes) SQ(sqlite3_column_count) SQ(sqlite3_column_text)
const char *sqlite3_errmsg(void *db) { (void)db; return "hosttest: sin sqlite"; }
void sqlite3_free(void *p) { free(p); }
void *sqlite3_malloc(int n) { return malloc(n); }
