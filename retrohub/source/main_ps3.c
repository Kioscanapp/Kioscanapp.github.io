/* Retrovicios PS3 v1.7 - modern cover frontend. */
#include "retrohub.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __PSL1GHT__
#include <SDL.h>
#else
#error "main_ps3.c is intended for PSL1GHT/PS3 builds"
#endif

#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_THREAD_LOCALS
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define WINDOW_W 1280
#define WINDOW_H 720
#define INSTALL_ROOT "/dev_hdd0/game/RVIC00001/USRDIR"
#define STATE_FILE INSTALL_ROOT "/data/state.txt"
#define VISIBLE_VIEWS 11
#define GRID_COLS 3
#define GRID_ROWS 2
#define GRID_PAGE (GRID_COLS * GRID_ROWS)
#define COVER_CACHE_SIZE 10

/* SDL2_PSL1GHT button order from its PS3 joystick driver. */
#define PAD_LEFT 0
#define PAD_DOWN 1
#define PAD_RIGHT 2
#define PAD_UP 3
#define PAD_START 4
#define PAD_SELECT 7
#define PAD_SQUARE 8
#define PAD_CROSS 9
#define PAD_CIRCLE 10
#define PAD_TRIANGLE 11

extern void rh_draw_text(SDL_Surface*,int,int,const char*,int,Uint32,int);

static const char *roots[] = {
    INSTALL_ROOT,
    "/dev_usb000/RETROVICIOS", "/dev_usb001/RETROVICIOS", "/dev_usb002/RETROVICIOS", "/dev_usb003/RETROVICIOS",
    "/dev_usb004/RETROVICIOS", "/dev_usb005/RETROVICIOS", "/dev_usb006/RETROVICIOS", "/dev_usb007/RETROVICIOS"
};

static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture *g_texture = NULL;

/* Keep large structures off the PS3 process stack. */
static RHCatalog g_catalog;
static RHState g_state;
static int g_indices[RH_MAX_FILTERED];

typedef struct {
    char path[RH_PATH_MAX];
    SDL_Surface *surface;
    unsigned long stamp;
} RHCoverCacheEntry;

static RHCoverCacheEntry g_cover_cache[COVER_CACHE_SIZE];
static unsigned long g_cover_stamp = 1;

static void boot_log(const char *message)
{
    FILE *f=fopen(INSTALL_ROOT "/data/boot.log","a");
    if(f){fprintf(f,"%s\n",message);fclose(f);}
}

static int present_screen(SDL_Surface *screen)
{
    if(!screen || !g_renderer || !g_texture) return -1;
    if(SDL_UpdateTexture(g_texture,NULL,screen->pixels,screen->pitch)!=0) return -2;
    SDL_RenderClear(g_renderer);
    if(SDL_RenderCopy(g_renderer,g_texture,NULL,NULL)!=0) return -3;
    SDL_RenderPresent(g_renderer);
    return 0;
}

static void fill_rect(SDL_Surface *s,int x,int y,int w,int h,Uint32 c)
{
    SDL_Rect r={x,y,w,h};
    SDL_FillRect(s,&r,c);
}

static void frame_rect(SDL_Surface *s,int x,int y,int w,int h,int t,Uint32 c)
{
    fill_rect(s,x,y,w,t,c);
    fill_rect(s,x,y+h-t,w,t,c);
    fill_rect(s,x,y,t,h,c);
    fill_rect(s,x+w-t,y,t,h,c);
}

static void draw_background(SDL_Surface *screen)
{
    int y;
    for(y=0;y<WINDOW_H;y+=8){
        int t=(y*30)/WINDOW_H;
        Uint32 c=SDL_MapRGB(screen->format,(Uint8)(5+t/4),(Uint8)(12+t/2),(Uint8)(24+t));
        fill_rect(screen,0,y,WINDOW_W,8,c);
    }
    fill_rect(screen,0,0,WINDOW_W,90,SDL_MapRGB(screen->format,8,18,34));
    fill_rect(screen,0,88,WINDOW_W,2,SDL_MapRGB(screen->format,27,92,145));
}

static int view_count(void){return 3+(int)rh_system_count;}

