/* hosttest: -Wl,--wrap=pthread_create. Los hilos del juego (darwin.c d_pthread_create, pila de 512 KB)
 * reciben el contexto EGL; los de psp2_fake (sceKernelCreateThread: audio) no. Solo para hosttest. */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

void hosttest_gl_release(void);
void hosttest_gl_acquire(void);
int __real_pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *arg);

typedef struct { void *(*fn)(void *); void *arg; } tramp;

static void *gl_tramp(void *p)
{
    tramp t = *(tramp *)p;
    free(p);
    hosttest_gl_acquire();
    return t.fn(t.arg);
}

int __wrap_pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *arg)
{
    size_t ss = 0;
    if (a) pthread_attr_getstacksize(a, &ss);
    /* solo el primer hilo del juego (render): Iron Man 2 crea después el hilo de audio Vox (0x2EDA34),
     * que se quedaba con el contexto y glGenTextures devolvía 0 en el de render */
    static int given;
    if (ss != 512 * 1024 || given++) return __real_pthread_create(t, a, fn, arg);
    tramp *p = malloc(sizeof *p);
    p->fn = fn, p->arg = arg;
    hosttest_gl_release();
    fprintf(stderr, "[HOSTTEST] contexto GL -> hilo del juego\n");
    return __real_pthread_create(t, a, gl_tramp, p);
}
