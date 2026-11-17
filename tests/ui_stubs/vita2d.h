#ifndef VP_TEST_VITA2D_H
#define VP_TEST_VITA2D_H
#include <stddef.h>
#include <stdint.h>
typedef struct { void *surface; int w, h; } vita2d_texture;
typedef struct { int unused; } vita2d_pgf;
#define SCE_GXM_TEXTURE_FILTER_LINEAR 1
void vita2d_draw_rectangle(float,float,float,float,unsigned int);
void vita2d_draw_fill_circle(float,float,float,unsigned int);
void vita2d_draw_line(float,float,float,float,unsigned int);
void vita2d_draw_texture_tint_scale(const vita2d_texture *,float,float,float,float,unsigned int);
unsigned int vita2d_texture_get_width(const vita2d_texture *);
unsigned int vita2d_texture_get_height(const vita2d_texture *);
vita2d_texture *vita2d_load_PNG_file(const char *);
void vita2d_texture_set_filters(vita2d_texture *,int,int);
void vita2d_free_texture(vita2d_texture *);
int vita2d_pgf_text_width(vita2d_pgf *,float,const char *);
int vita2d_pgf_draw_text(vita2d_pgf *,int,int,unsigned int,float,const char *);
void vita2d_wait_rendering_done(void);
#endif
