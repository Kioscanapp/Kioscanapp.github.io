/* Retrovicios PS3 v1.0 - neon grid frontend. */
#include "retrohub.h"

#include <stdio.h>
#include <string.h>

#ifdef __PSL1GHT__
#include <SDL.h>
#else
#error "main_ps3.c is intended for PSL1GHT/PS3 builds"
#endif

#define WINDOW_W 1280
#define WINDOW_H 720
#define INSTALL_ROOT "/dev_hdd0/game/RVIC00001/USRDIR"
#define STATE_FILE INSTALL_ROOT "/data/state.txt"
#define VISIBLE_VIEWS 10
#define GRID_COLS 4
#define GRID_ROWS 3
#define GRID_PAGE (GRID_COLS * GRID_ROWS)

#define PAD_SELECT 0
#define PAD_START 3
#define PAD_UP 4
#define PAD_RIGHT 5
#define PAD_DOWN 6
#define PAD_LEFT 7
#define PAD_TRIANGLE 12
#define PAD_CIRCLE 13
#define PAD_CROSS 14
#define PAD_SQUARE 15

extern void rh_draw_text(SDL_Surface*,int,int,const char*,int,Uint32,int);

static const char *roots[] = {
    INSTALL_ROOT,
    "/dev_usb000/RETROVICIOS", "/dev_usb001/RETROVICIOS", "/dev_usb002/RETROVICIOS", "/dev_usb003/RETROVICIOS",
    "/dev_usb004/RETROVICIOS", "/dev_usb005/RETROVICIOS", "/dev_usb006/RETROVICIOS", "/dev_usb007/RETROVICIOS"
};

static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture *g_texture = NULL;

/* Keep the large catalog off the PS3 process stack. */
static RHCatalog g_catalog;
static RHState g_state;
static int g_indices[RH_MAX_FILTERED];

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
    SDL_Rect r={(Sint16)x,(Sint16)y,(Uint16)w,(Uint16)h};
    SDL_FillRect(s,&r,c);
}

static void frame_rect(SDL_Surface *s,int x,int y,int w,int h,int t,Uint32 c)
{
    fill_rect(s,x,y,w,t,c); fill_rect(s,x,y+h-t,w,t,c);
    fill_rect(s,x,y,t,h,c); fill_rect(s,x+w-t,y,t,h,c);
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
        "NES","SNES","SMS","GAME GEAR","GENESIS","SEGA CD","GB/GBC","GBA",
        "ATARI 2600","ATARI 7800","LYNX","PC ENGINE","NG POCKET","WONDERSWAN",
        "VIRTUAL BOY","VECTREX","MSX","ARCADE"
    };
    if(system_index < 0 || system_index >= (int)(sizeof(shorts)/sizeof(shorts[0]))) return "RETRO";
    return shorts[system_index];
}

static Uint32 system_color(SDL_Surface *screen,int system_index,int variant)
{
    static const unsigned char rgb[][3] = {
        {0,226,255},{255,47,205},{0,255,174},{255,134,47},{135,93,255},{255,55,98},
        {67,208,255},{255,217,64},{255,78,78},{98,255,96},{43,225,190},{68,133,255},
        {255,106,209},{174,255,77},{255,83,180},{66,238,255},{120,197,255},{255,101,39}
    };
    int n=(int)(sizeof(rgb)/sizeof(rgb[0]));
    int i=system_index;
    int r,g,b;
    if(i<0)i=0; i%=n;
    r=rgb[i][0]; g=rgb[i][1]; b=rgb[i][2];
    if(variant==1){r=(r+40>255)?255:r+40; g=(g+40>255)?255:g+40; b=(b+40>255)?255:b+40;}
    if(variant==2){r/=3;g/=3;b/=3;}
    return SDL_MapRGB(screen->format,(Uint8)r,(Uint8)g,(Uint8)b);
}

static void rebuild_filter(const RHCatalog *cat,const RHState *st,int view,int *indices,size_t *count,int *sel)
{
    *count=rh_filter_catalog(cat,st,filter_for_view(view),indices,RH_MAX_FILTERED);
    if(*count==0)*sel=0;
    else if(*sel<0)*sel=0;
    else if((size_t)*sel>=*count)*sel=(int)*count-1;
}

static void draw_scanlines(SDL_Surface *screen,Uint32 line)
{
    int y;
    for(y=0;y<WINDOW_H;y+=4) fill_rect(screen,0,y,WINDOW_W,1,line);
}

