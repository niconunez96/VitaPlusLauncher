#include "ui_renderer.h"
#include "ui_presentation.h"
#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define C(r,g,b,a) ((unsigned int)(r) | ((unsigned int)(g)<<8) | ((unsigned int)(b)<<16) | ((unsigned int)(a)<<24))
typedef struct { const char *name; int w, h; vita2d_texture *texture; bool attempted; } Asset;
static Asset assets[] = {
    {"wallpaper",480,272,NULL,false}, {"emblem",128,128,NULL,false},
    {"platform_nes",128,128,NULL,false}, {"platform_famicom",128,128,NULL,false},
    {"platform_snes",128,128,NULL,false}, {"platform_gameboy",128,128,NULL,false},
    {"platform_gba",128,128,NULL,false}, {"platform_nds",128,128,NULL,false},
    {"platform_genesis",128,128,NULL,false}, {"platform_mastersystem",128,128,NULL,false},
    {"platform_gamegear",128,128,NULL,false}, {"platform_psx",128,128,NULL,false},
    {"platform_psp",128,128,NULL,false}, {"platform_vita",128,128,NULL,false},
    {"platform_arcade",128,128,NULL,false}, {"platform_pce",128,128,NULL,false},
    {"platform_atari2600",128,128,NULL,false}, {"platform_wonderswan",128,128,NULL,false},
    {"platform_homebrew",128,128,NULL,false}, {"platform_generic",128,128,NULL,false},
    {"platform_dreamcast",128,128,NULL,false}, {"platform_n3ds",128,128,NULL,false},
    {"platform_gamecube",128,128,NULL,false}, {"platform_ps2",128,128,NULL,false},
    {"platform_wii",128,128,NULL,false}, {"ui_romm",64,64,NULL,false},
    {"ui_library",64,64,NULL,false}, {"ui_home",64,64,NULL,false},
    {"ui_store",64,64,NULL,false}, {"ui_settings",64,64,NULL,false},
    {"ui_download",64,64,NULL,false}, {"ui_check",64,64,NULL,false},
    {"ui_star",64,64,NULL,false}, {"ui_search",64,64,NULL,false},
    {"ui_play",64,64,NULL,false}, {"ui_back",64,64,NULL,false},
    {"ui_grid",64,64,NULL,false}, {"ui_list",64,64,NULL,false},
    {"ui_carousel",64,64,NULL,false}, {"ui_sync",64,64,NULL,false},
    {"ui_key",64,64,NULL,false}, {"ui_folder",64,64,NULL,false}
};
static unsigned int bg, surface, ink, muted, accent;
static int asset_budget;
static float entrance;
static float dock_x = 278;
static unsigned long long last_frame;
static unsigned long long focus_at;
static char focus_key[VP_TEXT + 64];
static char last_notice[VP_TEXT];
static unsigned long long notice_at;
static VpUiAppearance appearance={180,16,130};

void vp_ui_load_theme(const char *ini) { vp_ui_appearance_load(&appearance,ini); }

static unsigned int alpha(unsigned int c, unsigned int a) { return (c & 0xffffffu) | (a << 24); }
static unsigned int rgb(uint32_t c) { return C((c>>16)&255,(c>>8)&255,c&255,255); }
static unsigned int mix(unsigned int a, unsigned int b, float t) {
    unsigned int out = 0;
    int i;
    for (i=0;i<3;i++) {
        float av = (float)((a>>(8*i))&255), bv = (float)((b>>(8*i))&255);
        out |= (unsigned int)(av+(bv-av)*t)<<(8*i);
    }
    return out | 0xff000000u;
}
static unsigned int color(const char *s, unsigned int fallback) {
    unsigned int r,g,b;
    return sscanf(s,"#%02x%02x%02x",&r,&g,&b)==3 ? C(r,g,b,255) : fallback;
}