static RHFilter filter_for_view(int v)
{
    RHFilter f;
    if(v==0){f.kind=RH_VIEW_ALL;f.system_index=-1;}
    else if(v==1){f.kind=RH_VIEW_FAVORITES;f.system_index=-1;}
    else if(v==2){f.kind=RH_VIEW_RECENTS;f.system_index=-1;}
    else{f.kind=RH_VIEW_SYSTEM;f.system_index=v-3;}
    return f;
}

static const char *view_name(int v)
{
    if(v==0)return "TODOS";
    if(v==1)return "FAVORITOS";
    if(v==2)return "RECIENTES";
    return rh_system_name(v-3);
}

static const char *system_short_name(int system_index)
{
    static const char *shorts[] = {
        "NES","SNES","MASTER SYSTEM","GAME GEAR","MEGA DRIVE","SEGA CD","GAME BOY","GBA",
        "ATARI 2600","ATARI 7800","LYNX","PC ENGINE","NEO GEO POCKET","WONDERSWAN",
        "VIRTUAL BOY","VECTREX","MSX","ARCADE","SG-1000","ATARI 5200",
        "COLECOVISION","INTELLIVISION","ODYSSEY2","CHANNEL F","GAME & WATCH","POKEMON MINI",
        "SUPERGRAFX","GX4000"
    };
    if(system_index < 0 || system_index >= (int)(sizeof(shorts)/sizeof(shorts[0]))) return "RETRO";
    return shorts[system_index];
}

static Uint32 system_color(SDL_Surface *screen,int system_index,int dim)
{
    static const unsigned char rgb[][3] = {
        {42,168,255},{121,92,255},{50,210,172},{255,162,72},{82,145,255},{255,76,115},
        {55,190,255},{255,197,62},{255,101,101},{96,214,109},{62,202,185},{65,135,255},
        {195,95,224},{151,215,78},{226,87,168},{67,206,235},{84,169,224},{242,121,55}
    };
    int n=(int)(sizeof(rgb)/sizeof(rgb[0]));
    int i=system_index;
    int r,g,b;
    if(i<0)i=0; i%=n;
    r=rgb[i][0]; g=rgb[i][1]; b=rgb[i][2];
    if(dim){r/=3;g/=3;b/=3;}
    return SDL_MapRGB(screen->format,(Uint8)r,(Uint8)g,(Uint8)b);
}

static void rebuild_filter(const RHCatalog *cat,const RHState *st,int view,int *indices,size_t *count,int *sel)
{
    *count=rh_filter_catalog(cat,st,filter_for_view(view),indices,RH_MAX_FILTERED);
    if(*count==0)*sel=0;
    else if(*sel<0)*sel=0;
    else if((size_t)*sel>=*count)*sel=(int)*count-1;
}

static int path_without_extension(const char *name,char *out,size_t out_n)
{
    const char *base=strrchr(name,'/');
    const char *dot;
    size_t len;
    base=base?base+1:name;
    dot=strrchr(base,'.');
    len=dot?(size_t)(dot-base):strlen(base);
    if(!out_n)return 0;
    if(len>=out_n)len=out_n-1;
    memcpy(out,base,len);
    out[len]='\0';
    return (int)len;
}

static int find_cover_path(const RHGame *g,char *out,size_t out_n)
{
    static const char *exts[]={"jpg","jpeg","png","bmp"};
    const char *mark=strstr(g->path,"/roms/");
    const char *slash=strrchr(g->path,'/');
    char root[RH_PATH_MAX],base[RH_NAME_MAX],dir[RH_PATH_MAX];
    size_t i,root_len,dir_len;

    if(!g || !out || out_n==0 || !mark || !slash)return 0;
    root_len=(size_t)(mark-g->path);
    if(root_len>=sizeof(root))return 0;
    memcpy(root,g->path,root_len); root[root_len]='\0';
    path_without_extension(g->path,base,sizeof(base));

    dir_len=(size_t)(slash-g->path);
    if(dir_len>=sizeof(dir))dir_len=sizeof(dir)-1;
    memcpy(dir,g->path,dir_len); dir[dir_len]='\0';

    for(i=0;i<sizeof(exts)/sizeof(exts[0]);++i){
        snprintf(out,out_n,"%s/covers/%s/%s.%s",root,rh_systems[g->system_index].id,base,exts[i]);
        if(rh_file_exists(out))return 1;
    }
    for(i=0;i<sizeof(exts)/sizeof(exts[0]);++i){
        snprintf(out,out_n,"%s/covers/%s.%s",root,base,exts[i]);
        if(rh_file_exists(out))return 1;
    }
    for(i=0;i<sizeof(exts)/sizeof(exts[0]);++i){
        snprintf(out,out_n,"%s/%s.%s",dir,base,exts[i]);
        if(rh_file_exists(out))return 1;
    }
    out[0]='\0';
    return 0;
}