static void draw_sunset(SDL_Surface *screen,int cx,int cy,Uint32 pink,Uint32 purple)
{
    int i;
    for(i=0;i<9;i++){
        int w=150-i*12;
        fill_rect(screen,cx-w/2,cy+i*5,w,4,(i&1)?purple:pink);
    }
}

static void draw_skyline(SDL_Surface *screen,int x,int y,int w,int h,Uint32 c,Uint32 lights)
{
    int i;
    for(i=0;i<18;i++){
        int bw=12+(i*7)%21;
        int bh=18+(i*19)%58;
        int bx=x+(i*37)%(w-20);
        int by=y+h-bh;
        int ly;
        fill_rect(screen,bx,by,bw,bh,c);
        for(ly=by+7;ly<y+h-4;ly+=11) fill_rect(screen,bx+4,ly,2,2,lights);
    }
}

static void draw_neon_title(SDL_Surface *screen,Uint32 cyan,Uint32 pink,Uint32 white)
{
    rh_draw_text(screen,40,24,"RETRO",5,cyan,0);
    rh_draw_text(screen,190,24,"VICIOS",5,pink,0);
    rh_draw_text(screen,42,66,"JUEGA. REVIVI. REPETI.",1,white,0);
}

static void draw_cover_card(SDL_Surface *screen,const RHGame *g,int x,int y,int w,int h,int selected,int favorite)
{
    Uint32 dark=SDL_MapRGB(screen->format,10,16,29);
    Uint32 dark2=SDL_MapRGB(screen->format,18,24,40);
    Uint32 white=SDL_MapRGB(screen->format,235,245,255);
    Uint32 muted=SDL_MapRGB(screen->format,128,154,180);
    Uint32 cyan=SDL_MapRGB(screen->format,0,226,255);
    Uint32 pink=SDL_MapRGB(screen->format,255,39,193);
    Uint32 c=system_color(screen,g->system_index,0);
    Uint32 c2=system_color(screen,g->system_index,2);
    char line1[16]={0},line2[16]={0};
    size_t len=strlen(g->name),n1=len>14?14:len,n2=0;
    memcpy(line1,g->name,n1); line1[n1]=0;
    if(len>14){n2=len-14;if(n2>14)n2=14;memcpy(line2,g->name+14,n2);line2[n2]=0;}

    fill_rect(screen,x,y,w,h,dark);
    frame_rect(screen,x,y,w,h,2,selected?cyan:c2);
    if(selected) frame_rect(screen,x+3,y+3,w-6,h-6,2,pink);

    fill_rect(screen,x+8,y+8,w-16,h-52,c2);
    fill_rect(screen,x+12,y+12,w-24,12,c);
    fill_rect(screen,x+14,y+30,w-28,h-90,dark2);
    draw_sunset(screen,x+w/2,y+42,pink,c);
    draw_skyline(screen,x+16,y+50,w-32,h-102,dark,cyan);

    rh_draw_text(screen,x+14,y+h-41,line1,2,white,14);
    if(line2[0]) rh_draw_text(screen,x+14,y+h-24,line2,1,muted,14);
    else rh_draw_text(screen,x+14,y+h-22,system_short_name(g->system_index),1,muted,15);
    if(favorite) rh_draw_text(screen,x+w-20,y+10,"*",2,pink,1);
}