static void round_rect(float x,float y,float w,float h,float r,unsigned int c) {
    vita2d_draw_rectangle(x+r,y,w-2*r,h,c);
    vita2d_draw_rectangle(x,y+r,r,h-2*r,c);
    vita2d_draw_rectangle(x+w-r,y+r,r,h-2*r,c);
    vita2d_draw_fill_circle(x+r,y+r,r,c);
    vita2d_draw_fill_circle(x+w-r,y+r,r,c);
    vita2d_draw_fill_circle(x+r,y+h-r,r,c);
    vita2d_draw_fill_circle(x+w-r,y+h-r,r,c);
}

static vita2d_texture *asset(const char *name) {
    size_t i;
    for (i=0;i<sizeof(assets)/sizeof(assets[0]);i++) {
        Asset *a=&assets[i];
        if (strcmp(a->name,name)!=0) continue;
        if (!a->attempted && asset_budget>0) {
            char path[128];
            unsigned char header[29];
            SceUID fd;
            a->attempted=true; asset_budget--;
            (void)snprintf(path,sizeof(path),"app0:assets/aurora/%s.png",a->name);
            fd=sceIoOpen(path,SCE_O_RDONLY,0);
            if (fd>=0) {
                int read_count=sceIoRead(fd,header,sizeof(header));
                SceOff length=sceIoLseek(fd,0,SCE_SEEK_END);
                sceIoClose(fd);
                if (read_count==(int)sizeof(header) && length>0 && length<=512*1024 &&
                    vp_safe_cover_png(header,sizeof(header)) &&
                    ((unsigned int)header[18]*256+header[19])==(unsigned int)a->w &&
                    ((unsigned int)header[22]*256+header[23])==(unsigned int)a->h) {
                    a->texture=vita2d_load_PNG_file(path);
                    if (a->texture) vita2d_texture_set_filters(a->texture,SCE_GXM_TEXTURE_FILTER_LINEAR,SCE_GXM_TEXTURE_FILTER_LINEAR);
                }
            }
        }
        return a->texture;
    }
    return NULL;
}

static void fit(vita2d_texture *t,float x,float y,float w,float h,unsigned int tint) {
    float tw,th,s;
    if (!t) return;
    tw=(float)vita2d_texture_get_width(t); th=(float)vita2d_texture_get_height(t);
    if (tw<=0 || th<=0) return;
    s=fminf(w/tw,h/th);
    vita2d_draw_texture_tint_scale(t,x+(w-tw*s)/2,y+(h-th*s)/2,s,s,tint);
}
static void icon(const char *name,float x,float y,float size) {
    vita2d_texture *t=asset(name);
    if (t) fit(t,x,y,size,size,0xffffffffu);
    else {
        round_rect(x+size*.2f,y+size*.2f,size*.6f,size*.6f,size*.15f,alpha(accent,100));
        vita2d_draw_fill_circle(x+size*.5f,y+size*.5f,size*.09f,ink);
    }
}

/* Width-based UTF-8 truncation, rather than cutting arbitrary bytes mid-glyph. */
static void label(vita2d_pgf *font,int x,int y,int width,float scale,unsigned int c,const char *value) {
    char text[VP_URL+4];
    size_t length,n;
    if (width<=0 || value==NULL) return;
    /* Keep small PGF labels readable; truncate at the final drawing scale. */
    scale=fmaxf(scale,.75f);
    length=strlen(value); n=length;
    if (n>sizeof(text)-4) n=sizeof(text)-4;
    while (n>0 && ((unsigned char)value[n]&0xc0)==0x80) --n;
    memcpy(text,value,n); text[n]='\0';
    if (n<length || vita2d_pgf_text_width(font,scale,text)>width) {
        for (;;) {
            memcpy(text+n,"...",4);
            if (vita2d_pgf_text_width(font,scale,text)<=width) break;
            if (n==0) { text[0]='\0'; break; }
            do { --n; } while (n>0 && ((unsigned char)text[n]&0xc0)==0x80);
        }
    }
    vita2d_pgf_draw_text(font,x,y,c,scale,text);
}

