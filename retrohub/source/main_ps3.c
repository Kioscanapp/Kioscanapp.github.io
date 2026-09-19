/* RetroHub PS3 v1.0 - controller-first frontend shell. */
#include "retrohub.h"

#include <stdio.h>
#include <string.h>

#ifdef __PSL1GHT__
#include <SDL/SDL.h>
#else
#error "main_ps3.c is intended for PSL1GHT/PS3 builds"
#endif

#define WINDOW_W 1280
#define WINDOW_H 720
#define INSTALL_ROOT "/dev_hdd0/game/RHUB00001/USRDIR"
#define STATE_FILE INSTALL_ROOT "/data/state.txt"
#define VISIBLE_ROWS 15
#define VISIBLE_VIEWS 10

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
    "/dev_usb000/RETROHUB", "/dev_usb001/RETROHUB", "/dev_usb002/RETROHUB", "/dev_usb003/RETROHUB",
    "/dev_usb004/RETROHUB", "/dev_usb005/RETROHUB", "/dev_usb006/RETROHUB", "/dev_usb007/RETROHUB"
};

static void fill_rect(SDL_Surface *s,int x,int y,int w,int h,Uint32 c){SDL_Rect r={(Sint16)x,(Sint16)y,(Uint16)w,(Uint16)h};SDL_FillRect(s,&r,c);}
static int view_count(void){return 3+(int)rh_system_count;}
static RHFilter filter_for_view(int v){RHFilter f; if(v==0){f.kind=RH_VIEW_ALL;f.system_index=-1;}else if(v==1){f.kind=RH_VIEW_FAVORITES;f.system_index=-1;}else if(v==2){f.kind=RH_VIEW_RECENTS;f.system_index=-1;}else{f.kind=RH_VIEW_SYSTEM;f.system_index=v-3;}return f;}
static const char *view_name(int v){if(v==0)return "TODOS";if(v==1)return "FAVORITOS";if(v==2)return "RECIENTES";return rh_system_name(v-3);}

static void rebuild_filter(const RHCatalog *cat,const RHState *st,int view,int *indices,size_t *count,int *sel)
{
    *count=rh_filter_catalog(cat,st,filter_for_view(view),indices,RH_MAX_FILTERED);
    if(*count==0)*sel=0; else if(*sel<0)*sel=0; else if((size_t)*sel>=*count)*sel=(int)*count-1;
}

static void draw_ui(SDL_Surface *screen,const RHCatalog *cat,const RHState *st,int view,int focus,int game_sel,const int *indices,size_t filtered,const char *status)
{
    Uint32 bg=SDL_MapRGB(screen->format,15,17,23), panel=SDL_MapRGB(screen->format,31,35,45);
    Uint32 panel2=SDL_MapRGB(screen->format,40,45,58), active=SDL_MapRGB(screen->format,225,65,45);
    Uint32 text=SDL_MapRGB(screen->format,235,238,244), muted=SDL_MapRGB(screen->format,160,168,182);
    Uint32 yellow=SDL_MapRGB(screen->format,244,198,62), good=SDL_MapRGB(screen->format,90,200,120);
    int i,first=0;
    char buf[192];

    SDL_FillRect(screen,NULL,bg);
    fill_rect(screen,32,28,1216,74,panel);
    rh_draw_text(screen,58,48,"RETROHUB PS3",4,text,0);
    snprintf(buf,sizeof(buf),"V1.0   %d JUEGOS",(int)cat->count);
    rh_draw_text(screen,870,55,buf,2,muted,0);

    fill_rect(screen,32,122,298,520,panel);
    fill_rect(screen,350,122,898,520,panel);
    rh_draw_text(screen,54,142,"BIBLIOTECA",2,muted,0);
    rh_draw_text(screen,375,142,view_name(view),2,muted,32);

    {
        int vf = 0, row;
        if (view >= VISIBLE_VIEWS) vf = view - VISIBLE_VIEWS + 1;
        for(row=0; row<VISIBLE_VIEWS && vf+row<view_count(); ++row){
            i = vf + row;
            { int y=178+row*42;
              if(i==view)fill_rect(screen,46,y-8,270,34,focus==0?active:panel2);
              rh_draw_text(screen,60,y,view_name(i),2,text,20); }
        }
        if(vf>0) rh_draw_text(screen,286,156,"^",1,muted,1);
        if(vf+VISIBLE_VIEWS<view_count()) rh_draw_text(screen,286,610,"V",1,muted,1);
    }

    if(game_sel>=VISIBLE_ROWS) first=game_sel-VISIBLE_ROWS+1;
    for(i=0;i<VISIBLE_ROWS && first+i<(int)filtered;++i){
        int pos=first+i, gi=indices[pos], y=180+i*29;
        const RHGame *g=&cat->games[gi];
        if(pos==game_sel)fill_rect(screen,366,y-7,864,25,focus==1?active:panel2);
        if(rh_state_is_favorite(st,g->path))rh_draw_text(screen,378,y,"*",2,yellow,1);
        rh_draw_text(screen,400,y,g->name,2,text,45);
        rh_draw_text(screen,1050,y,rh_system_name(g->system_index),1,muted,24);
    }
    if(filtered==0)rh_draw_text(screen,390,230,"NO HAY JUEGOS EN ESTA VISTA",3,muted,0);

    fill_rect(screen,32,658,1216,42,panel);
    rh_draw_text(screen,52,672,"X ABRIR   O VOLVER   CUADRADO FAVORITO   TRIANGULO REESCANEAR",1,text,0);
    if(status && status[0]){
        fill_rect(screen,760,610,470,26,panel2);
        rh_draw_text(screen,775,618,status,1,good,54);
    }
    SDL_Flip(screen);
}