static void draw_preview(SDL_Surface *screen,const RHCatalog *cat,const RHState *st,const int *indices,size_t filtered,int game_sel,Uint32 panel,Uint32 cyan,Uint32 pink,Uint32 white,Uint32 muted)
{
    const int x=884,y=112,w=376,h=516;
    fill_rect(screen,x,y,w,h,panel);
    frame_rect(screen,x,y,w,h,2,cyan);
    rh_draw_text(screen,x+18,y+16,"VISTA PREVIA",2,cyan,0);

    if(filtered==0){
        rh_draw_text(screen,x+28,y+90,"SIN JUEGOS",3,muted,0);
        rh_draw_text(screen,x+28,y+128,"COPIA TUS ROMS Y",2,muted,0);
        rh_draw_text(screen,x+28,y+150,"PULSA TRIANGULO",2,muted,0);
        return;
    }

    {
        const RHGame *g=&cat->games[indices[game_sel]];
        Uint32 c=system_color(screen,g->system_index,0);
        Uint32 c2=system_color(screen,g->system_index,2);
        char info[96];
        int fav=rh_state_is_favorite(st,g->path);

        fill_rect(screen,x+18,y+50,w-36,224,c2);
        frame_rect(screen,x+18,y+50,w-36,224,2,pink);
        draw_sunset(screen,x+w/2,y+77,pink,c);
        draw_skyline(screen,x+26,y+114,w-52,150,SDL_MapRGB(screen->format,8,12,24),cyan);
        rh_draw_text(screen,x+34,y+168,g->name,3,white,18);
        rh_draw_text(screen,x+34,y+204,system_short_name(g->system_index),2,cyan,18);
        if(fav) rh_draw_text(screen,x+w-60,y+65,"*",3,pink,1);

        rh_draw_text(screen,x+20,y+294,g->name,3,cyan,19);
        snprintf(info,sizeof(info),"SISTEMA  %s",system_short_name(g->system_index));
        rh_draw_text(screen,x+20,y+330,info,2,white,28);
        snprintf(info,sizeof(info),"ESTADO   %s",rh_core_available_ps3(g->system_index)?"LISTO":"FALTA CORE");
        rh_draw_text(screen,x+20,y+357,info,2,rh_core_available_ps3(g->system_index)?cyan:pink,28);
        rh_draw_text(screen,x+20,y+395,"UBICACION",1,muted,0);
        rh_draw_text(screen,x+20,y+413,g->path,1,muted,48);
        rh_draw_text(screen,x+20,y+454,"X JUGAR",2,white,0);
        rh_draw_text(screen,x+156,y+454,"CUADRADO FAVORITO",1,pink,0);
    }
}

static void draw_ui(SDL_Surface *screen,const RHCatalog *cat,const RHState *st,int view,int focus,int game_sel,const int *indices,size_t filtered,const char *status)
{
    Uint32 bg=SDL_MapRGB(screen->format,3,7,16);
    Uint32 panel=SDL_MapRGB(screen->format,8,14,27);
    Uint32 panel2=SDL_MapRGB(screen->format,15,24,42);
    Uint32 cyan=SDL_MapRGB(screen->format,0,226,255);
    Uint32 pink=SDL_MapRGB(screen->format,255,39,193);
    Uint32 purple=SDL_MapRGB(screen->format,120,56,255);
    Uint32 white=SDL_MapRGB(screen->format,235,245,255);
    Uint32 muted=SDL_MapRGB(screen->format,125,151,180);
    Uint32 scan=SDL_MapRGB(screen->format,5,12,22);
    int i,vf,row;
    int first=(game_sel/GRID_PAGE)*GRID_PAGE;
    char buf[128];

    SDL_FillRect(screen,NULL,bg);
    draw_sunset(screen,1010,15,pink,purple);
    draw_skyline(screen,780,24,420,62,SDL_MapRGB(screen->format,4,10,20),cyan);
    draw_neon_title(screen,cyan,pink,white);

    snprintf(buf,sizeof(buf),"%d JUEGOS",(int)cat->count);
    rh_draw_text(screen,1035,26,buf,2,white,0);
    snprintf(buf,sizeof(buf),"%d SISTEMAS",(int)rh_system_count);
    rh_draw_text(screen,1035,52,buf,1,cyan,0);
    snprintf(buf,sizeof(buf),"%d FAVORITOS",(int)st->favorite_count);
    rh_draw_text(screen,1145,52,buf,1,pink,0);

    fill_rect(screen,18,106,224,522,panel);
    frame_rect(screen,18,106,224,522,2,purple);
    rh_draw_text(screen,38,124,"BIBLIOTECA",2,cyan,0);

    vf=0;
    if(view>=VISIBLE_VIEWS)vf=view-VISIBLE_VIEWS+1;
    for(row=0;row<VISIBLE_VIEWS && vf+row<view_count();++row){
        int v=vf+row;
        int yy=160+row*43;
        if(v==view){
            fill_rect(screen,28,yy-8,204,34,focus==0?SDL_MapRGB(screen->format,74,18,103):panel2);
            frame_rect(screen,28,yy-8,204,34,2,focus==0?pink:cyan);
        }
        rh_draw_text(screen,44,yy,view_name(v),2,white,17);
    }
    if(vf>0)rh_draw_text(screen,210,142,"^",1,cyan,1);
    if(vf+VISIBLE_VIEWS<view_count())rh_draw_text(screen,210,600,"V",1,cyan,1);

    fill_rect(screen,254,106,616,522,panel);
    frame_rect(screen,254,106,616,522,2,cyan);
    rh_draw_text(screen,274,124,view_name(view),3,cyan,22);
    snprintf(buf,sizeof(buf),"%d JUEGOS",(int)filtered);
    rh_draw_text(screen,758,132,buf,1,pink,0);

    for(i=0;i<GRID_PAGE && first+i<(int)filtered;i++){
        int pos=first+i;
        int col=i%GRID_COLS,rowg=i/GRID_COLS;
        int gx=272+col*148;
        int gy=166+rowg*146;
        const RHGame *g=&cat->games[indices[pos]];
        draw_cover_card(screen,g,gx,gy,136,132,pos==game_sel && focus==1,rh_state_is_favorite(st,g->path));
    }
    if(filtered==0){
        rh_draw_text(screen,342,300,"NO HAY JUEGOS EN ESTA VISTA",2,muted,0);
        rh_draw_text(screen,368,334,"TRIANGULO PARA REESCANEAR",2,cyan,0);
    }

    draw_preview(screen,cat,st,indices,filtered,game_sel,panel,cyan,pink,white,muted);

    fill_rect(screen,18,646,1242,56,panel);
    frame_rect(screen,18,646,1242,56,2,cyan);
    rh_draw_text(screen,48,666,"X",2,cyan,1); rh_draw_text(screen,70,668,"JUGAR",1,white,0);
    rh_draw_text(screen,170,666,"O",2,pink,1); rh_draw_text(screen,192,668,"VOLVER",1,white,0);
    rh_draw_text(screen,300,666,"[]",2,purple,2); rh_draw_text(screen,330,668,"FAVORITO",1,white,0);
    rh_draw_text(screen,452,666,"A",2,cyan,1); rh_draw_text(screen,474,668,"REESCANEAR",1,white,0);
    if(status && status[0]){
        fill_rect(screen,720,659,515,28,panel2);
        rh_draw_text(screen,736,668,status,1,cyan,62);
    }

    draw_scanlines(screen,scan);
    present_screen(screen);
}