static void two_line_label(vita2d_pgf *font,int x,int y,int width,unsigned int c,const char *value) {
    char line[VP_URL+4];
    const char *p=value,*split=NULL;
    if (vita2d_pgf_text_width(font,.75f,value)<=width) { label(font,x,y,width,.75f,c,value); return; }
    while ((p=strchr(p,' '))!=NULL) {
        size_t n=(size_t)(p-value);
        if (n>=sizeof(line)) break;
        memcpy(line,value,n); line[n]='\0';
        if (vita2d_pgf_text_width(font,.75f,line)>width) break;
        split=p++;
    }
    if (!split) { label(font,x,y,width,.75f,c,value); return; }
    memcpy(line,value,(size_t)(split-value)); line[split-value]='\0';
    label(font,x,y,width,.75f,c,line);
    while (*split==' ') ++split;
    label(font,x,y+24,width,.75f,c,split);
}

#ifdef VP_UI_TESTING
/* Probe the private renderer text boundary without exporting a production API. */
void vp_ui_test_label(vita2d_pgf *font,int width,const char *value) {
    label(font,0,24,width,.45f,0xffffffffu,value);
}
#endif

static void card(float x,float y,float w,float h,bool selected,unsigned int tint) {
    int lift=selected ? (int)(4*entrance) : 0;
    round_rect(x,y+5,w,h,17, C(0,0,0,60));
    if (selected) round_rect(x-3,y-lift-3,w+6,h+6,(float)appearance.radius+3,alpha(tint,190));
    round_rect(x,y-lift,w,h,(float)appearance.radius,selected ? mix(surface,tint,.16f) : mix(surface,bg,.12f));
    vita2d_draw_line(x+18,y-lift+1,x+w-18,y-lift+1,alpha(ink,selected?90:24));
}

static void backdrop(vita2d_texture *cover) {
    vita2d_texture *wall=asset("wallpaper");
    int y;
    vita2d_draw_rectangle(0,0,960,544,bg);
    if (wall) vita2d_draw_texture_tint_scale(wall,0,0,2,2,C(160,170,195,appearance.wallpaper_alpha));
    if (cover) {
        float tw=(float)vita2d_texture_get_width(cover), th=(float)vita2d_texture_get_height(cover);
        float s=fmaxf(960/tw,544/th);
        vita2d_draw_texture_tint_scale(cover,(960-tw*s)/2+6*(1-entrance),(544-th*s)/2,s,s,C(120,135,160,60));
    }
    for (y=0;y<544;y+=16) {
        unsigned int opacity=(unsigned int)(y<140 ? 95-y/3 : 45+(y-140)/3);
        vita2d_draw_rectangle(0,y,960,16,alpha(bg,opacity));
    }
}

static size_t position(const VpUiFrame *f) {
    size_t i;
    for (i=0;i<f->visible_count;i++) if (f->visible[i]==f->app->selected_item) return i;
    return 0;
}
static void pages(const VpUiFrame *f,size_t selected,size_t total,size_t page_size) {
    char s[48];
    if (!total) return;
    (void)snprintf(s,sizeof(s),"%u / %u",(unsigned)(selected/page_size+1),(unsigned)((total+page_size-1)/page_size));
    label(f->font,848,132,84,.52f,muted,s);
}

static void platforms(const VpUiFrame *f) {
    size_t start=(f->app->selected_platform/8)*8,i;
    for (i=start;i<f->group_count && i<start+8;i++) {
        const VpPlatformGroup *g=&f->groups[i];
        const VpUiPlatform *p=vp_ui_platform(g->platform);
        VpUiRect r=vp_ui_tile(i-start);
        bool selected=i==f->app->selected_platform;
        unsigned int tint=rgb(p->rgb);
        char count[32];
        card(r.x,r.y,r.w,r.h,selected,tint);
        round_rect(r.x+12,r.y+12,74,78,14,mix(surface,tint,.23f));
        icon(p->asset,r.x+8,r.y+9-(selected?(int)(3*entrance):0),84);
        two_line_label(f->font,r.x+96,r.y+32,108,ink,g->display);
        (void)snprintf(count,sizeof(count),"%u juegos",(unsigned)g->count);
        label(f->font,r.x+96,r.y+88,108,.75f,muted,count);
        round_rect(r.x+98,r.y+106,selected?54:22,3,1,tint);
    }
    pages(f,f->app->selected_platform,f->group_count,8);
}

