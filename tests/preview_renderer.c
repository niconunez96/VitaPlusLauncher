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
static unsigned int loaded;
static int probe_width=-1;
static char drawn_text[VP_URL+4];
static int selected_seen, row_count, line_one, line_two;
static char expected_title[64];
void vp_ui_test_label(vita2d_pgf *font,int width,const char *value);

/* Mirror CMake's selected source files while preserving the app0 runtime path. */
static const char *asset_source(const char *path) {
    static char candidate[256];
    const char *base=path+5;
    FILE *file;
    if (strncmp(base,"assets/aurora/",14)!=0) return base;
    snprintf(candidate,sizeof(candidate),"assets/controller-tiles/%s",base+14);
    file=fopen(candidate,"rb");
    if (!file) return base;
    fclose(file); return candidate;
}

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
    static const char *expected[]={"platform_arcade","platform_dreamcast","platform_gba","platform_n3ds",
        "platform_nds","platform_gamecube","platform_psx","platform_ps2","platform_psp",
        "platform_genesis","platform_snes","platform_wii","ui_romm"};
    size_t i;
    vita2d_texture *t=calloc(1,sizeof(*t)); assert(t);
    for (i=0;i<sizeof(expected)/sizeof(expected[0]);i++) if (strstr(path,expected[i])) loaded|=1u<<i;
    t->surface=cairo_image_surface_create_from_png(asset_source(path));
    assert(cairo_surface_status(t->surface)==CAIRO_STATUS_SUCCESS);
    t->w=cairo_image_surface_get_width(t->surface); t->h=cairo_image_surface_get_height(t->surface);
    texture_count++; return t;
}
void vita2d_texture_set_filters(vita2d_texture *t,int a,int b) { (void)t;(void)a;(void)b; }
void vita2d_free_texture(vita2d_texture *t) { cairo_surface_destroy(t->surface); free(t); texture_count--; }
void vita2d_wait_rendering_done(void) {}
SceUID sceIoOpen(const char *path,int flags,int mode) {
    (void)mode; return missing?-1:open(asset_source(path),flags);
}
unsigned long long sceKernelGetProcessTimeWide(void) { clock_us+=16667; return clock_us; }
int vita2d_pgf_text_width(vita2d_pgf *f,float scale,const char *s) {
    cairo_text_extents_t e; (void)f;
    cairo_set_font_size(cr,24*scale); cairo_text_extents(cr,s,&e);
    assert(cairo_status(cr)==CAIRO_STATUS_SUCCESS);
    return (int)ceil(e.x_advance);
}
int vita2d_pgf_draw_text(vita2d_pgf *f,int x,int y,unsigned int c,float scale,const char *s) {
    int width=vita2d_pgf_text_width(f,scale,s);
    assert(scale>=.75f);
    if (strcmp(s,expected_title)==0) selected_seen++;
    if (x==120 && (y==169 || y==233 || y==297 || y==361)) row_count++;
    if (x==124 && y==178 && strlen(s)>0) line_one++;
    if (x==124 && y==202 && strlen(s)>0) line_two++;
    assert(probe_width<0 || width<=probe_width);
    snprintf(drawn_text,sizeof(drawn_text),"%s",s);
    assert(x>=0 && y>=0 && x+width<=960 && y<=544);
    source(c); cairo_move_to(cr,x,y); cairo_show_text(cr,s);
    assert(cairo_status(cr)==CAIRO_STATUS_SUCCESS); return width;
}