static void rescan(RHCatalog *cat,const RHState *st,int view,int *indices,size_t *filtered,int *game_sel,char *status,size_t status_n)
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
    SDL_Surface *screen; SDL_Event ev; SDL_Joystick *joy=0;
    RHCatalog *catalog=&g_catalog; RHState *state=&g_state; int *indices=g_indices; size_t filtered=0;
    int view=0,focus=0,game_sel=0,running=1; Uint32 last_axis=0;
    char status[96]="";
    (void)argc;(void)argv;

    boot_log("Retrovicios v1.2 boot");
    SDL_SetMainReady();
    if(SDL_Init(SDL_INIT_VIDEO)<0){boot_log("SDL video init failed");return 1;}
    boot_log("SDL2 video init OK");

    g_window=SDL_CreateWindow("Retrovicios",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,
                              WINDOW_W,WINDOW_H,0);
    if(!g_window){
        boot_log("1280x720 window failed; trying 720x480");
        g_window=SDL_CreateWindow("Retrovicios",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,
                                  720,480,0);
    }
    if(!g_window){boot_log("SDL_CreateWindow failed");SDL_Quit();return 2;}
    boot_log("Window OK");

    g_renderer=SDL_CreateRenderer(g_window,-1,0);
    if(!g_renderer){boot_log("SDL_CreateRenderer failed");SDL_DestroyWindow(g_window);SDL_Quit();return 3;}
    SDL_RenderSetLogicalSize(g_renderer,WINDOW_W,WINDOW_H);
    boot_log("Renderer OK");

    g_texture=SDL_CreateTexture(g_renderer,SDL_PIXELFORMAT_RGB888,SDL_TEXTUREACCESS_STREAMING,
                                WINDOW_W,WINDOW_H);
    if(!g_texture){boot_log("SDL_CreateTexture failed");SDL_DestroyRenderer(g_renderer);SDL_DestroyWindow(g_window);SDL_Quit();return 4;}

    {
        Uint32 fmt=0;
        int access=0,tw=0,th=0;
        if(SDL_QueryTexture(g_texture,&fmt,&access,&tw,&th)!=0) fmt=SDL_PIXELFORMAT_RGB888;
        screen=SDL_CreateRGBSurfaceWithFormat(0,WINDOW_W,WINDOW_H,SDL_BITSPERPIXEL(fmt),fmt);
    }
    if(!screen){boot_log("SDL_CreateRGBSurfaceWithFormat failed");SDL_DestroyTexture(g_texture);SDL_DestroyRenderer(g_renderer);SDL_DestroyWindow(g_window);SDL_Quit();return 5;}
    boot_log("Framebuffer OK");

    if(SDL_InitSubSystem(SDL_INIT_JOYSTICK)==0 && SDL_NumJoysticks()>0){
        joy=SDL_JoystickOpen(0);
        if(joy) SDL_JoystickEventState(SDL_ENABLE);
    }
    boot_log("Input init complete");

    rh_state_load(state,STATE_FILE);
    rescan(catalog,state,view,indices,&filtered,&game_sel,status,sizeof(status));
    draw_ui(screen,catalog,state,view,focus,game_sel,indices,filtered,status);

    while(running){
        while(SDL_PollEvent(&ev)){
            int nav=0,confirm=0,back=0,favorite=0,scan_req=0,left=0,right=0;
            if(ev.type==SDL_QUIT)running=0;
            else if(ev.type==SDL_KEYDOWN){
                if(ev.key.keysym.sym==SDLK_UP)nav=-1; else if(ev.key.keysym.sym==SDLK_DOWN)nav=1;
                else if(ev.key.keysym.sym==SDLK_LEFT)left=1; else if(ev.key.keysym.sym==SDLK_RIGHT)right=1;
                else if(ev.key.keysym.sym==SDLK_RETURN||ev.key.keysym.sym==SDLK_SPACE)confirm=1;
                else if(ev.key.keysym.sym==SDLK_ESCAPE)back=1; else if(ev.key.keysym.sym==SDLK_f)favorite=1;
                else if(ev.key.keysym.sym==SDLK_r)scan_req=1;
            } else if(ev.type==SDL_JOYBUTTONDOWN){
                if(ev.jbutton.button==PAD_UP)nav=-1; else if(ev.jbutton.button==PAD_DOWN)nav=1;
                else if(ev.jbutton.button==PAD_LEFT)left=1; else if(ev.jbutton.button==PAD_RIGHT)right=1;
                else if(ev.jbutton.button==PAD_CROSS)confirm=1; else if(ev.jbutton.button==PAD_CIRCLE)back=1;
                else if(ev.jbutton.button==PAD_SQUARE)favorite=1; else if(ev.jbutton.button==PAD_TRIANGLE)scan_req=1;
                else if(ev.jbutton.button==PAD_START && filtered)confirm=1;
            } else if(ev.type==SDL_JOYAXISMOTION && SDL_GetTicks()-last_axis>180){
                if(ev.jaxis.axis==1){
                    if(ev.jaxis.value<-18000){nav=-1;last_axis=SDL_GetTicks();}
                    else if(ev.jaxis.value>18000){nav=1;last_axis=SDL_GetTicks();}
                } else if(ev.jaxis.axis==0){
                    if(ev.jaxis.value<-18000){left=1;last_axis=SDL_GetTicks();}
                    else if(ev.jaxis.value>18000){right=1;last_axis=SDL_GetTicks();}
                }
            }

            if(nav){
                if(focus==0){
                    view+=nav;if(view<0)view=view_count()-1;if(view>=view_count())view=0;
                    game_sel=0;rebuild_filter(catalog,state,view,indices,&filtered,&game_sel);
                } else {
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
            if(back){if(focus==1)focus=0;else running=0;}
            if(confirm){
                if(focus==0){focus=1;}
                else if(filtered){
                    RHGame *g=&catalog->games[indices[game_sel]];
                    if(!rh_core_available_ps3(g->system_index)){
                        snprintf(status,sizeof(status),"FALTA CORE: %.70s",rh_system_core(g->system_index));
                    } else {
                        rh_state_touch_recent(state,g->path); rh_state_save(state,STATE_FILE);
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
    if(joy)SDL_JoystickClose(joy);
    if(screen)SDL_FreeSurface(screen);
    if(g_texture)SDL_DestroyTexture(g_texture);
    if(g_renderer)SDL_DestroyRenderer(g_renderer);
    if(g_window)SDL_DestroyWindow(g_window);
    SDL_Quit();
    return 0;
}
