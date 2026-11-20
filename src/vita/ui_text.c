#include "ui_text.h"
#include <math.h>
#include <string.h>

static const VpTypeToken tokens[VP_TYPE_COUNT] = {
    {18,700,1.15f}, {17,500,1.15f}, {16,500,1.35f}, {30,700,1.15f},
    {16,500,1.35f}, {16,600,1.15f}, {26,700,1.15f}, {16,500,1.15f},
    {16,500,1.15f}, {16,500,1.35f}, {16,500,1.35f}, {20,500,1.15f}
};

const VpTypeToken *vp_ui_type(VpTypeRole role) {
    return &tokens[role >= 0 && role < VP_TYPE_COUNT ? role : VP_TYPE_DESCRIPTION];
}

bool vp_ui_fonts_load(VpUiFonts *fonts) {
    static const char *paths[] = {"app0:assets/blueglass/fonts/Inter-Regular.ttf",
        "app0:assets/blueglass/fonts/Inter-Medium.ttf", "app0:assets/blueglass/fonts/Inter-SemiBold.ttf",
        "app0:assets/blueglass/fonts/Inter-Bold.ttf"};
    size_t i;
    memset(fonts,0,sizeof(*fonts));
    for (i=0;i<4;i++) {
        fonts->weights[i]=vita2d_load_font_file(paths[i]);
        if (!fonts->weights[i]) { vp_ui_fonts_free(fonts); return false; }
    }
    return true;
}

void vp_ui_fonts_free(VpUiFonts *fonts) {
    size_t i;
    for (i=0;i<4;i++) {
        if (fonts->weights[i]) vita2d_free_font(fonts->weights[i]);
        fonts->weights[i]=NULL;
    }
}

static vita2d_font *face(VpUiFonts *fonts, VpTypeRole role, bool selected) {
    unsigned int weight=vp_ui_type(role)->weight;
    if (selected && (role==VP_TYPE_NAV || role==VP_TYPE_LIST_TITLE)) weight=600;
    return fonts->weights[(weight-400)/100];
}

/* Fixed-size cache: repeated frames do not traverse the FreeType glyph atlas to
 * measure the same labels. Text and the final weight/size form the entire key. */
int vp_ui_text_width(VpUiFonts *fonts, VpTypeRole role, bool selected, const char *text) {
    typedef struct { vita2d_font *font; unsigned int size; int width; char text[768]; } Metric;
    static Metric cache[96];
    static size_t next;
    size_t i,n=strlen(text);
    vita2d_font *font=face(fonts,role,selected);
    unsigned int size=vp_ui_type(role)->size;
    for (i=0;i<96;i++)
        if (cache[i].font==font && cache[i].size==size && !strcmp(cache[i].text,text)) return cache[i].width;
    {
        int width=vita2d_font_text_width(font,size,text);
        if (n<sizeof(cache[0].text)) {
            Metric *entry=&cache[next++%96];
            entry->font=font; entry->size=size; entry->width=width;
            memcpy(entry->text,text,n+1);
        }
        return width;
    }
}

static size_t previous_glyph(const char *text,size_t n) {
    if (n) { --n; while (n && ((unsigned char)text[n]&0xc0)==0x80) --n; }
    return n;
}

static size_t complete_prefix(const char *text,size_t limit) {
    size_t n=0;
    while (n<limit && text[n]) {
        unsigned char c=(unsigned char)text[n];
        size_t bytes=c<0x80?1:(c>=0xc2 && c<=0xdf?2:(c>=0xe0 && c<=0xef?3:(c>=0xf0 && c<=0xf4?4:0))),i;
        if (!bytes || n+bytes>limit) break;
        for (i=1;i<bytes;i++) if (((unsigned char)text[n+i]&0xc0)!=0x80) return n;
        n+=bytes;
    }
    return n;
}

void vp_ui_text(VpUiFonts *fonts,VpTypeRole role,bool selected,
                int x,int y,int width,unsigned int color,const char *value) {
    char text[768];
    size_t n,length,i;
    if (!fonts || !value || width<=0) return;
    length=strlen(value); n=length<sizeof(text)-4?length:sizeof(text)-4;
    n=complete_prefix(value,n);
    memcpy(text,value,n); text[n]='\0';
    for (i=0;i<n;i++) if ((unsigned char)text[i]<0x20) text[i]=' ';
    if (n<length || vp_ui_text_width(fonts,role,selected,text)>width) {
        for (;;) {
            memcpy(text+n,"...",4);
            if (vp_ui_text_width(fonts,role,selected,text)<=width) break;
            if (!n) { text[0]='\0'; break; }
            n=previous_glyph(text,n);
        }
    }
    vita2d_font_draw_text(face(fonts,role,selected),x,y,color,vp_ui_type(role)->size,text);
}

void vp_ui_paragraph(VpUiFonts *fonts,VpTypeRole role,int x,int y,
                     int width,int lines,unsigned int color,const char *value) {
    typedef struct {
        vita2d_font *font;
        VpTypeRole role;
        int width,lines,count;
        char text[768];
        size_t offsets[8],lengths[8];
    } Layout;
    static Layout cache[8];
    static size_t next;
    Layout *layout=NULL;
    int row,advance=(int)lroundf(vp_ui_type(role)->size*vp_ui_type(role)->leading);
    size_t i;
    if (!value || width<=0 || lines<=0) return;
    if (lines>8) lines=8;
    for (i=0;i<8;i++) if (cache[i].font==face(fonts,role,false) && cache[i].role==role &&
        cache[i].width==width && cache[i].lines==lines && !strcmp(cache[i].text,value)) { layout=&cache[i]; break; }
    if (!layout) {
        const char *p;
        layout=&cache[next++%8]; memset(layout,0,sizeof(*layout));
        layout->font=face(fonts,role,false); layout->role=role; layout->width=width; layout->lines=lines;
        i=strlen(value); if (i>=sizeof(layout->text)) i=sizeof(layout->text)-1;
        i=complete_prefix(value,i);
        memcpy(layout->text,value,i); layout->text[i]='\0'; p=layout->text;
        for (row=0;row<lines && *p;row++) {
            char line[768]; size_t n=0,last_space=0;
            layout->offsets[row]=(size_t)(p-layout->text);
            if (row==lines-1) n=strlen(p);
            else while (p[n] && p[n]!='\n' && n<sizeof(line)-5) {
                size_t end=n+1;
                while (p[end] && ((unsigned char)p[end]&0xc0)==0x80) ++end;
                memcpy(line,p,end); line[end]='\0';
                if (vp_ui_text_width(fonts,role,false,line)>width) break;
                if (p[n]==' ') last_space=n;
                n=end;
            }
            if (!n && *p!='\n') n=strlen(p);
            else if (row!=lines-1 && p[n] && p[n]!='\n' && last_space) n=last_space;
            layout->lengths[row]=n; layout->count++;
            p+=n; while (*p==' ' || *p=='\n' || *p=='\r') ++p;
        }
    }
    for (row=0;row<layout->count;row++) {
        char line[768]; size_t n=layout->lengths[row];
        memcpy(line,layout->text+layout->offsets[row],n); line[n]='\0';
        vp_ui_text(fonts,role,false,x,y+row*advance,width,color,line);
    }
}