static SDL_Surface *load_image_surface(SDL_Surface *screen,const char *path)
{
    int w=0,h=0,n=0;
    unsigned char *pixels=stbi_load(path,&w,&h,&n,4);
    SDL_Surface *raw,*converted;
    if(!pixels || w<=0 || h<=0)return NULL;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    raw=SDL_CreateRGBSurfaceFrom(pixels,w,h,32,w*4,
                                 0xFF000000,0x00FF0000,0x0000FF00,0x000000FF);
#else
    raw=SDL_CreateRGBSurfaceFrom(pixels,w,h,32,w*4,
                                 0x000000FF,0x0000FF00,0x00FF0000,0xFF000000);
#endif
    if(!raw){stbi_image_free(pixels);return NULL;}
    converted=SDL_ConvertSurface(raw,screen->format,0);
    SDL_FreeSurface(raw);
    stbi_image_free(pixels);
    return converted;
}

static SDL_Surface *cover_for_game(SDL_Surface *screen,const RHGame *g)
{
    char path[RH_PATH_MAX];
    int i,slot=-1;
    unsigned long oldest=~0UL;

    if(!find_cover_path(g,path,sizeof(path)))return NULL;

    for(i=0;i<COVER_CACHE_SIZE;++i){
        if(g_cover_cache[i].surface && strcmp(g_cover_cache[i].path,path)==0){
            g_cover_cache[i].stamp=g_cover_stamp++;
            return g_cover_cache[i].surface;
        }
        if(!g_cover_cache[i].surface){slot=i;oldest=0;break;}
        if(g_cover_cache[i].stamp<oldest){oldest=g_cover_cache[i].stamp;slot=i;}
    }

    if(slot<0)slot=0;
    if(g_cover_cache[slot].surface){
        SDL_FreeSurface(g_cover_cache[slot].surface);
        g_cover_cache[slot].surface=NULL;
    }
    g_cover_cache[slot].surface=load_image_surface(screen,path);
    if(!g_cover_cache[slot].surface){
        g_cover_cache[slot].path[0]='\0';
        return NULL;
    }
    snprintf(g_cover_cache[slot].path,sizeof(g_cover_cache[slot].path),"%s",path);
    g_cover_cache[slot].stamp=g_cover_stamp++;
    return g_cover_cache[slot].surface;
}

static void free_cover_cache(void)
{
    int i;
    for(i=0;i<COVER_CACHE_SIZE;++i){
        if(g_cover_cache[i].surface)SDL_FreeSurface(g_cover_cache[i].surface);
        g_cover_cache[i].surface=NULL;
    }
}

static void blit_cover(SDL_Surface *screen,SDL_Surface *cover,int x,int y,int w,int h)
{
    SDL_Rect src,dst;
    float sr,dr;
    int nw,nh;
    if(!cover)return;

    sr=(float)cover->w/(float)cover->h;
    dr=(float)w/(float)h;
    if(sr>dr){
        nh=h;
        nw=(int)(h*sr);
    }else{
        nw=w;
        nh=(int)(w/sr);
    }
    src.x=0;src.y=0;src.w=cover->w;src.h=cover->h;
    dst.x=x+(w-nw)/2;dst.y=y+(h-nh)/2;dst.w=nw;dst.h=nh;

    SDL_SetClipRect(screen,&(SDL_Rect){x,y,w,h});
    SDL_BlitScaled(cover,&src,screen,&dst);
    SDL_SetClipRect(screen,NULL);
}

static void draw_fallback_cover(SDL_Surface *screen,const RHGame *g,int x,int y,int w,int h)
{
    Uint32 c=system_color(screen,g->system_index,0);
    Uint32 dark=system_color(screen,g->system_index,1);
    Uint32 white=SDL_MapRGB(screen->format,240,246,252);
    int i;
    fill_rect(screen,x,y,w,h,dark);
    for(i=0;i<h;i+=14){
        if(((i/14)&1)==0)fill_rect(screen,x,y+i,w,7,c);
    }
    fill_rect(screen,x+12,y+18,w-24,h-36,SDL_MapRGB(screen->format,11,19,31));
    rh_draw_text(screen,x+24,y+42,"RETROVICIOS",2,c,14);
    rh_draw_text(screen,x+24,y+h/2-12,system_short_name(g->system_index),2,white,18);
    rh_draw_text(screen,x+24,y+h-58,g->name,1,white,20);
}