int main(int argc,char **argv) {
    static VpApp app, before;
    static VpDownloadQueue queue;
    static VpPlatformGroup groups[64];
    static size_t visible[12];
    vita2d_pgf font={0};
    cairo_surface_t *canvas=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,960,544);
    VpUiFrame frame={0};
    const char *slugs[]={"arcade","dreamcast","gba","3ds","nds","gamecube","ps1","ps2","psp","genesis","snes","wii"};
    const char *names[]={"Arcade","Dreamcast","Game Boy Advance","Nintendo 3DS","Nintendo DS","GameCube","PlayStation","PlayStation 2","PlayStation Portable","Sega Genesis","Super Nintendo","Nintendo Wii"};
    const char *cases[]={"platforms","grid","list","carousel","settings","themes","romm","downloads","store","empty","last-page"};
    size_t c,i;
    cr=cairo_create(canvas); cairo_select_font_face(cr,"sans-serif",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_NORMAL);
    {
        char long_text[VP_URL*2+32];
        memset(long_text,'A',sizeof(long_text)-1); long_text[sizeof(long_text)-1]='\0';
        probe_width=900; vp_ui_test_label(&font,probe_width,long_text);
        assert(strlen(drawn_text)>=3 && strcmp(drawn_text+strlen(drawn_text)-3,"...")==0);
        for (i=0;i+2<sizeof(long_text);i+=2) { long_text[i]=(char)0xc3; long_text[i+1]=(char)0xa9; }
        long_text[i]='\0'; vp_ui_test_label(&font,probe_width,long_text);
        assert(strlen(drawn_text)>=3 && strcmp(drawn_text+strlen(drawn_text)-3,"...")==0);
        probe_width=1; vp_ui_test_label(&font,probe_width,"Text does not fit");
        assert(drawn_text[0]=='\0');
        vp_ui_test_label(&font,probe_width,""); assert(drawn_text[0]=='\0');
        probe_width=-1;
        {
            char long_utf8[VP_URL*2+32];
            const char *glyphs[]={"日","🎮"};
            size_t a;
            for (a=0;a<2;a++) {
                size_t used=0, bytes=strlen(glyphs[a]);
                while (used+bytes<sizeof(long_utf8)) { memcpy(long_utf8+used,glyphs[a],bytes); used+=bytes; }
                long_utf8[used]='\0';
                probe_width=108; vp_ui_test_label(&font,probe_width,long_utf8);
                assert(strlen(drawn_text)>=3 && strcmp(drawn_text+strlen(drawn_text)-3,"...")==0);
            }
            probe_width=-1;
        }
    }
    vp_app_init(&app);
    assert(vp_theme_load(&app.theme,"name=VitaPlus Aurora\nbackground=#101624\nsurface=#222b3d\naccent=#87ddc4\ntext=#f1f3fa\nmuted=#a8b5c9\n"));
    for (i=0;i<12;i++) {
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
        snprintf(groups[i].platform,sizeof(groups[i].platform),"%s",slugs[i%12]);
        snprintf(groups[i].display,sizeof(groups[i].display),"%s",names[i%12]); groups[i].count=24+i;
    }
    queue.count=6;
    for (i=0;i<queue.count;i++) {
        queue.entries[i].item=app.catalog.items[i]; queue.entries[i].state=(VpDownloadState)(i%5);
        queue.entries[i].percent=(unsigned)i*20;
        snprintf(queue.entries[i].error,sizeof(queue.entries[i].error),"Sin conexion al servidor. X reintenta.");
    }
    frame.app=&app; frame.font=&font; frame.groups=groups; frame.group_count=12;
    frame.visible=visible; frame.visible_count=12; frame.downloads=&queue;
    frame.active_theme="default"; frame.romm_url="https://romm.example.test"; frame.pair_code="";
    for (c=0;c<sizeof(cases)/sizeof(cases[0]);c++) {
        char path[1024];
        app.active_screen=VP_SCREEN_LIBRARY; app.library_level=VP_LEVEL_PLATFORMS; app.selected_platform=0;
        frame.settings_detail=false; frame.settings_section=0; frame.group_count=12;
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
    {
        static VpDownloadQueue queue_before;
        static VpPlatformGroup groups_before[64];
        size_t visible_before[12], selection, view;
        const char *samples[]={"", "é", "日本語", "🎮", "é 日本語 🎮", "a very long title with spaces and unicode 日本語 🎮"};
        int widths[]={0,1,10,20,40,108,900};
        size_t a,b;
        for (a=0;a<sizeof(samples)/sizeof(samples[0]);a++)
            for (b=0;b<sizeof(widths)/sizeof(widths[0]);b++) {
                probe_width=widths[b]; vp_ui_test_label(&font,probe_width,samples[a]);
            }
        probe_width=-1;
        for (a=0;a<12;a++) {
            snprintf(app.catalog.items[a].title,sizeof(app.catalog.items[a].title),"Item%02u",(unsigned)a);
            queue.entries[a%6].item=app.catalog.items[a];
        }
        for (selection=0;selection<12;selection++) for (view=0;view<3;view++) {
            app.active_screen=VP_SCREEN_LIBRARY; app.library_level=VP_LEVEL_ROMS;
            app.active_view=(VpView)view; app.selected_item=selection;
            snprintf(expected_title,sizeof(expected_title),"Item%02u",(unsigned)selection);
            selected_seen=0; before=app; queue_before=queue;
            memcpy(groups_before,groups,sizeof(groups)); memcpy(visible_before,visible,sizeof(visible));
            vp_ui_render(&frame); assert(selected_seen>=2);
            assert(memcmp(&before,&app,sizeof(app))==0);
            assert(memcmp(&queue_before,&queue,sizeof(queue))==0);
            assert(memcmp(groups_before,groups,sizeof(groups))==0);
            assert(memcmp(visible_before,visible,sizeof(visible))==0);
        }
        app.active_screen=VP_SCREEN_SETTINGS; frame.settings_detail=true; frame.settings_section=2;
        for (selection=0;selection<queue.count;selection++) {
            frame.download_selection=selection;
            snprintf(expected_title,sizeof(expected_title),"%.63s",queue.entries[selection].item.title);
            selected_seen=0; row_count=0; queue_before=queue; before=app;
            vp_ui_render(&frame);
            assert(selected_seen==1); assert(row_count==(selection<4?4:2));
            assert(memcmp(&before,&app,sizeof(app))==0); assert(memcmp(&queue_before,&queue,sizeof(queue))==0);
        }
        frame.settings_detail=false; app.active_screen=VP_SCREEN_LIBRARY; app.library_level=VP_LEVEL_PLATFORMS;
        app.selected_platform=0; frame.group_count=1;
        snprintf(groups[0].display,sizeof(groups[0].display),"Alpha Beta Gamma Delta 日本語 🎮");
        line_one=line_two=0; vp_ui_render(&frame); assert(line_one==1 && line_two==1);
        queue.count=0; app.active_screen=VP_SCREEN_SETTINGS; frame.settings_detail=true; frame.settings_section=2;
        vp_ui_render(&frame);
        puts("independent public-frame probes: PASS (36 ROM selections/views, 6 queue selections, empty queue, UTF-8 width matrix, snapshot immutability)");
    }
    assert(loaded==((1u<<13)-1));
    vp_ui_shutdown(); assert(texture_count==0);
    missing=1; frame.group_count=12; app.selected_platform=0;
    for (i=0;i<40;i++) vp_ui_render(&frame);
    assert(texture_count==0);
    vp_ui_shutdown(); cairo_destroy(cr); cairo_surface_destroy(canvas);
    puts("renderer smoke tests: PASS (11 states, 12 platforms, RomM, readable text, UTF-8, immutable model, missing assets, cleanup)");
    return 0;
}
