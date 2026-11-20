#ifndef VP_UI_BACKDROP_H
#define VP_UI_BACKDROP_H
#include <stdbool.h>
#include <vita2d.h>
bool vp_ui_backdrop_init(void);
/* Call between the previous-frame GPU fence and start_drawing. */
void vp_ui_backdrop_prepare(const char *platform_asset,unsigned long long now);
void vp_ui_backdrop_draw(unsigned long long now,int motion_ms,unsigned int opacity,bool plain);
void vp_ui_backdrop_shutdown(void);
#endif
