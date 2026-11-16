#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>

#include "vitaplus.h"

int main(void) {
    VpApp app;
    SceCtrlData pad;

    vp_app_init(&app);
    vita2d_init();
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    for (;;) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        if (pad.buttons & SCE_CTRL_START) break;

        vita2d_start_drawing();
        vita2d_clear_screen();
        vita2d_end_drawing();
        vita2d_swap_buffers();
    }

    vita2d_fini();
    sceKernelExitProcess(0);
    return 0;
}