static void badge(int x,int y,const VpItem *item) {
    round_rect(x,y,24,24,8,alpha(bg,235));
    icon(item->installed?"ui_check":"ui_download",x+1,y+1,22);
}
static void placeholder(const VpItem *item,float x,float y,float w,float h) {
    round_rect(x,y,w,h,10,mix(surface,accent,.1f));
    icon(vp_ui_platform(item->platform)->asset,x+(w-60)/2,y+(h-60)/2,60);
}

static void roms(const VpUiFrame *f) {
    size_t pos=position(f),i,start;
    if (f->app->active_view==VP_VIEW_GRID) {
        start=(pos/8)*8;
        for (i=start;i<f->visible_count && i<start+8;i++) {
            const VpItem *item=&f->app->catalog.items[f->visible[i]];
            VpUiRect r=vp_ui_tile(i-start);
            card(r.x,r.y,r.w,r.h,f->visible[i]==f->app->selected_item,accent);
            if (f->covers[i-start]) fit(f->covers[i-start],r.x+8,r.y+8,74,104,0xffffffffu);
            else placeholder(item,r.x+10,r.y+12,70,96);
            two_line_label(f->font,r.x+92,r.y+30,110,ink,item->title);
            label(f->font,r.x+92,r.y+80,110,.75f,muted,item->platform_display);
            badge(r.x+92,r.y+94,item);
            if (item->favorite) icon("ui_star",r.x+166,r.y+94,25);
        }
        pages(f,pos,f->visible_count,8);
    } else if (f->app->active_view==VP_VIEW_LIST) {
        start=(pos/6)*6;
        for (i=start;i<f->visible_count && i<start+6;i++) {
            const VpItem *item=&f->app->catalog.items[f->visible[i]];
            int y=146+(int)(i-start)*43;
            bool selected=f->visible[i]==f->app->selected_item;
            round_rect(28,y,904,39,11,selected?mix(surface,accent,.2f):alpha(surface,210));
            if (selected) round_rect(28,y+8,4,23,2,accent);
            badge(39,y+7,item);
            label(f->font,77,y+25,620,.65f,selected?accent:ink,item->title);
            label(f->font,718,y+25,172,.51f,muted,item->platform_display);
            if (item->favorite) icon("ui_star",896,y+7,24);
        }
        pages(f,pos,f->visible_count,6);
    } else {
        /* f->covers is explicitly previous/selected/next, including wraparound. */
        size_t indexes[3]={(pos+f->visible_count-1)%f->visible_count,pos,(pos+1)%f->visible_count};
        for (i=0;i<3;i++) {
            const VpItem *item=&f->app->catalog.items[f->visible[indexes[i]]];
            int x=i==0?116:(i==1?365:654),y=i==1?142:173,w=i==1?230:190,h=i==1?264:204;
            if (f->visible_count==1 && i!=1) continue;
            card(x,y,w,h,i==1,accent);
            if (f->covers[i]) fit(f->covers[i],x+12,y+10,w-24,h-46,i==1?0xffffffffu:C(180,185,200,230));
            else placeholder(item,x+12,y+10,w-24,h-46);
            label(f->font,x+14,y+h-16,w-28,.6f,ink,item->title);
            badge(x+w-35,y+10,item);
        }
        pages(f,pos,f->visible_count,1);
    }
}