static void draw_game_card(SDL_Surface *screen,const RHGame *g,int x,int y,int w,int h,int selected,int favorite)
{
    Uint32 panel=SDL_MapRGB(screen->format,15,27,43);
    Uint32 border=SDL_MapRGB(screen->format,39,61,84);
    Uint32 white=SDL_MapRGB(screen->format,238,244,250);
    Uint32 muted=SDL_MapRGB(screen->format,147,164,181);
    Uint32 accent=system_color(screen,g->system_index,0);
    SDL_Surface *cover=cover_for_game(screen,g);

    fill_rect(screen,x+5,y+7,w,h,SDL_MapRGB(screen->format,2,7,14));
    fill_rect(screen,x,y,w,h,panel);
    frame_rect(screen,x,y,w,h,selected?3:1,selected?accent:border);

    fill_rect(screen,x+8,y+8,w-16,h-48,SDL_MapRGB(screen->format,5,10,18));
    if(cover)blit_cover(screen,cover,x+8,y+8,w-16,h-48);
    else draw_fallback_cover(screen,g,x+8,y+8,w-16,h-48);

    rh_draw_text(screen,x+10,y+h-34,g->name,1,white,22);
    rh_draw_text(screen,x+10,y+h-18,system_short_name(g->system_index),1,muted,22);
    if(favorite)rh_draw_text(screen,x+w-22,y+10,"*",2,accent,1);
}

static void draw_sidebar(SDL_Surface *screen,int view,int focus)
{
    Uint32 panel=SDL_MapRGB(screen->format,9,20,34);
    Uint32 panel_sel=SDL_MapRGB(screen->format,20,43,67);
    Uint32 white=SDL_MapRGB(screen->format,238,244,250);
    Uint32 muted=SDL_MapRGB(screen->format,128,151,173);
    Uint32 accent=SDL_MapRGB(screen->format,65,173,255);
    int vf=0,row;

    fill_rect(screen,18,108,202,538,panel);
    frame_rect(screen,18,108,202,538,1,SDL_MapRGB(screen->format,31,59,84));
    rh_draw_text(screen,38,128,"BIBLIOTECA",2,white,0);

    if(view>=VISIBLE_VIEWS)vf=view-VISIBLE_VIEWS+1;
    for(row=0;row<VISIBLE_VIEWS && vf+row<view_count();++row){
        int v=vf+row;
        int yy=166+row*42;
        if(v==view){
            fill_rect(screen,28,yy-9,182,34,panel_sel);
            fill_rect(screen,28,yy-9,4,34,accent);
        }
        rh_draw_text(screen,42,yy,view_name(v),1,v==view?white:muted,23);
    }
    if(focus==0)frame_rect(screen,18,108,202,538,2,accent);
}