static void rescan(RHCatalog *cat,const RHState *st,int view,int *indices,size_t *filtered,int *game_sel,char *status,size_t status_n)
{
    int n=rh_catalog_scan_roots(cat,roots,sizeof(roots)/sizeof(roots[0]));
    rebuild_filter(cat,st,view,indices,filtered,game_sel);
    snprintf(status,status_n,"ESCANEO COMPLETO: %d JUEGOS",n);
}

int main(int argc,char **argv)
{
    SDL_Surface *screen; SDL_Event ev; SDL_Joystick *joy=0;
    RHCatalog catalog; RHState state; int indices[RH_MAX_FILTERED]; size_t filtered=0;
    int view=0,focus=0,game_sel=0,running=1; Uint32 last_axis=0;
    char status[96]="";
    (void)argc;(void)argv;

    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK)<0)return 1;
    screen=SDL_SetVideoMode(WINDOW_W,WINDOW_H,32,SDL_HWSURFACE|SDL_DOUBLEBUF);
    if(!screen){SDL_Quit();return 2;}
    if(SDL_NumJoysticks()>0){joy=SDL_JoystickOpen(0);SDL_JoystickEventState(SDL_ENABLE);}

    rh_state_load(&state,STATE_FILE);
    rescan(&catalog,&state,view,indices,&filtered,&game_sel,status,sizeof(status));
    draw_ui(screen,&catalog,&state,view,focus,game_sel,indices,filtered,status);

    while(running){
        while(SDL_PollEvent(&ev)){
            int nav=0,confirm=0,back=0,favorite=0,scan=0;
            if(ev.type==SDL_QUIT)running=0;
            else if(ev.type==SDL_KEYDOWN){
                if(ev.key.keysym.sym==SDLK_UP)nav=-1; else if(ev.key.keysym.sym==SDLK_DOWN)nav=1;
                else if(ev.key.keysym.sym==SDLK_LEFT){focus=0;} else if(ev.key.keysym.sym==SDLK_RIGHT){focus=1;}
                else if(ev.key.keysym.sym==SDLK_RETURN||ev.key.keysym.sym==SDLK_SPACE)confirm=1;
                else if(ev.key.keysym.sym==SDLK_ESCAPE)back=1; else if(ev.key.keysym.sym==SDLK_f)favorite=1;
                else if(ev.key.keysym.sym==SDLK_r)scan=1;
            } else if(ev.type==SDL_JOYBUTTONDOWN){
                if(ev.jbutton.button==PAD_UP)nav=-1; else if(ev.jbutton.button==PAD_DOWN)nav=1;
                else if(ev.jbutton.button==PAD_LEFT)focus=0; else if(ev.jbutton.button==PAD_RIGHT)focus=1;
                else if(ev.jbutton.button==PAD_CROSS)confirm=1; else if(ev.jbutton.button==PAD_CIRCLE)back=1;
                else if(ev.jbutton.button==PAD_SQUARE)favorite=1; else if(ev.jbutton.button==PAD_TRIANGLE)scan=1;
                else if(ev.jbutton.button==PAD_START && filtered)confirm=1;
            } else if(ev.type==SDL_JOYAXISMOTION && ev.jaxis.axis==1 && SDL_GetTicks()-last_axis>180){
                if(ev.jaxis.value<-18000){nav=-1;last_axis=SDL_GetTicks();}
                else if(ev.jaxis.value>18000){nav=1;last_axis=SDL_GetTicks();}
            }

            if(nav){
                if(focus==0){view+=nav;if(view<0)view=view_count()-1;if(view>=view_count())view=0;game_sel=0;rebuild_filter(&catalog,&state,view,indices,&filtered,&game_sel);}
                else if(filtered){game_sel+=nav;if(game_sel<0)game_sel=(int)filtered-1;if((size_t)game_sel>=filtered)game_sel=0;}
            }
            if(back){if(focus==1)focus=0;else running=0;}
            if(confirm){
                if(focus==0){focus=1;}
                else if(filtered){
                    RHGame *g=&catalog.games[indices[game_sel]];
                    if(!rh_core_available_ps3(g->system_index)){snprintf(status,sizeof(status),"FALTA CORE: %.70s",rh_system_core(g->system_index));}
                    else{
                        rh_state_touch_recent(&state,g->path); rh_state_save(&state,STATE_FILE);
                        snprintf(status,sizeof(status),"ABRIENDO %.65s",g->name); draw_ui(screen,&catalog,&state,view,focus,game_sel,indices,filtered,status);
                        rh_launch_game_ps3(g);
                    }
                }
            }
            if(favorite && focus==1 && filtered){
                RHGame *g=&catalog.games[indices[game_sel]]; int added=rh_state_toggle_favorite(&state,g->path); rh_state_save(&state,STATE_FILE);
                snprintf(status,sizeof(status),added>0?"AGREGADO A FAVORITOS":"QUITADO DE FAVORITOS");
                rebuild_filter(&catalog,&state,view,indices,&filtered,&game_sel);
            }
            if(scan)rescan(&catalog,&state,view,indices,&filtered,&game_sel,status,sizeof(status));
            draw_ui(screen,&catalog,&state,view,focus,game_sel,indices,filtered,status);
        }
        SDL_Delay(8);
    }
    rh_state_save(&state,STATE_FILE);
    if(joy) SDL_JoystickClose(joy);
    SDL_Quit();
    return 0;
}
