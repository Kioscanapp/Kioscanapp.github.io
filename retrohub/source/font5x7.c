#include <ctype.h>
#include <stdint.h>
#include <string.h>

#ifdef __PSL1GHT__
#include <SDL.h>
#endif

/* Tiny built-in 5x7 font. Each glyph is 7 rows of 5 bits. */
static const uint8_t *glyph(char c)
{
    static const uint8_t blank[7]={0,0,0,0,0,0,0};
    static const uint8_t qmark[7]={14,17,1,2,4,0,4};
    static const uint8_t A[7]={14,17,17,31,17,17,17}; static const uint8_t B[7]={30,17,17,30,17,17,30};
    static const uint8_t C[7]={14,17,16,16,16,17,14}; static const uint8_t D[7]={30,17,17,17,17,17,30};
    static const uint8_t E[7]={31,16,16,30,16,16,31}; static const uint8_t F[7]={31,16,16,30,16,16,16};
    static const uint8_t G[7]={14,17,16,23,17,17,14}; static const uint8_t H[7]={17,17,17,31,17,17,17};
    static const uint8_t I[7]={14,4,4,4,4,4,14}; static const uint8_t J[7]={7,2,2,2,2,18,12};
    static const uint8_t K[7]={17,18,20,24,20,18,17}; static const uint8_t L[7]={16,16,16,16,16,16,31};
    static const uint8_t M[7]={17,27,21,21,17,17,17}; static const uint8_t N[7]={17,25,21,19,17,17,17};
    static const uint8_t O[7]={14,17,17,17,17,17,14}; static const uint8_t P[7]={30,17,17,30,16,16,16};
    static const uint8_t Q[7]={14,17,17,17,21,18,13}; static const uint8_t R[7]={30,17,17,30,20,18,17};
    static const uint8_t S[7]={15,16,16,14,1,1,30}; static const uint8_t T[7]={31,4,4,4,4,4,4};
    static const uint8_t U[7]={17,17,17,17,17,17,14}; static const uint8_t V[7]={17,17,17,17,17,10,4};
    static const uint8_t W[7]={17,17,17,21,21,21,10}; static const uint8_t X[7]={17,17,10,4,10,17,17};
    static const uint8_t Y[7]={17,17,10,4,4,4,4}; static const uint8_t Z[7]={31,1,2,4,8,16,31};
    static const uint8_t n0[7]={14,17,19,21,25,17,14}; static const uint8_t n1[7]={4,12,4,4,4,4,14};
    static const uint8_t n2[7]={14,17,1,2,4,8,31}; static const uint8_t n3[7]={30,1,1,14,1,1,30};
    static const uint8_t n4[7]={2,6,10,18,31,2,2}; static const uint8_t n5[7]={31,16,16,30,1,1,30};
    static const uint8_t n6[7]={14,16,16,30,17,17,14}; static const uint8_t n7[7]={31,1,2,4,8,8,8};
    static const uint8_t n8[7]={14,17,17,14,17,17,14}; static const uint8_t n9[7]={14,17,17,15,1,1,14};
    static const uint8_t dash[7]={0,0,0,31,0,0,0}; static const uint8_t dot[7]={0,0,0,0,0,12,12};
    static const uint8_t colon[7]={0,12,12,0,12,12,0}; static const uint8_t slash[7]={1,2,2,4,8,8,16};
    static const uint8_t lbr[7]={14,8,8,8,8,8,14}; static const uint8_t rbr[7]={14,2,2,2,2,2,14};
    static const uint8_t star[7]={0,21,14,31,14,21,0};

    c=(char)toupper((unsigned char)c);
    if(c>='A'&&c<='Z') { const uint8_t *letters[]={A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z}; return letters[c-'A']; }
    if(c>='0'&&c<='9') { const uint8_t *nums[]={n0,n1,n2,n3,n4,n5,n6,n7,n8,n9}; return nums[c-'0']; }
    if(c==' ') return blank;
    if(c=='-') return dash;
    if(c=='.') return dot;
    if(c==':') return colon;
    if(c=='/') return slash;
    if(c=='[') return lbr;
    if(c==']') return rbr;
    if(c=='*') return star;
    return qmark;
}

#ifdef __PSL1GHT__
static void px(SDL_Surface *s,int x,int y,int w,int h,Uint32 color){SDL_Rect r={(Sint16)x,(Sint16)y,(Uint16)w,(Uint16)h};SDL_FillRect(s,&r,color);}

void rh_draw_text(SDL_Surface *s,int x,int y,const char *text,int scale,Uint32 color,int max_chars)
{
    int i=0;
    while(*text && (max_chars<=0 || i<max_chars)) {
        const uint8_t *g=glyph(*text++); int row,col;
        for(row=0;row<7;row++) for(col=0;col<5;col++) if(g[row]&(1<<(4-col))) px(s,x+col*scale,y+row*scale,scale,scale,color);
        x += 6*scale; i++;
    }
}
#endif