static const char *download_label(VpDownloadState state) {
    switch (state) {
        case VP_DOWNLOAD_PENDING:return "En cola";
        case VP_DOWNLOAD_DOWNLOADING:return "Descargando";
        case VP_DOWNLOAD_COMPLETED:return "Completada";
        case VP_DOWNLOAD_FAILED:return "Fallida";
        case VP_DOWNLOAD_CANCELED:return "Cancelada";
    }
    return "Desconocido";
}
static void empty(const VpUiFrame *f,const char *asset_name,const char *title,const char *message) {
    card(166,160,628,238,false,accent);
    icon(asset_name,428,176,104);
    label(f->font,204,315,552,.94f,ink,title);
    label(f->font,204,350,552,.62f,muted,message);
}

static void settings(const VpUiFrame *f) {
    static const char *names[]={"RomM","Temas","Descargas"};
    static const char *icons[]={"ui_romm","ui_settings","ui_download"};
    int i;
    if (!f->settings_detail) {
        static const char *descriptions[]={"Conecta tu biblioteca y sincroniza caratulas","Tu espacio, tus colores","Progreso, errores y reintentos"};
        for (i=0;i<3;i++) {
            int y=149+i*84;
            card(96,y,768,72,f->settings_section==i,accent);
            icon(icons[i],112,y+7,54);
            label(f->font,186,y+28,600,.8f,ink,names[i]);
            label(f->font,186,y+53,600,.56f,muted,descriptions[i]);
        }
        return;
    }
    if (f->settings_section==1) {
        card(78,150,804,248,false,accent);
        icon("emblem",98,176,128);
        label(f->font,252,200,584,.92f,ink,"Tu biblioteca, a tu manera");
        label(f->font,252,239,584,.68f,accent,f->app->theme.name);
        label(f->font,252,271,584,.56f,muted,f->active_theme);
        label(f->font,110,336,740,.57f,muted,"Temas: ux0:data/vitaplus/themes/<tema>/theme.ini");
        label(f->font,110,367,740,.57f,muted,"Cambia active_theme en config.ini. Circulo vuelve.");
        return;
    }
    if (f->settings_section==2) {
        size_t start=(f->download_selection/4)*4,j;
        if (!f->downloads->count) { empty(f,"ui_download","Todo al dia","Agrega un juego desde la biblioteca para descargarlo."); return; }
        for (j=start;j<f->downloads->count && j<start+4;j++) {
            const VpDownloadEntry *e=&f->downloads->entries[j];
            int y=145+(int)(j-start)*64;
            unsigned int state=e->state==VP_DOWNLOAD_FAILED?C(245,166,145,255):accent;
            char status[48];
            round_rect(64,y,832,58,10,j==f->download_selection?mix(surface,state,.18f):surface);
            icon(e->state==VP_DOWNLOAD_COMPLETED?"ui_check":"ui_download",74,y+5,32);
            label(f->font,120,y+24,480,.75f,ink,e->item.title);
            (void)snprintf(status,sizeof(status),"%s  %u%%",download_label(e->state),e->percent);
            label(f->font,668,y+24,214,.75f,state,status);
            if (e->state==VP_DOWNLOAD_FAILED) label(f->font,120,y+48,748,.75f,muted,e->error);
            else {
                round_rect(120,y+42,752,4,2,alpha(ink,35));
                if (e->percent) round_rect(120,y+42,fmaxf(4,752*fminf((float)e->percent,100)/100),4,2,state);
            }
        }
        label(f->font,64,430,832,.52f,muted,"X cancela pendiente/activa; X reintenta fallida/cancelada; Circulo vuelve");
        return;
    }
    {
        const char *titles[]={"URL del servidor","Pair code","Sincronizar ahora","Sincronizar metadata"};
        const char *symbols[]={"ui_home","ui_key","ui_sync","ui_folder"};
        const char *values[]={f->romm_url[0]?f->romm_url:"X para configurar",f->pair_code[0]?"Codigo ingresado; Triangulo para canjear":"X para ingresar el codigo de 8 caracteres","Actualiza el catalogo en segundo plano","Descarga y prepara las caratulas"};
        for (i=0;i<4;i++) {
            int y=145+i*62;
            card(64,y,832,54,f->settings_field==i,accent);
            icon(symbols[i],76,y+5,40);
            label(f->font,134,y+23,734,.64f,ink,titles[i]);
            label(f->font,134,y+44,734,.51f,muted,values[i]);
        }
        label(f->font,66,421,828,.54f,muted,"X edita/sincroniza  |  Triangulo canjea y guarda  |  Circulo vuelve");
        label(f->font,66,443,828,.49f,muted,"El codigo se canjea una sola vez. El token nunca se muestra.");
    }
}