static void draw_preview(SDL_Surface *screen,const RHCatalog *cat,const RHState *st,
                         const int *indices,size_t filtered,int game_sel)
{
    const int x=835,y=108,w=427,h=538;
    Uint32 panel=SDL_MapRGB(screen->format,9,20,34);
    Uint32 white=SDL_MapRGB(screen->format,238,244,250);
    Uint32 muted=SDL_MapRGB(screen->format,137,157,177);
    Uint32 border=SDL_MapRGB(screen->format,31,59,84);

    fill_rect(screen,x,y,w,h,panel);
    frame_rect(screen,x,y,w,h,1,border);

    if(filtered==0){
        rh_draw_text(screen,x+34,y+72,"NO HAY JUEGOS",3,white,0);
        rh_draw_text(screen,x+34,y+118,"COPIA LAS ROMS Y PULSA",1,muted,0);
        rh_draw_text(screen,x+34,y+138,"TRIANGULO PARA REESCANEAR",1,muted,0);
        return;
    }

    {
        const RHGame *g=&cat->games[indices[game_sel]];
        Uint32 accent=system_color(screen,g->system_index,0);
        SDL_Surface *cover=cover_for_game(screen,g);
        char info[96];
        int fav=rh_state_is_favorite(st,g->path);

        fill_rect(screen,x+20,y+20,190,278,SDL_MapRGB(screen->format,4,10,18));
        frame_rect(screen,x+20,y+20,190,278,2,accent);
        if(cover)blit_cover(screen,cover,x+24,y+24,182,270);
        else draw_fallback_cover(screen,g,x+24,y+24,182,270);

        rh_draw_text(screen,x+230,y+26,system_short_name(g->system_index),2,accent,19);
        rh_draw_text(screen,x+230,y+66,g->name,2,white,20);
        snprintf(info,sizeof(info),"%s",rh_core_available_ps3(g->system_index)?"LISTO PARA JUGAR":"CORE NO DISPONIBLE");
        rh_draw_text(screen,x+230,y+132,info,1,rh_core_available_ps3(g->system_index)?accent:SDL_MapRGB(screen->format,255,104,104),24);
        if(fav)rh_draw_text(screen,x+230,y+164,"EN FAVORITOS",1,accent,24);

        fill_rect(screen,x+20,y+320,w-40,1,border);
        rh_draw_text(screen,x+20,y+344,"RUTA",1,muted,0);
        rh_draw_text(screen,x+20,y+365,g->path,1,muted,51);

        rh_draw_text(screen,x+20,y+430,"X  JUGAR",2,white,0);
        rh_draw_text(screen,x+168,y+432,"CUADRADO  FAVORITO",1,muted,0);
        rh_draw_text(screen,x+20,y+474,"SELECT + START",2,accent,0);
        rh_draw_text(screen,x+220,y+478,"VOLVER DESDE EL JUEGO",1,muted,0);
    }
}

static void draw_ui(SDL_Surface *screen,const RHCatalog *cat,const RHState *st,int view,int focus,
                    int game_sel,const int *indices,size_t filtered,const char *status)
{
    Uint32 white=SDL_MapRGB(screen->format,238,244,250);
    Uint32 muted=SDL_MapRGB(screen->format,133,154,174);
    Uint32 accent=SDL_MapRGB(screen->format,65,173,255);
    Uint32 panel=SDL_MapRGB(screen->format,9,20,34);
    int i;
    int first=(game_sel/GRID_PAGE)*GRID_PAGE;
    char buf[128];

    draw_background(screen);

    rh_draw_text(screen,32,25,"RETROVICIOS",4,white,0);
    rh_draw_text(screen,36,62,"TU BIBLIOTECA RETRO",1,muted,0);
    snprintf(buf,sizeof(buf),"%d JUEGOS",(int)cat->count);
    rh_draw_text(screen,1040,28,buf,2,white,0);
    snprintf(buf,sizeof(buf),"%d FAVORITOS",(int)st->favorite_count);
    rh_draw_text(screen,1040,57,buf,1,accent,0);

    draw_sidebar(screen,view,focus);

    fill_rect(screen,232,108,590,538,panel);
    frame_rect(screen,232,108,590,538,1,SDL_MapRGB(screen->format,31,59,84));
    rh_draw_text(screen,252,128,view_name(view),2,white,26);
    snprintf(buf,sizeof(buf),"%d JUEGOS",(int)filtered);
    rh_draw_text(screen,704,132,buf,1,muted,0);

    for(i=0;i<GRID_PAGE && first+i<(int)filtered;i++){
        int pos=first+i;
        int col=i%GRID_COLS,row=i/GRID_COLS;
        int gx=250+col*188;
        int gy=166+row*232;
        const RHGame *g=&cat->games[indices[pos]];
        draw_game_card(screen,g,gx,gy,174,215,pos==game_sel && focus==1,
                       rh_state_is_favorite(st,g->path));
    }

    if(filtered==0){
        rh_draw_text(screen,338,310,"SIN JUEGOS EN ESTA VISTA",2,muted,0);
        rh_draw_text(screen,350,344,"TRIANGULO PARA REESCANEAR",1,accent,0);
    }

    draw_preview(screen,cat,st,indices,filtered,game_sel);

    fill_rect(screen,18,660,1244,44,SDL_MapRGB(screen->format,7,16,28));
    frame_rect(screen,18,660,1244,44,1,SDL_MapRGB(screen->format,31,59,84));
    rh_draw_text(screen,38,675,"X JUGAR",1,white,0);
    rh_draw_text(screen,132,675,"O VOLVER",1,white,0);
    rh_draw_text(screen,244,675,"CUADRADO FAVORITO",1,white,0);
    rh_draw_text(screen,430,675,"TRIANGULO REESCANEAR",1,white,0);
    if(status && status[0])rh_draw_text(screen,720,675,status,1,accent,68);

    present_screen(screen);
}

