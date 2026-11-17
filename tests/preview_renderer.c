/* Headless layout smoke test. Cairo approximates PGF text; this is not Vita GPU proof. */
#include "ui_renderer.h"
#include <psp2/io/fcntl.h>
#include <cairo.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cairo_t *cr;
static unsigned long long clock_us=1000000;
static int missing;
static size_t texture_count;

static void source(unsigned int c) {
    cairo_set_source_rgba(cr,(c&255)/255.,((c>>8)&255)/255.,((c>>16)&255)/255.,(c>>24)/255.);
}
void vita2d_draw_rectangle(float x,float y,float w,float h,unsigned int c) {
    assert(isfinite(x) && isfinite(y) && w>=0 && h>=0);
    source(c); cairo_rectangle(cr,x,y,w,h); cairo_fill(cr);
}
void vita2d_draw_fill_circle(float x,float y,float r,unsigned int c) {
    assert(r>=0); source(c); cairo_arc(cr,x,y,r,0,6.283185307); cairo_fill(cr);
}
void vita2d_draw_line(float x,float y,float xx,float yy,unsigned int c) {
    source(c); cairo_set_line_width(cr,1); cairo_move_to(cr,x,y); cairo_line_to(cr,xx,yy); cairo_stroke(cr);
}
void vita2d_draw_texture_tint_scale(const vita2d_texture *t,float x,float y,float sx,float sy,unsigned int tint) {
    assert(t && t->surface && sx>0 && sy>0);
    cairo_save(cr); cairo_translate(cr,x,y); cairo_scale(cr,sx,sy);
    cairo_set_source_surface(cr,t->surface,0,0);
    cairo_paint_with_alpha(cr,(tint>>24)/255.); cairo_restore(cr);
}
unsigned int vita2d_texture_get_width(const vita2d_texture *t) { return (unsigned)t->w; }
unsigned int vita2d_texture_get_height(const vita2d_texture *t) { return (unsigned)t->h; }
vita2d_texture *vita2d_load_PNG_file(const char *path) {
    vita2d_texture *t=calloc(1,sizeof(*t)); assert(t);
    t->surface=cairo_image_surface_create_from_png(path+5);
    assert(cairo_surface_status(t->surface)==CAIRO_STATUS_SUCCESS);
    t->w=cairo_image_surface_get_width(t->surface); t->h=cairo_image_surface_get_height(t->surface);
    texture_count++; return t;
}
void vita2d_texture_set_filters(vita2d_texture *t,int a,int b) { (void)t;(void)a;(void)b; }
void vita2d_free_texture(vita2d_texture *t) { cairo_surface_destroy(t->surface); free(t); texture_count--; }
void vita2d_wait_rendering_done(void) {}
SceUID sceIoOpen(const char *path,int flags,int mode) {
    (void)mode; return missing?-1:open(path+5,flags);
}
unsigned long long sceKernelGetProcessTimeWide(void) { clock_us+=16667; return clock_us; }
int vita2d_pgf_text_width(vita2d_pgf *f,float scale,const char *s) {
    cairo_text_extents_t e; (void)f;
    cairo_set_font_size(cr,24*scale); cairo_text_extents(cr,s,&e); return (int)ceil(e.x_advance);
}
int vita2d_pgf_draw_text(vita2d_pgf *f,int x,int y,unsigned int c,float scale,const char *s) {
    int width=vita2d_pgf_text_width(f,scale,s);
    assert(x>=0 && y>=0 && x+width<=960 && y<=544);
    source(c); cairo_move_to(cr,x,y); cairo_show_text(cr,s);
    assert(cairo_status(cr)==CAIRO_STATUS_SUCCESS); return width;
}