static void footer(const VpUiFrame *f,float dt) {
    static const char *icons[]={"ui_library","ui_store","ui_settings"};
    static const char *names[]={"Biblioteca","Tiendas","Ajustes"};
    int i;
    round_rect(28,480,904,48,18,alpha(surface,245));
    if (!appearance.motion_ms) dock_x=278+f->app->active_screen*156;
    else dock_x+=(278+f->app->active_screen*156-dock_x)*fminf(1,dt*14);
    round_rect(dock_x,485,152,38,13,mix(surface,accent,.16f));
    label(f->font,46,499,216,.75f,muted,"L / R  secciones");
    label(f->font,46,521,216,.75f,muted,"START  salir");
    for (i=0;i<3;i++) {
        int x=278+i*156;
        icon(icons[i],x+7,489,30);
        label(f->font,x+41,509,104,.75f,f->app->active_screen==(VpScreen)i?accent:muted,names[i]);
    }
    if (f->downloads->count) {
        char count[24];
        icon("ui_download",768,490,28);
        (void)snprintf(count,sizeof(count),"%u en cola",(unsigned)f->downloads->count);
        label(f->font,803,510,110,.51f,muted,count);
    }
}

void vp_ui_render(const VpUiFrame *f) {
    unsigned long long now=sceKernelGetProcessTimeWide();
    float dt=last_frame?(float)(now-last_frame)/1000000:.016f;
    char key[sizeof(focus_key)];
    char metadata[VP_TEXT+64];
    const char *title=vp_screen_name(f->app->active_screen);
    const char *subtitle="Tu proxima aventura empieza aqui";
    bg=color(f->app->theme.background,C(16,22,36,255));
    surface=color(f->app->theme.surface,C(34,43,61,255));
    accent=color(f->app->theme.accent,C(135,221,196,255));
    ink=color(f->app->theme.text,C(241,243,250,255));
    muted=color(f->app->theme.muted,C(168,181,201,255));
    (void)snprintf(key,sizeof(key),"%d:%d:%d:%u:%u:%d:%d:%d",f->app->active_screen,f->app->library_level,f->app->active_view,(unsigned)f->app->selected_platform,(unsigned)f->app->selected_item,f->settings_section,f->settings_field,f->settings_detail);
    if (strcmp(key,focus_key)) { (void)snprintf(focus_key,sizeof(focus_key),"%s",key); focus_at=now; }
    entrance=appearance.motion_ms?vp_ui_ease((float)(now-focus_at)/((float)appearance.motion_ms*1000)):1;
    asset_budget=1; last_frame=now;
    backdrop(f->background);
    if (f->app->active_screen==VP_SCREEN_LIBRARY) {
        if (f->app->library_level==VP_LEVEL_ROMS) {
            title=f->app->active_platform;
            if (f->visible_count) title=f->app->catalog.items[f->visible[position(f)]].platform_display;
            subtitle="Biblioteca / Tu coleccion";
        } else {
            (void)snprintf(metadata,sizeof(metadata),"%u consolas  /  %u juegos en tu biblioteca",(unsigned)f->group_count,(unsigned)f->app->catalog.count);
            subtitle=metadata;
        }
    } else if (f->app->active_screen==VP_SCREEN_SETTINGS) {
        subtitle=f->settings_detail?"Ajustes / Configuracion":"Un espacio que se adapta a ti";
    } else subtitle="Nuevos mundos por descubrir";
    icon("emblem",25,20,35);
    label(f->font,66,44,130,.8f,ink,"VitaPlus");
    label(f->font,220,44,250,.75f,muted,"A U R O R A");
    round_rect(700,25,232,28,12,alpha(surface,200));
    label(f->font,714,44,204,.75f,f->busy?accent:muted,f->busy?"Sincronizando...":"Tu espacio de juego");
    label(f->font,28,94,720,1.05f,ink,title);
    label(f->font,29,121,770,.57f,muted,subtitle);
    if (f->app->active_screen==VP_SCREEN_LIBRARY) {
        if (f->app->library_level==VP_LEVEL_PLATFORMS) {
            if (!f->group_count) empty(f,"platform_generic","Tu biblioteca te espera","Abre Ajustes / RomM para conectar tu servidor.");
            else {
                const VpPlatformGroup *g=&f->groups[f->app->selected_platform];
                platforms(f);
                icon("ui_folder",28,429,28);
                label(f->font,64,449,600,.65f,ink,g->display);
                label(f->font,700,449,232,.51f,muted,"Flechas navega  /  X abre");
            }
        } else if (!f->visible_count) empty(f,"ui_folder","No hay juegos en esta consola","Circulo vuelve a la biblioteca.");
        else {
            const VpItem *item=&f->app->catalog.items[f->app->selected_item];
            const char *view_icon=f->app->active_view==VP_VIEW_GRID?"ui_grid":(f->app->active_view==VP_VIEW_LIST?"ui_list":"ui_carousel");
            icon(view_icon,802,80,28);
            label(f->font,838,100,94,.51f,muted,"Triangulo");
            roms(f);
            icon(item->installed?"ui_play":"ui_download",28,427,29);
            label(f->font,66,441,538,.68f,ink,item->title);
            label(f->font,66,464,600,.49f,muted,item->description[0]?item->description:"Sin descripcion");
            label(f->font,685,440,247,.52f,accent,item->multi_file?"Varios archivos":(item->installed?"X jugar":"X descargar / instalar"));
            label(f->font,685,463,247,.75f,muted,"O volver  /  [] favorito");
        }
    } else if (f->app->active_screen==VP_SCREEN_SETTINGS) settings(f);
    else {
        empty(f,"ui_store","Proximamente","Estamos preparando un nuevo lugar para descubrir juegos.");
        label(f->font,204,379,552,.52f,accent,"Tu biblioteca y RomM siguen disponibles con L / R.");
    }
    footer(f,dt);
    if (strcmp(last_notice,f->app->notice) || f->refresh_notice) {
        (void)snprintf(last_notice,sizeof(last_notice),"%s",f->app->notice); notice_at=now;
    }
    if (last_notice[0]) {
        float age=(float)(now-notice_at)/1000000;
        /* Installation confirmation remains visible until explicitly resolved. */
        if (age<6 || f->app->installation_armed) {
            unsigned int a=(unsigned int)(255*fmaxf(0,fminf(1,(6-age)*2)));
            if (f->app->installation_armed) a=255;
            round_rect(106,414,748,58,15,alpha(mix(surface,bg,.6f),a));
            icon(f->app->installation_armed?"ui_key":"ui_sync",118,427,28);
            two_line_label(f->font,156,437,677,alpha(ink,a),last_notice);
        }
    }
}

void vp_ui_shutdown(void) {
    size_t i;
    vita2d_wait_rendering_done();
    for (i=0;i<sizeof(assets)/sizeof(assets[0]);i++) {
        if (assets[i].texture) vita2d_free_texture(assets[i].texture);
        assets[i].texture=NULL; assets[i].attempted=false;
    }
}