static void rescan(RHCatalog *cat,const RHState *st,int view,int *indices,size_t *filtered,
                   int *game_sel,char *status,size_t status_n)
{
    int n=rh_catalog_scan_roots(cat,roots,sizeof(roots)/sizeof(roots[0]));
    rebuild_filter(cat,st,view,indices,filtered,game_sel);
    snprintf(status,status_n,"ESCANEO COMPLETO: %d JUEGOS",n);
}

static void grid_move(int delta,const size_t filtered,int *game_sel)
{
    int next;
    if(!filtered)return;
    next=*game_sel+delta;
    if(next<0)next=0;
    if((size_t)next>=filtered)next=(int)filtered-1;
    *game_sel=next;
}

int main(int argc,char **argv)
{
    SDL_Surface *screen=NULL;
    SDL_Event ev;
    SDL_Joystick *joy=0;
    RHCatalog *catalog=&g_catalog;
    RHState *state=&g_state;
    int *indices=g_indices;
    size_t filtered=0;
    int view=0,focus=0,game_sel=0,running=1;
    Uint32 last_axis=0;
    char status[96]="";
    (void)argc;(void)argv;

    boot_log("Retrovicios v1.7 boot");
    SDL_SetMainReady();
    if(SDL_Init(SDL_INIT_VIDEO)<0){boot_log("SDL video init failed");return 1;}

    g_window=SDL_CreateWindow("Retrovicios",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,
                              WINDOW_W,WINDOW_H,0);
    if(!g_window){
        g_window=SDL_CreateWindow("Retrovicios",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,
                                  720,480,0);
    }
    if(!g_window){SDL_Quit();return 2;}

    g_renderer=SDL_CreateRenderer(g_window,-1,0);
    if(!g_renderer){SDL_DestroyWindow(g_window);SDL_Quit();return 3;}
    SDL_RenderSetLogicalSize(g_renderer,WINDOW_W,WINDOW_H);

    g_texture=SDL_CreateTexture(g_renderer,SDL_PIXELFORMAT_RGB888,SDL_TEXTUREACCESS_STREAMING,
                                WINDOW_W,WINDOW_H);
    if(!g_texture){SDL_DestroyRenderer(g_renderer);SDL_DestroyWindow(g_window);SDL_Quit();return 4;}

    {
        Uint32 fmt=0;
        int access=0,tw=0,th=0;
        if(SDL_QueryTexture(g_texture,&fmt,&access,&tw,&th)!=0)fmt=SDL_PIXELFORMAT_RGB888;
        screen=SDL_CreateRGBSurfaceWithFormat(0,WINDOW_W,WINDOW_H,SDL_BITSPERPIXEL(fmt),fmt);
    }
    if(!screen){SDL_DestroyTexture(g_texture);SDL_DestroyRenderer(g_renderer);SDL_DestroyWindow(g_window);SDL_Quit();return 5;}

    if(SDL_InitSubSystem(SDL_INIT_JOYSTICK)==0 && SDL_NumJoysticks()>0){
        joy=SDL_JoystickOpen(0);
        if(joy)SDL_JoystickEventState(SDL_ENABLE);
    }

    rh_state_load(state,STATE_FILE);
    rescan(catalog,state,view,indices,&filtered,&game_sel,status,sizeof(status));
    draw_ui(screen,catalog,state,view,focus,game_sel,indices,filtered,status);

    while(running){
        while(SDL_PollEvent(&ev)){
            int nav=0,confirm=0,back=0,favorite=0,scan_req=0,left=0,right=0;
            if(ev.type==SDL_QUIT)running=0;
            else if(ev.type==SDL_KEYDOWN){
                if(ev.key.keysym.sym==SDLK_UP)nav=-1;
                else if(ev.key.keysym.sym==SDLK_DOWN)nav=1;
                else if(ev.key.keysym.sym==SDLK_LEFT)left=1;
                else if(ev.key.keysym.sym==SDLK_RIGHT)right=1;
                else if(ev.key.keysym.sym==SDLK_RETURN||ev.key.keysym.sym==SDLK_SPACE)confirm=1;
                else if(ev.key.keysym.sym==SDLK_ESCAPE)back=1;
                else if(ev.key.keysym.sym==SDLK_f)favorite=1;
                else if(ev.key.keysym.sym==SDLK_r)scan_req=1;
            }else if(ev.type==SDL_JOYBUTTONDOWN){
                if(ev.jbutton.button==PAD_UP)nav=-1;
                else if(ev.jbutton.button==PAD_DOWN)nav=1;
                else if(ev.jbutton.button==PAD_LEFT)left=1;
                else if(ev.jbutton.button==PAD_RIGHT)right=1;
                else if(ev.jbutton.button==PAD_CROSS)confirm=1;
                else if(ev.jbutton.button==PAD_CIRCLE)back=1;
                else if(ev.jbutton.button==PAD_SQUARE)favorite=1;
                else if(ev.jbutton.button==PAD_TRIANGLE)scan_req=1;
                else if(ev.jbutton.button==PAD_START && filtered)confirm=1;
            }else if(ev.type==SDL_JOYAXISMOTION && SDL_GetTicks()-last_axis>180){
                if(ev.jaxis.axis==1){
                    if(ev.jaxis.value<-18000){nav=-1;last_axis=SDL_GetTicks();}
                    else if(ev.jaxis.value>18000){nav=1;last_axis=SDL_GetTicks();}
                }else if(ev.jaxis.axis==0){
                    if(ev.jaxis.value<-18000){left=1;last_axis=SDL_GetTicks();}
                    else if(ev.jaxis.value>18000){right=1;last_axis=SDL_GetTicks();}
                }
            }

            if(nav){
                if(focus==0){
                    view+=nav;
                    if(view<0)view=view_count()-1;
                    if(view>=view_count())view=0;
                    game_sel=0;
                    rebuild_filter(catalog,state,view,indices,&filtered,&game_sel);
                }else{
                    grid_move(nav*GRID_COLS,filtered,&game_sel);
                }
            }
            if(left){
                if(focus==1){
                    if(game_sel%GRID_COLS==0)focus=0;
                    else grid_move(-1,filtered,&game_sel);
                }
            }
            if(right){
                if(focus==0)focus=1;
                else if(filtered && game_sel%GRID_COLS<GRID_COLS-1)grid_move(1,filtered,&game_sel);
            }
            if(back){
                if(focus==1)focus=0;
                else running=0;
            }
            if(confirm){
                if(focus==0){
                    focus=1;
                }else if(filtered){
                    RHGame *g=&catalog->games[indices[game_sel]];
                    if(!rh_core_available_ps3(g->system_index)){
                        snprintf(status,sizeof(status),"FALTA CORE: %.70s",rh_system_core(g->system_index));
                    }else{
                        rh_state_touch_recent(state,g->path);
                        rh_state_save(state,STATE_FILE);
                        snprintf(status,sizeof(status),"ABRIENDO %.65s",g->name);
                        draw_ui(screen,catalog,state,view,focus,game_sel,indices,filtered,status);
                        rh_launch_game_ps3(g);
                    }
                }
            }
            if(favorite && focus==1 && filtered){
                RHGame *g=&catalog->games[indices[game_sel]];
                int added=rh_state_toggle_favorite(state,g->path);
                rh_state_save(state,STATE_FILE);
                snprintf(status,sizeof(status),added>0?"AGREGADO A FAVORITOS":"QUITADO DE FAVORITOS");
                rebuild_filter(catalog,state,view,indices,&filtered,&game_sel);
            }
            if(scan_req)rescan(catalog,state,view,indices,&filtered,&game_sel,status,sizeof(status));
            draw_ui(screen,catalog,state,view,focus,game_sel,indices,filtered,status);
        }
        SDL_Delay(8);
    }

    rh_state_save(state,STATE_FILE);
    free_cover_cache();
    if(joy)SDL_JoystickClose(joy);
    if(screen)SDL_FreeSurface(screen);
    if(g_texture)SDL_DestroyTexture(g_texture);
    if(g_renderer)SDL_DestroyRenderer(g_renderer);
    if(g_window)SDL_DestroyWindow(g_window);
    SDL_Quit();
    return 0;
}
