/* Retrovicios PS3 v1.8 - lightweight modern frontend. */
#include "retrohub.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef __PSL1GHT__
#include <SDL.h>
#include <sysutil/sysutil.h>
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
#define LIST_ROWS 8
#define SEARCH_MAX_RESULTS 7
#define SEARCH_QUERY_MAX 28
#define SEARCH_KEY_COLS 8

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
#define PAD_R1 12
#define PAD_L1 13
#define PAD_R2 14

extern void rh_draw_text(SDL_Surface*,int,int,const char*,int,Uint32,int);

static const char *roots[] = {
    INSTALL_ROOT,
    "/dev_usb000/RETROVICIOS", "/dev_usb001/RETROVICIOS", "/dev_usb002/RETROVICIOS", "/dev_usb003/RETROVICIOS",
    "/dev_usb004/RETROVICIOS", "/dev_usb005/RETROVICIOS", "/dev_usb006/RETROVICIOS", "/dev_usb007/RETROVICIOS"
};

static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture *g_texture = NULL;

static RHCatalog g_catalog;
static RHState g_state;
static int g_indices[RH_MAX_FILTERED];

/* Only one cover is kept in memory. This avoids the slowdown of v1.7. */
static SDL_Surface *g_selected_cover = NULL;
static char g_selected_cover_game[RH_PATH_MAX];
static volatile int g_xmb_exit_requested=0;
static int g_search_results[SEARCH_MAX_RESULTS];
static int g_search_scores[SEARCH_MAX_RESULTS];
static size_t g_search_count=0;
static int g_search_sel=0;
static int g_search_key=0;
static char g_search_query[SEARCH_QUERY_MAX+1]="";
static const char g_search_keys[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -";

static void rh_sysutil_callback(u64 status,u64 param,void *userdata)
{
    (void)param;
    (void)userdata;
    if(status==SYSUTIL_EXIT_GAME)g_xmb_exit_requested=1;
}

static void boot_log(const char *message)
{
    FILE *f=fopen(INSTALL_ROOT "/data/boot.log","a");
    if(f){fprintf(f,"%s\n",message);fclose(f);}
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

static int present_screen(SDL_Surface *screen)
{
    if(!screen || !g_renderer || !g_texture)return -1;
    if(SDL_UpdateTexture(g_texture,NULL,screen->pixels,screen->pitch)!=0)return -2;
    SDL_RenderClear(g_renderer);
    if(SDL_RenderCopy(g_renderer,g_texture,NULL,NULL)!=0)return -3;
    SDL_RenderPresent(g_renderer);
    return 0;
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
    if(system_index<0 || system_index>=(int)(sizeof(shorts)/sizeof(shorts[0])))return "RETRO";
    return shorts[system_index];
}

static Uint32 system_color(SDL_Surface *screen,int system_index,int dark)
{
    static const unsigned char rgb[][3]={
        {48,155,255},{138,103,255},{45,205,169},{245,150,66},{62,139,245},{242,78,104},
        {53,183,242},{244,192,56},{235,91,91},{92,205,104},{51,194,176},{61,126,238},
        {192,91,216},{145,205,78},{219,84,161},{59,194,220},{80,158,219},{234,114,48}
    };
    int n=(int)(sizeof(rgb)/sizeof(rgb[0]));
    int i=system_index;
    int r,g,b;
    if(i<0)i=0;
    i%=n;
    r=rgb[i][0];g=rgb[i][1];b=rgb[i][2];
    if(dark){r/=3;g/=3;b/=3;}
    return SDL_MapRGB(screen->format,(Uint8)r,(Uint8)g,(Uint8)b);
}

static void rebuild_filter(const RHCatalog *cat,const RHState *st,int view,int *indices,size_t *count,int *sel)
{
    *count=rh_filter_catalog(cat,st,filter_for_view(view),indices,RH_MAX_FILTERED);
    if(*count==0)*sel=0;
    else if(*sel<0)*sel=0;
    else if((size_t)*sel>=*count)*sel=(int)*count-1;
}

static int rh_char_upper(int c)
{
    if(c>='a'&&c<='z')return c-'a'+'A';
    return c;
}

static int rh_search_score(const char *name,const char *query)
{
    char nbuf[RH_NAME_MAX];
    char qbuf[SEARCH_QUERY_MAX+1];
    size_t ni=0,qi=0,i,j;
    int pos=-1,match=0,first=-1;

    if(!name || !query || !query[0])return 0;

    for(i=0;name[i] && ni+1<sizeof(nbuf);++i){
        unsigned char ch=(unsigned char)name[i];
        if(isalnum(ch) || ch==' ' || ch=='-' || ch=='_')
            nbuf[ni++]=(char)rh_char_upper(ch);
    }
    nbuf[ni]='\0';

    for(i=0;query[i] && qi+1<sizeof(qbuf);++i){
        unsigned char ch=(unsigned char)query[i];
        if(isalnum(ch) || ch==' ' || ch=='-' || ch=='_')
            qbuf[qi++]=(char)rh_char_upper(ch);
    }
    qbuf[qi]='\0';
    if(!qbuf[0])return 0;

    {
        char *p=strstr(nbuf,qbuf);
        if(p){
            pos=(int)(p-nbuf);
            if(pos==0)return 5000-(int)(ni-qi);
            return 4200-pos*20-(int)(ni-qi);
        }
    }

    j=0;
    for(i=0;nbuf[i] && qbuf[j];++i){
        if(nbuf[i]==qbuf[j]){
            if(first<0)first=(int)i;
            match+=12;
            ++j;
        }
    }
    if(qbuf[j])return 0;

    return 1800+match-(first<0?0:first*6)-(int)(ni-qi);
}

static void rh_build_search(const RHCatalog *cat)
{
    size_t i;
    int r;

    g_search_count=0;
    g_search_sel=0;
    for(r=0;r<SEARCH_MAX_RESULTS;++r){
        g_search_results[r]=-1;
        g_search_scores[r]=-1;
    }

    if(!g_search_query[0])return;

    for(i=0;i<cat->count;++i){
        int score=rh_search_score(cat->games[i].name,g_search_query);
        int slot;
        if(score<=0)continue;

        for(slot=0;slot<SEARCH_MAX_RESULTS;++slot){
            if(score>g_search_scores[slot]){
                int k;
                for(k=SEARCH_MAX_RESULTS-1;k>slot;--k){
                    g_search_scores[k]=g_search_scores[k-1];
                    g_search_results[k]=g_search_results[k-1];
                }
                g_search_scores[slot]=score;
                g_search_results[slot]=(int)i;
                break;
            }
        }
    }

    for(r=0;r<SEARCH_MAX_RESULTS;++r)
        if(g_search_results[r]>=0)++g_search_count;
}

static void rh_search_add_char(char ch,const RHCatalog *cat)
{
    size_t n=strlen(g_search_query);
    if(n<SEARCH_QUERY_MAX){
        g_search_query[n]=ch;
        g_search_query[n+1]='\0';
        rh_build_search(cat);
    }
}

static void rh_search_delete(const RHCatalog *cat)
{
    size_t n=strlen(g_search_query);
    if(n){
        g_search_query[n-1]='\0';
        rh_build_search(cat);
    }
}

static void rh_search_clear(const RHCatalog *cat)
{
    g_search_query[0]='\0';
    rh_build_search(cat);
}

static void draw_search(SDL_Surface *screen,const RHCatalog *cat)
{
    Uint32 bg=SDL_MapRGB(screen->format,5,13,24);
    Uint32 panel=SDL_MapRGB(screen->format,11,25,42);
    Uint32 selected=SDL_MapRGB(screen->format,28,60,91);
    Uint32 white=SDL_MapRGB(screen->format,240,246,252);
    Uint32 muted=SDL_MapRGB(screen->format,132,155,177);
    Uint32 blue=SDL_MapRGB(screen->format,60,166,255);
    Uint32 keybg=SDL_MapRGB(screen->format,17,34,53);
    int i;
    char title[96];

    fill_rect(screen,0,0,WINDOW_W,WINDOW_H,bg);
    rh_draw_text(screen,34,24,"BUSCAR JUEGO",4,white,0);
    rh_draw_text(screen,36,70,"L1/R1 CAMBIAN RESULTADO - O VOLVER",2,muted,0);

    fill_rect(screen,34,108,760,74,panel);
    frame_rect(screen,34,108,760,74,2,blue);
    snprintf(title,sizeof(title),"BUSCAR: %s%s",g_search_query,g_search_query[0]?"":"_");
    rh_draw_text(screen,52,132,title,3,white,28);

    fill_rect(screen,34,202,760,430,panel);
    frame_rect(screen,34,202,760,430,1,SDL_MapRGB(screen->format,39,67,91));
    rh_draw_text(screen,52,220,"RESULTADOS",3,blue,0);

    if(g_search_count==0){
        rh_draw_text(screen,72,292,g_search_query[0]?"SIN COINCIDENCIAS":"ESCRIBI PARTE DEL NOMBRE",3,muted,0);
    }else{
        for(i=0;i<(int)g_search_count;++i){
            const RHGame *g=&cat->games[g_search_results[i]];
            int y=266+i*49;
            Uint32 accent=system_color(screen,g->system_index,0);
            if(i==g_search_sel){
                fill_rect(screen,52,y-9,722,42,selected);
                fill_rect(screen,52,y-9,5,42,accent);
            }
            rh_draw_text(screen,70,y,g->name,i==g_search_sel?3:2,white,31);
            rh_draw_text(screen,605,y+4,system_short_name(g->system_index),2,accent,13);
        }
    }

    fill_rect(screen,814,108,432,524,panel);
    frame_rect(screen,814,108,432,524,1,SDL_MapRGB(screen->format,39,67,91));
    rh_draw_text(screen,834,126,"TECLADO",3,white,0);

    for(i=0;i<(int)strlen(g_search_keys);++i){
        int col=i%SEARCH_KEY_COLS;
        int row=i/SEARCH_KEY_COLS;
        int x=836+col*49;
        int y=176+row*58;
        char s[2]={g_search_keys[i],0};
        fill_rect(screen,x,y,40,44,i==g_search_key?selected:keybg);
        frame_rect(screen,x,y,40,44,i==g_search_key?2:1,i==g_search_key?blue:SDL_MapRGB(screen->format,49,73,96));
        rh_draw_text(screen,x+12,y+11,s,3,white,1);
    }

    rh_draw_text(screen,834,492,"X  ESCRIBIR",2,white,0);
    rh_draw_text(screen,834,526,"CUADRADO  BORRAR",2,white,0);
    rh_draw_text(screen,834,560,"TRIANGULO  LIMPIAR",2,white,0);
    rh_draw_text(screen,834,594,"START JUGAR   R2 VER EN LISTA",2,blue,0);

    fill_rect(screen,34,650,1212,48,SDL_MapRGB(screen->format,7,16,28));
    rh_draw_text(screen,52,665,"L1/R1 RESULTADO   R2 VER EN LISTA   O CERRAR",2,white,0);

    present_screen(screen);
}

static int path_without_extension(const char *path,char *out,size_t out_n)
{
    const char *base=strrchr(path,'/');
    const char *dot;
    size_t len;
    if(!out || out_n==0)return 0;
    base=base?base+1:path;
    dot=strrchr(base,'.');
    len=dot?(size_t)(dot-base):strlen(base);
    if(len>=out_n)len=out_n-1;
    memcpy(out,base,len);
    out[len]='\0';
    return (int)len;
}

static int find_cover_path(const RHGame *g,char *out,size_t out_n)
{
    static const char *exts[]={"jpg","jpeg","png","bmp"};
    const char *mark;
    const char *slash;
    char root[RH_PATH_MAX],dir[RH_PATH_MAX],base[RH_NAME_MAX];
    size_t root_len,dir_len,i;

    if(!g || !out || out_n==0)return 0;
    mark=strstr(g->path,"/roms/");
    slash=strrchr(g->path,'/');
    if(!mark || !slash)return 0;

    root_len=(size_t)(mark-g->path);
    if(root_len>=sizeof(root))return 0;
    memcpy(root,g->path,root_len);root[root_len]='\0';

    dir_len=(size_t)(slash-g->path);
    if(dir_len>=sizeof(dir))dir_len=sizeof(dir)-1;
    memcpy(dir,g->path,dir_len);dir[dir_len]='\0';

    path_without_extension(g->path,base,sizeof(base));

    for(i=0;i<sizeof(exts)/sizeof(exts[0]);++i){
        snprintf(out,out_n,"%s/covers/%s.%s",root,base,exts[i]);
        if(rh_file_exists(out))return 1;
    }
    for(i=0;i<sizeof(exts)/sizeof(exts[0]);++i){
        snprintf(out,out_n,"%s/covers/%s/%s.%s",root,rh_systems[g->system_index].id,base,exts[i]);
        if(rh_file_exists(out))return 1;
    }
    for(i=0;i<sizeof(exts)/sizeof(exts[0]);++i){
        snprintf(out,out_n,"%s/%s.%s",dir,base,exts[i]);
        if(rh_file_exists(out))return 1;
    }
    out[0]='\0';
    return 0;
}

static SDL_Surface *decode_cover(SDL_Surface *screen,const char *path)
{
    int w=0,h=0,n=0;
    unsigned char *pixels;
    SDL_Surface *raw=NULL,*converted=NULL,*scaled=NULL;
    SDL_Rect dst={0,0,256,352};

    pixels=stbi_load(path,&w,&h,&n,4);
    if(!pixels || w<=0 || h<=0)return NULL;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    raw=SDL_CreateRGBSurfaceFrom(pixels,w,h,32,w*4,0xFF000000,0x00FF0000,0x0000FF00,0x000000FF);
#else
    raw=SDL_CreateRGBSurfaceFrom(pixels,w,h,32,w*4,0x000000FF,0x0000FF00,0x00FF0000,0xFF000000);
#endif
    if(raw)converted=SDL_ConvertSurface(raw,screen->format,0);
    if(raw)SDL_FreeSurface(raw);
    stbi_image_free(pixels);
    if(!converted)return NULL;

    scaled=SDL_CreateRGBSurfaceWithFormat(0,256,352,screen->format->BitsPerPixel,screen->format->format);
    if(!scaled){SDL_FreeSurface(converted);return NULL;}
    SDL_FillRect(scaled,NULL,SDL_MapRGB(scaled->format,5,10,18));

    {
        float sr=(float)converted->w/(float)converted->h;
        float dr=256.0f/352.0f;
        SDL_Rect d;
        if(sr>dr){
            d.h=352;
            d.w=(int)(352.0f*sr);
        }else{
            d.w=256;
            d.h=(int)(256.0f/sr);
        }
        d.x=(256-d.w)/2;
        d.y=(352-d.h)/2;
        SDL_SetClipRect(scaled,&dst);
        SDL_BlitScaled(converted,NULL,scaled,&d);
        SDL_SetClipRect(scaled,NULL);
    }

    SDL_FreeSurface(converted);
    return scaled;
}

static SDL_Surface *selected_cover(SDL_Surface *screen,const RHGame *g)
{
    char cover_path[RH_PATH_MAX];

    if(!g)return NULL;
    if(strcmp(g_selected_cover_game,g->path)==0)return g_selected_cover;

    if(g_selected_cover){SDL_FreeSurface(g_selected_cover);g_selected_cover=NULL;}
    snprintf(g_selected_cover_game,sizeof(g_selected_cover_game),"%s",g->path);

    if(find_cover_path(g,cover_path,sizeof(cover_path)))
        g_selected_cover=decode_cover(screen,cover_path);

    return g_selected_cover;
}

static void draw_background(SDL_Surface *screen)
{
    int y;
    Uint32 top=SDL_MapRGB(screen->format,6,14,27);
    Uint32 mid=SDL_MapRGB(screen->format,8,20,36);
    Uint32 bottom=SDL_MapRGB(screen->format,4,10,19);

    fill_rect(screen,0,0,WINDOW_W,WINDOW_H,bottom);
    for(y=0;y<WINDOW_H;y+=12){
        Uint32 c=(y<240)?top:((y<520)?mid:bottom);
        fill_rect(screen,0,y,WINDOW_W,12,c);
    }
    fill_rect(screen,0,88,WINDOW_W,2,SDL_MapRGB(screen->format,36,87,126));
}

static void draw_logo(SDL_Surface *screen)
{
    Uint32 white=SDL_MapRGB(screen->format,240,245,250);
    Uint32 blue=SDL_MapRGB(screen->format,60,166,255);
    rh_draw_text(screen,30,25,"RETROVICIOS",4,white,0);
    fill_rect(screen,31,67,142,3,blue);
    rh_draw_text(screen,188,61,"PS3",1,blue,0);
}

static void draw_sidebar(SDL_Surface *screen,int view,int focus)
{
    Uint32 panel=SDL_MapRGB(screen->format,10,22,36);
    Uint32 selected=SDL_MapRGB(screen->format,19,45,70);
    Uint32 white=SDL_MapRGB(screen->format,238,244,250);
    Uint32 muted=SDL_MapRGB(screen->format,131,151,171);
    Uint32 blue=SDL_MapRGB(screen->format,60,166,255);
    int vf=0,row;

    fill_rect(screen,20,108,210,532,panel);
    frame_rect(screen,20,108,210,532,1,SDL_MapRGB(screen->format,34,61,84));
    rh_draw_text(screen,40,128,"BIBLIOTECA",2,white,0);

    if(view>=VISIBLE_VIEWS)vf=view-VISIBLE_VIEWS+1;
    for(row=0;row<VISIBLE_VIEWS && vf+row<view_count();++row){
        int v=vf+row;
        int yy=166+row*41;
        if(v==view){
            fill_rect(screen,30,yy-9,190,32,selected);
            fill_rect(screen,30,yy-9,4,32,blue);
        }
        rh_draw_text(screen,43,yy,view_name(v),2,v==view?white:muted,17);
    }
    if(focus==0)frame_rect(screen,20,108,210,532,2,blue);
}

static void draw_game_list(SDL_Surface *screen,const RHCatalog *cat,const RHState *st,
                           const int *indices,size_t filtered,int game_sel,int focus)
{
    Uint32 panel=SDL_MapRGB(screen->format,10,22,36);
    Uint32 row=SDL_MapRGB(screen->format,13,28,45);
    Uint32 selected=SDL_MapRGB(screen->format,25,53,81);
    Uint32 white=SDL_MapRGB(screen->format,238,244,250);
    Uint32 muted=SDL_MapRGB(screen->format,132,153,174);
    int first=(game_sel/LIST_ROWS)*LIST_ROWS;
    int i;

    fill_rect(screen,244,108,548,532,panel);
    frame_rect(screen,244,108,548,532,1,SDL_MapRGB(screen->format,34,61,84));

    for(i=0;i<LIST_ROWS && first+i<(int)filtered;++i){
        int pos=first+i;
        const RHGame *g=&cat->games[indices[pos]];
        int y=126+i*59;
        Uint32 accent=system_color(screen,g->system_index,0);
        char idxbuf[16];

        fill_rect(screen,258,y,520,51,pos==game_sel?selected:row);
        if(pos==game_sel)fill_rect(screen,258,y,5,51,accent);
        snprintf(idxbuf,sizeof(idxbuf),"%02d",pos+1);
        rh_draw_text(screen,272,y+17,idxbuf,2,muted,0);
        rh_draw_text(screen,306,y+11,g->name,3,white,19);
        rh_draw_text(screen,638,y+18,system_short_name(g->system_index),2,accent,12);
        if(rh_state_is_favorite(st,g->path))rh_draw_text(screen,754,y+13,"*",3,accent,1);
    }

    if(filtered==0){
        rh_draw_text(screen,330,305,"NO HAY JUEGOS",3,muted,0);
        rh_draw_text(screen,320,348,"TRIANGULO PARA REESCANEAR",1,muted,0);
    }

    if(focus==1)frame_rect(screen,244,108,548,532,2,SDL_MapRGB(screen->format,60,166,255));
}

static void draw_cover_placeholder(SDL_Surface *screen,const RHGame *g,int x,int y,int w,int h)
{
    Uint32 accent=system_color(screen,g->system_index,0);
    Uint32 dark=system_color(screen,g->system_index,1);
    Uint32 white=SDL_MapRGB(screen->format,240,245,250);
    int i;

    fill_rect(screen,x,y,w,h,dark);
    for(i=0;i<h;i+=22){
        if(((i/22)&1)==0)fill_rect(screen,x,y+i,w,8,accent);
    }
    fill_rect(screen,x+14,y+20,w-28,h-40,SDL_MapRGB(screen->format,7,15,26));
    rh_draw_text(screen,x+28,y+42,"RETROVICIOS",2,accent,16);
    rh_draw_text(screen,x+28,y+h/2-15,system_short_name(g->system_index),3,white,14);
    rh_draw_text(screen,x+28,y+h-66,g->name,1,white,21);
}

static void draw_preview(SDL_Surface *screen,const RHCatalog *cat,const RHState *st,
                         const int *indices,size_t filtered,int game_sel)
{
    const int x=808,y=108,w=452,h=532;
    Uint32 panel=SDL_MapRGB(screen->format,10,22,36);
    Uint32 white=SDL_MapRGB(screen->format,238,244,250);
    Uint32 muted=SDL_MapRGB(screen->format,132,153,174);

    fill_rect(screen,x,y,w,h,panel);
    frame_rect(screen,x,y,w,h,1,SDL_MapRGB(screen->format,34,61,84));

    if(filtered==0){
        rh_draw_text(screen,x+34,y+70,"SIN SELECCION",3,muted,0);
        return;
    }

    {
        const RHGame *g=&cat->games[indices[game_sel]];
        Uint32 accent=system_color(screen,g->system_index,0);
        SDL_Surface *cover=selected_cover(screen,g);
        SDL_Rect dst={x+22,y+22,256,352};
        char info[96];

        fill_rect(screen,x+20,y+20,260,356,SDL_MapRGB(screen->format,4,9,16));
        frame_rect(screen,x+20,y+20,260,356,2,accent);
        if(cover)SDL_BlitSurface(cover,NULL,screen,&dst);
        else draw_cover_placeholder(screen,g,x+22,y+22,256,352);

        rh_draw_text(screen,x+300,y+26,system_short_name(g->system_index),3,accent,12);
        rh_draw_text(screen,x+300,y+82,g->name,3,white,12);
        snprintf(info,sizeof(info),"%s",rh_core_available_ps3(g->system_index)?"LISTO":"FALTA CORE");
        rh_draw_text(screen,x+300,y+160,info,2,rh_core_available_ps3(g->system_index)?accent:SDL_MapRGB(screen->format,255,100,100),12);
        if(rh_state_is_favorite(st,g->path))rh_draw_text(screen,x+300,y+194,"FAVORITO",2,accent,12);

        fill_rect(screen,x+20,y+396,w-40,1,SDL_MapRGB(screen->format,34,61,84));
        rh_draw_text(screen,x+20,y+420,"X  JUGAR",2,white,0);
        rh_draw_text(screen,x+160,y+424,"CUADRADO  FAVORITO",1,muted,0);
        rh_draw_text(screen,x+20,y+464,"SELECT + START",2,accent,0);
        rh_draw_text(screen,x+224,y+468,"VOLVER AL MENU",1,muted,0);
        rh_draw_text(screen,x+20,y+503,"CARATULA: JPG / PNG / BMP",1,muted,0);
    }
}

static void draw_ui(SDL_Surface *screen,const RHCatalog *cat,const RHState *st,int view,int focus,
                    int game_sel,const int *indices,size_t filtered,const char *status)
{
    Uint32 white=SDL_MapRGB(screen->format,238,244,250);
    Uint32 muted=SDL_MapRGB(screen->format,132,153,174);
    Uint32 blue=SDL_MapRGB(screen->format,60,166,255);
    char buf[96];

    draw_background(screen);
    draw_logo(screen);

    snprintf(buf,sizeof(buf),"%d JUEGOS",(int)cat->count);
    rh_draw_text(screen,1036,27,buf,2,white,0);
    snprintf(buf,sizeof(buf),"%d FAVORITOS",(int)st->favorite_count);
    rh_draw_text(screen,1036,57,buf,1,blue,0);

    draw_sidebar(screen,view,focus);
    draw_game_list(screen,cat,st,indices,filtered,game_sel,focus);
    draw_preview(screen,cat,st,indices,filtered,game_sel);

    fill_rect(screen,20,655,1240,49,SDL_MapRGB(screen->format,7,16,28));
    frame_rect(screen,20,655,1240,49,1,SDL_MapRGB(screen->format,34,61,84));
    rh_draw_text(screen,38,668,"X JUGAR",2,white,0);
    rh_draw_text(screen,146,668,"O VOLVER",2,white,0);
    rh_draw_text(screen,270,668,"CUADRADO FAVORITO",2,white,0);
    rh_draw_text(screen,486,668,"TRIANGULO REESCANEAR",2,white,0);
    rh_draw_text(screen,742,668,"L1 BUSCAR",2,blue,0);
    if(status && status[0])rh_draw_text(screen,910,670,status,1,blue,42);
    else rh_draw_text(screen,1110,670,"SELECT SALIR",1,muted,0);

    present_screen(screen);
}

static void rescan(RHCatalog *cat,const RHState *st,int view,int *indices,size_t *filtered,
                   int *game_sel,char *status,size_t status_n)
{
    int n=rh_catalog_scan_roots(cat,roots,sizeof(roots)/sizeof(roots[0]));
    rebuild_filter(cat,st,view,indices,filtered,game_sel);
    g_selected_cover_game[0]='\0';
    snprintf(status,status_n,"ESCANEO COMPLETO: %d JUEGOS",n);
}

static void list_move(int delta,size_t filtered,int *game_sel)
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
    int view=0,focus=0,game_sel=0,running=1,search_mode=0;
    Uint32 last_axis=0;
    char status[96]="";
    (void)argc;(void)argv;

    boot_log("Retrovicios v2.4 boot");
    SDL_SetMainReady();
    if(SDL_Init(SDL_INIT_VIDEO)<0){boot_log("SDL video init failed");return 1;}
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0,rh_sysutil_callback,NULL);

    g_window=SDL_CreateWindow("Retrovicios",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,WINDOW_W,WINDOW_H,0);
    if(!g_window)g_window=SDL_CreateWindow("Retrovicios",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,720,480,0);
    if(!g_window){SDL_Quit();return 2;}

    g_renderer=SDL_CreateRenderer(g_window,-1,0);
    if(!g_renderer){SDL_DestroyWindow(g_window);SDL_Quit();return 3;}
    SDL_RenderSetLogicalSize(g_renderer,WINDOW_W,WINDOW_H);

    g_texture=SDL_CreateTexture(g_renderer,SDL_PIXELFORMAT_RGB888,SDL_TEXTUREACCESS_STREAMING,WINDOW_W,WINDOW_H);
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
        sysUtilCheckCallback();
        if(g_xmb_exit_requested){running=0;break;}
        while(SDL_PollEvent(&ev)){
            int nav=0,confirm=0,back=0,favorite=0,scan_req=0,left=0,right=0;
            int search_open=0,search_prev=0,search_next=0,search_launch=0,search_to_list=0;
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
                else if(ev.key.keysym.sym==SDLK_s)search_open=1;
            }else if(ev.type==SDL_JOYBUTTONDOWN){
                if(ev.jbutton.button==PAD_UP)nav=-1;
                else if(ev.jbutton.button==PAD_DOWN)nav=1;
                else if(ev.jbutton.button==PAD_LEFT)left=1;
                else if(ev.jbutton.button==PAD_RIGHT)right=1;
                else if(ev.jbutton.button==PAD_CROSS)confirm=1;
                else if(ev.jbutton.button==PAD_CIRCLE)back=1;
                else if(ev.jbutton.button==PAD_SQUARE)favorite=1;
                else if(ev.jbutton.button==PAD_TRIANGLE)scan_req=1;
                else if(ev.jbutton.button==PAD_L1){if(search_mode)search_prev=1;else search_open=1;}
                else if(ev.jbutton.button==PAD_R1){if(search_mode)search_next=1;}
                else if(ev.jbutton.button==PAD_R2){if(search_mode)search_to_list=1;}
                else if(ev.jbutton.button==PAD_SELECT){if(!search_mode){g_xmb_exit_requested=1;running=0;}}
                else if(ev.jbutton.button==PAD_START){if(search_mode)search_launch=1;else if(filtered)confirm=1;}
            }else if(ev.type==SDL_JOYAXISMOTION && SDL_GetTicks()-last_axis>180){
                if(ev.jaxis.axis==1){
                    if(ev.jaxis.value<-18000){nav=-1;last_axis=SDL_GetTicks();}
                    else if(ev.jaxis.value>18000){nav=1;last_axis=SDL_GetTicks();}
                }else if(ev.jaxis.axis==0){
                    if(ev.jaxis.value<-18000){left=1;last_axis=SDL_GetTicks();}
                    else if(ev.jaxis.value>18000){right=1;last_axis=SDL_GetTicks();}
                }
            }

            if(search_open && !search_mode){
                search_mode=1;
                g_search_key=0;
                g_search_sel=0;
                rh_build_search(catalog);
                draw_search(screen,catalog);
                continue;
            }

            if(search_mode){
                int key_count=(int)strlen(g_search_keys);
                int key_rows=(key_count+SEARCH_KEY_COLS-1)/SEARCH_KEY_COLS;

                if(back){
                    search_mode=0;
                    draw_ui(screen,catalog,state,view,focus,game_sel,indices,filtered,status);
                    continue;
                }

                if(search_prev && g_search_count){
                    status[0]='\0';
                    --g_search_sel;
                    if(g_search_sel<0)g_search_sel=(int)g_search_count-1;
                }
                if(search_next && g_search_count){
                    status[0]='\0';
                    ++g_search_sel;
                    if(g_search_sel>=(int)g_search_count)g_search_sel=0;
                }

                if(nav<0){
                    g_search_key-=SEARCH_KEY_COLS;
                    if(g_search_key<0)g_search_key+=(key_rows*SEARCH_KEY_COLS);
                    if(g_search_key>=key_count)g_search_key=key_count-1;
                }else if(nav>0){
                    g_search_key+=SEARCH_KEY_COLS;
                    if(g_search_key>=key_rows*SEARCH_KEY_COLS)g_search_key%=SEARCH_KEY_COLS;
                    if(g_search_key>=key_count)g_search_key=key_count-1;
                }
                if(left){
                    --g_search_key;
                    if(g_search_key<0)g_search_key=key_count-1;
                }
                if(right){
                    ++g_search_key;
                    if(g_search_key>=key_count)g_search_key=0;
                }

                if(favorite)rh_search_delete(catalog);
                if(scan_req)rh_search_clear(catalog);
                if(confirm)rh_search_add_char(g_search_keys[g_search_key],catalog);

                if(search_launch && g_search_count){
                    RHGame *g=&catalog->games[g_search_results[g_search_sel]];
                    if(!rh_core_available_ps3(g->system_index)){
                        snprintf(status,sizeof(status),"FALTA CORE: %.70s",rh_system_core(g->system_index));
                    }else{
                        rh_state_touch_recent(state,g->path);
                        rh_state_save(state,STATE_FILE);
                        {
                            int launch_rc=rh_launch_game_ps3(g);
                            if(launch_rc==-60) snprintf(status,sizeof(status),"SEGA CD: USA CUE/CHD/ISO, NO ZIP/7Z/BIN");
                            else if(launch_rc<0) snprintf(status,sizeof(status),"NO SE PUDO ABRIR (%d)",launch_rc);
                        }
                    }
                }

                if(search_to_list && g_search_count){
                    int target=g_search_results[g_search_sel];
                    RHGame *chosen=&catalog->games[target];
                    size_t k;

                    view=3+chosen->system_index;
                    game_sel=0;
                    rebuild_filter(catalog,state,view,indices,&filtered,&game_sel);
                    for(k=0;k<filtered;++k){
                        if(indices[k]==target){
                            game_sel=(int)k;
                            break;
                        }
                    }
                    focus=1;
                    search_mode=0;
                    status[0]='\0';
                    g_selected_cover_game[0]='\0';
                    draw_ui(screen,catalog,state,view,focus,game_sel,indices,filtered,status);
                    continue;
                }

                draw_search(screen,catalog);
                continue;
            }

            if(nav){
                status[0]='\0';
                if(focus==0){
                    view+=nav;
                    if(view<0)view=view_count()-1;
                    if(view>=view_count())view=0;
                    game_sel=0;
                    rebuild_filter(catalog,state,view,indices,&filtered,&game_sel);
                    g_selected_cover_game[0]='\0';
                }else{
                    list_move(nav,filtered,&game_sel);
                }
            }

            if(left){
                status[0]='\0';
                if(focus==1)list_move(-10,filtered,&game_sel);
            }
            if(right){
                status[0]='\0';
                if(focus==0)focus=1;
                else list_move(10,filtered,&game_sel);
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
                        {
                            int launch_rc=rh_launch_game_ps3(g);
                            if(launch_rc==-60) snprintf(status,sizeof(status),"SEGA CD: USA CUE/CHD/ISO, NO ZIP/7Z/BIN");
                            else if(launch_rc<0) snprintf(status,sizeof(status),"NO SE PUDO ABRIR EL JUEGO (%d)",launch_rc);
                        }
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
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    if(g_selected_cover)SDL_FreeSurface(g_selected_cover);
    if(joy)SDL_JoystickClose(joy);
    if(screen)SDL_FreeSurface(screen);
    if(g_texture)SDL_DestroyTexture(g_texture);
    if(g_renderer)SDL_DestroyRenderer(g_renderer);
    if(g_window)SDL_DestroyWindow(g_window);
    SDL_Quit();
    return 0;
}
