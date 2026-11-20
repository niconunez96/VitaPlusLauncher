#ifndef VITASHELF_LAUNCHER_VITA_THREAD_PRIORITY_H
#define VITASHELF_LAUNCHER_VITA_THREAD_PRIORITY_H

#include <psp2/kernel/threadmgr.h>

/* Vita priorities are inverted: the lower the number, the more CPU time the
 * thread gets. These values follow the VitaSDL scale (normal 160, low 191) so
 * a ROM download or a PNG decode can never preempt the renderer or the audio
 * port on the console's general-purpose core. */
#define VP_VITA_THREAD_PRIORITY_AUDIO 64
#define VP_VITA_THREAD_PRIORITY_BACKGROUND 191

/* Set the priority from inside the thread: the SceUID behind a pthread_t is
 * not part of a documented public contract, but the current thread id is. */
static inline void vp_vita_thread_set_priority(int priority) {
    (void)sceKernelChangeThreadPriority(sceKernelGetThreadId(), priority);
}

#endif
