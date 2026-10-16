#include "gameover.h"
#include "proc.h"
#include "palette.h"
#include "save.h"
#include "audio.h"
#include "map.h"
#include "constants/songs.h"
#include <string.h>
#define gPlaySt (*fe8_save_loaded_playst())
#include "generated/gameover_assets.h"
static int nativeState,screenStarted,clockTicks,errors,pendingConfirm,nextGameAction=-1;
static u8 pixels[240*160*4],vram[32768];
static u16 gBG0TilemapBuffer[1024],gBG2TilemapBuffer[1024],gBG3TilemapBuffer[1024];
static int scrollX[4],scrollY[4],REG_VCOUNT,REG_BLDCA,REG_BLDCB;
static struct {struct {int priority;} bg0cnt,bg1cnt,bg2cnt,bg3cnt;} gLCDControlBuffer;
static struct {u16 newKeys;} keysNow,*gKeyStatusPtr=&keysNow;
enum {BG_2=2,BG_3=3,A_BUTTON=1,B_BUTTON=2,START_BUTTON=8,DISPLAY_HEIGHT=160,BGCHR_GAMEOVER_TEXT=0x80,BGCHR_BMFX_IMG=0x100,BGPAL_GAMEOVER_TEXT=0,BGPAL_GAMEOVER_4=4,BG2_SYNC_BIT=4,BG3_SYNC_BIT=8,GAME_ACTION_EVENT_RETURN=0};
#define BG_CHR_ADDR(tile) (vram+(tile)*32)
#define PAL_BG_COLOR(bank,color) gPaletteBuffer[(bank)*16+(color)]
#define PAL_BACKDROP_OFFSET 0
#define TILEMAP_INDEX(x,y) ((y)*32+(x))
#define TILEMAP_LOCATED(map,x,y) ((map)+TILEMAP_INDEX(x,y))
#define TILEREF(tile,pal) ((tile)|((pal)<<12))
static void BMapDispSuspend(void){} // Native screen owns output while the tactical state is paused.
static void StartBgm(int song,int unused){(void)unused;fe8_audio_bgm(song,0);}
static void Sound_FadeOutBGM(int speed){fe8_audio_fadeout(speed);}
static int GetGameClock(void){return clockTicks;}
static void BG_SetPosition(int bg,int x,int y){scrollX[bg]=x;scrollY[bg]=y;}
static void SetBackgroundTileDataOffset(int bg,int offset){(void)bg;(void)offset;}
static void Decompress(const u8 *src,u8 *dst){size_t count=src==Img_GameOverText?sizeof(Img_GameOverText):src==Img_ChapterIntroFog?sizeof(Img_ChapterIntroFog):0;if(!count||dst<vram||dst>vram+sizeof(vram)-count){errors++;return;}memcpy(dst,src,count);}
static void ApplyPalette(const u8 *src,int bank){for(int i=0;i<16;i++)gPaletteBuffer[bank*16+i]=src[i*2]|src[i*2+1]<<8;}
static void ClearBg0Bg1(void){memset(gBG0TilemapBuffer,0,sizeof(gBG0TilemapBuffer));}
static void CallARM_FillTileRect(u16 *dst,const u8 *tsa,int base){int w=tsa[0]+1,h=tsa[1]+1,p=2;for(int y=h-1;y>=0;y--)for(int x=0;x<w;x++){if(p+1>=sizeof(Tsa_GameOverFx)||dst+y*32+x>=gBG0TilemapBuffer+1024){errors++;return;}dst[y*32+x]=(tsa[p]|tsa[p+1]<<8)+base;p+=2;}}
static void BG_EnableSyncByMask(int mask){(void)mask;}
static void SetPrimaryHBlankHandler(void (*callback)(void)){(void)callback;}
static void SetSecondaryHBlankHandler(void (*callback)(void)){(void)callback;}
static void SetBlendConfig(int a,int b,int c,int d){(void)a;(void)b;(void)c;(void)d;}
static void SetBlendTargetB(int a,int b,int c,int d,int e){(void)a;(void)b;(void)c;(void)d;(void)e;}
static void EnablePaletteSync(void){} // Renderer reads the shared native palette directly.
extern void CALLARM_ColorFadeTick(void);
#include "generated/gameover_palette.h"
static void NativeEndAllMus(ProcPtr p){(void)p;}
#include "generated/gameover_screen.h"
_Static_assert(sizeof(struct ProcGameOverScroll)<=sizeof(struct Proc),"Game Over scroll Proc fits native pool");
_Static_assert(sizeof(struct ProcGameOverScreen)<=sizeof(struct Proc),"Game Over screen Proc fits native pool");
extern const struct ProcCmd ProcScr_FadeToBlack[];
static void NativeSlowFade(ProcPtr p){(void)p;struct Proc *fade=Proc_Start(ProcScr_FadeToBlack,PROC_TREE_5);fade->unk64=4;}
static void NativeStartScreen(ProcPtr p){StartGameOverScreen(p);}
static void SetNextGameActionId(int id){nextGameAction=id;} // Controller consumes the completed EVENT_RETURN via its safe reset/title boundary.
static void EndBMapMain(void){nativeState=5;}
static void DeleteEventEngines(void){} // Game controller resets event/tactical state after original Game Over finishes.
#include "generated/gameover_flow.h"
void fe8_gameover_reset(void){Proc_EndEach(ProcScr_BmGameOver);Proc_EndEach(ProcScr_GameOverScreen);nativeState=screenStarted=clockTicks=errors=pendingConfirm=0;nextGameAction=-1;keysNow.newKeys=0;memset(scrollX,0,sizeof(scrollX));memset(scrollY,0,sizeof(scrollY));}
int fe8_gameover_start(void){if(nativeState)return -1;nativeState=1;Proc_Start(ProcScr_BmGameOver,PROC_TREE_5);return 0;}
int fe8_gameover_confirm(void){if(nativeState!=3)return -1;pendingConfirm=1;return 0;}
void fe8_gameover_tick(int keys){clockTicks++;keysNow.newKeys=keys|pendingConfirm;pendingConfirm=0;if(nativeState>0&&nativeState<5)Proc_Run(gProcTreeRootArray[5]);}
int fe8_gameover_active(void){return nativeState>0&&nativeState<5;}
int fe8_gameover_info(int f){struct ProcGameOverScreen *p=Proc_Find(ProcScr_GameOverScreen);return f==0?nativeState:f==1?clockTicks:f==2?(p?p->counter1:-1):f==3?(p?p->counter2:-1):f==4?scrollX[2]:f==5?scrollY[2]:f==6?scrollX[3]:f==7?scrollY[3]:f==8?errors:f==9?nextGameAction:-1;}
static int LayerPixel(const u16 *map,int x,int y,int sx,int sy,int *opaque){x=(x+sx)&255;y=(y+sy)&255;int t=map[(y/8)*32+x/8],dx=t&1024?7-(x&7):x&7,dy=t&2048?7-(y&7):y&7,pos=(t&1023)*32+dy*4+dx/2;if(pos>=sizeof(vram)){errors++;*opaque=0;return 0;}int ink=vram[pos]>>((dx&1)*4)&15;*opaque=ink!=0;return gPaletteBuffer[(t>>12)*16+ink];}
u8 *fe8_gameover_pixels(void){
 if(!screenStarted){memcpy(pixels,fe8_map_pixels(),sizeof(pixels));return pixels;}
 for(int y=0;y<160;y++){REG_VCOUNT=y?y-1:227;GameOverScreenHBlank();for(int x=0;x<240;x++){int a,b,c,front=LayerPixel(gBG0TilemapBuffer,x,y,0,0,&a),upper=LayerPixel(gBG2TilemapBuffer,x,y,scrollX[2],scrollY[2],&b),lower=LayerPixel(gBG3TilemapBuffer,x,y,scrollX[3],scrollY[3],&c);for(int k=0;k<3;k++){int v=a?(front>>(k*5)&31):b&&c?((upper>>(k*5)&31)*REG_BLDCA+(lower>>(k*5)&31)*REG_BLDCB)>>4:b?(upper>>(k*5)&31):c?(lower>>(k*5)&31):gPaletteBuffer[0]>>(k*5)&31;if(v>31)v=31;pixels[(y*240+x)*4+k]=(v<<3)|(v>>2);}pixels[(y*240+x)*4+3]=255;}}
 return pixels;
}