int main(int argc,char **argv) {
    static VpApp app, before;
    static VpDownloadQueue queue;
    static VpPlatformGroup groups[64];
    static size_t visible[9];
    vita2d_pgf font={0};
    cairo_surface_t *canvas=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,960,544);
    VpUiFrame frame={0};
    const char *slugs[]={"snes","gba","psx","psp","nds","nes","genesis","PS Vita","unknown"};
    const char *names[]={"Super Nintendo","Game Boy Advance","PlayStation","PlayStation Portable","Nintendo DS","Nintendo NES","Mega Drive","PS Vita","Otra plataforma"};
    const char *cases[]={"platforms","grid","list","carousel","settings","themes","romm","downloads","store","empty","last-page"};
    size_t c,i;
    cr=cairo_create(canvas); cairo_select_font_face(cr,"sans-serif",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_NORMAL);
    vp_app_init(&app);
    assert(vp_theme_load(&app.theme,"name=VitaPlus Aurora\nbackground=#101624\nsurface=#222b3d\naccent=#87ddc4\ntext=#f1f3fa\nmuted=#a8b5c9\n"));
    for (i=0;i<9;i++) {
        VpItem item={0};
        snprintf(item.id,sizeof(item.id),"romm:%u",(unsigned)i);
        snprintf(item.title,sizeof(item.title),"%s",i==0?"Chrono Trigger":(i==1?"Super Mario Advance":"Un juego con titulo largo y acentos: aventura fantástica 日本語"));
        snprintf(item.platform,sizeof(item.platform),"%s",slugs[i]);
        snprintf(item.platform_display,sizeof(item.platform_display),"%s",names[i]);
        snprintf(item.description,sizeof(item.description),"Una aventura que te espera. Explora mundos y descubre nuevos caminos.");
        item.installed=i%2==0; item.favorite=i==0; vp_catalog_add(&app.catalog,&item);
        visible[i]=i;
    }
    for (i=0;i<64;i++) {
        snprintf(groups[i].platform,sizeof(groups[i].platform),"%s",slugs[i%9]);
        snprintf(groups[i].display,sizeof(groups[i].display),"%s",names[i%9]); groups[i].count=24+i;
    }
    queue.count=6;
    for (i=0;i<queue.count;i++) {
        queue.entries[i].item=app.catalog.items[i]; queue.entries[i].state=(VpDownloadState)(i%5);
        queue.entries[i].percent=(unsigned)i*20;
        snprintf(queue.entries[i].error,sizeof(queue.entries[i].error),"Sin conexion al servidor. X reintenta.");
    }
    frame.app=&app; frame.font=&font; frame.groups=groups; frame.group_count=9;
    frame.visible=visible; frame.visible_count=9; frame.downloads=&queue;
    frame.active_theme="default"; frame.romm_url="https://romm.example.test"; frame.pair_code="";
    for (c=0;c<sizeof(cases)/sizeof(cases[0]);c++) {
        char path[1024];
        app.active_screen=VP_SCREEN_LIBRARY; app.library_level=VP_LEVEL_PLATFORMS; app.selected_platform=0;
        frame.settings_detail=false; frame.settings_section=0; frame.group_count=9;
        if (c>=1 && c<=3) { app.library_level=VP_LEVEL_ROMS; app.active_view=(VpView)(c-1); }
        if (c>=4 && c<=7) {
            app.active_screen=VP_SCREEN_SETTINGS; frame.settings_detail=c!=4;
            frame.settings_section=c==5?1:(c==7?2:0);
        }
        if (c==8) app.active_screen=VP_SCREEN_STORES;
        if (c==9) frame.group_count=0;
        if (c==10) { frame.group_count=64; app.selected_platform=63; }
        before=app;
        for (i=0;i<40;i++) vp_ui_render(&frame);
        assert(memcmp(&app,&before,sizeof(app))==0);
        if (argc>1) {
            snprintf(path,sizeof(path),"%s/%s.png",argv[1],cases[c]);
            assert(cairo_surface_write_to_png(canvas,path)==CAIRO_STATUS_SUCCESS);
        }
    }
    vp_ui_shutdown(); assert(texture_count==0);
    missing=1; frame.group_count=9; app.selected_platform=0;
    for (i=0;i<40;i++) vp_ui_render(&frame);
    assert(texture_count==0);
    vp_ui_shutdown(); cairo_destroy(cr); cairo_surface_destroy(canvas);
    puts("renderer smoke tests: PASS (11 screen states, immutable model, missing assets, texture cleanup)");
    return 0;
}
