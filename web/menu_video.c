#include "menu_video.h"
#include "save.h"
#define NONMATCHING
static u8 gPlayStChapterBits[4],gPlayStChapterMode[4];
static struct PlaySt_OptionBits gPlayStOptionBits[4];
#include "generated/menu_assets.h"
#include "generated/menu_font.h"
#include "generated/chapter_save_titles.h"
#include <string.h>
static u8 bgVram[65536],objVram[32768],rgba[240*160*4];
static u16 bgMap[1024],fogMap[1024],colors[240*160],fogLines[160];
static s16 matrix[32][4];
static int fogY,errors,difficultyScreen,difficultySelection,confirmChoice,confirmOperation,confirmCopy;
static u16 panelMap[1024];
struct Sprite {int layer,x,y,base;const u16 *layout;};
static struct Sprite sprites[256];static int spriteCount;
static void Palette(const u16 *src,int bank,int count){memcpy(gPaletteBuffer+bank*16,src,count*32);}
// TmApplyTsa in asm/arm.s writes packed TSA rows from bottom to top.
static void Tsa(u16 *map,const u8 *src,int base){
 int width=src[0]+1,height=src[1]+1,p=2;
 memset(map,0,2048);
 if(width>32||height>32){errors++;return;}
 for(int y=height-1;y>=0;y--)for(int x=0;x<width;x++,p+=2)
  map[y*32+x]=(src[p]|src[p+1]<<8)+base;
}
void menu_video_reset(void){
 memset(bgVram,0,sizeof(bgVram));memset(objVram,0,sizeof(objVram));
 memset(bgMap,0,sizeof(bgMap));memset(fogMap,0,sizeof(fogMap));
 memset(fogLines,0,sizeof(fogLines));fogY=errors=spriteCount=0;confirmChoice=0;
 for(int i=0;i<32;i++)SetObjAffine(i,256,0,0,256);
}
static void ApplyPalette(const u16 *p,int bank){Palette(p,bank,1);}
static void EnablePaletteSync(void){}
static const u16 *gUnknown_08A07B0A=gPal_08A07AD8+0x19,*gUnknown_08A07AEA=gPal_08A07AD8+9;
static const u16 *gUnknown_08A07C0A=gPal_08A07AD8+0x99,*gUnknown_08A07BEA=gPal_08A07AD8+0x89;
#include "generated/save_palette_routines.h"
void menu_slot_palette(int selected){
 for(int i=0;i<3;i++){struct PlaySt p;fe8_save_read_playst(i,&p);gPlayStChapterBits[i]=p.chapterStateBits;gPlayStChapterMode[i]=p.chapterModeIndex;gPlayStOptionBits[i]=p.config;}
 SaveMenuInitSlotPalette(selected);
}
void menu_slot_hard_palette(int frame,int selected){SaveDrawSetDifficultSlotPalette(frame,selected);}
void menu_video_load(int difficulty){
 menu_video_reset();fe8_palette_reset();Palette(menuTextPalette,4,1);difficultyScreen=difficulty;memset(panelMap,0,sizeof(panelMap));
 memcpy(bgVram+0x8000,Img_SaveMenuBG,sizeof(Img_SaveMenuBG));
 memcpy(bgVram+0xcc00,Img_MainMenuBgFog,sizeof(Img_MainMenuBgFog));
 Tsa(bgMap,Tsa_SaveMenuBG,0x8000);Tsa(fogMap,Tsa_MainMenuBgFog,0x7260);
 Palette(Pal_SaveMenuBG,8,8);Palette(Pal_MainMenuBgFog,7,1);
 Palette(Pal_SaveScreenSprits,18,8);Palette(Pal_SaveScreenSprits,17,1);
 Palette(Pal_08A295B4,2,1);
 memcpy(objVram+0x800,Img_SaveScreenSprits,sizeof(Img_SaveScreenSprits));
 memcpy(objVram+0x4000,Img_GameMainMenuObjs,sizeof(Img_GameMainMenuObjs));
 memcpy(objVram+0x5800,gGfx_08A09E4C,sizeof(gGfx_08A09E4C));
 for(int i=0;i<3;i++)memcpy(objVram+0x6800+i*0x800,empty_title,sizeof(empty_title));
 menu_slot_palette(0);
 if(difficulty){
  memcpy(bgVram,gUiFrameImage,sizeof(gUiFrameImage));Palette(gUiFramePaletteA,1,1);Palette(menuTextPalette,0,1);gPaletteBuffer[0]=0;
  const u8 *tsa=gUnknown_08A29558;int p=2;
  for(int y=tsa[1];y>=0;y--)for(int x=0;x<=tsa[0];x++,p+=2)
    if(x+17<32&&y+6<32)panelMap[(y+6)*32+x+17]=(tsa[p]|tsa[p+1]<<8)+0x1000;

  memcpy(objVram+0x800,Img_DifficultyMenuObjs,sizeof(Img_DifficultyMenuObjs));
  // ROM palettes following Pal_DifficultyMenuObjs hold active/inactive pairs.
  Palette(Pal_DifficultyMenuObjs,17,4);Palette(gUnknown_08A29498,21,6);
 }
 SetBackdropColor(0);
}
void menu_video_begin(void){spriteCount=0;}
void menu_video_fog(const u16 *lines,int y){memcpy(fogLines,lines,320);fogY=y;}
void SetObjAffine(int i,int pa,int pb,int pc,int pd){if(i<0||i>=32){errors++;return;}matrix[i][0]=pa;matrix[i][1]=pb;matrix[i][2]=pc;matrix[i][3]=pd;}
void PutSpriteExt(int layer,int x,int y,const u16 *layout,int oam2){
 if(spriteCount>=256){errors++;return;}sprites[spriteCount++]=(struct Sprite){layer,x,y,oam2,layout};
}
static int TilePixel(const u8 *vram,int limit,int tile,int x,int y){
 int p=tile*32+y*4+x/2;if(p<0||p>=limit){errors++;return 0;}
 return (vram[p]>>((x&1)*4))&15;
}
static int BgPixel(const u16 *map,int x,int y){
 x&=255;y&=255;int entry=map[(y/8)*32+x/8];
 int sx=(entry&1024)?7-x%8:x%8,sy=(entry&2048)?7-y%8:y%8;
 int index=TilePixel(bgVram+0x8000,32768,entry&1023,sx,sy);
 return index?gPaletteBuffer[((entry>>12)&15)*16+index]:-1;
}
static u16 Blend(int top,int bottom,int a,int b){
 int color=0;for(int i=0;i<3;i++){int c=(((top>>(i*5))&31)*a+((bottom>>(i*5))&31)*b)>>4;if(c>31)c=31;color|=c<<(i*5);}return color;
}
static void DrawSprite(const struct Sprite *s,int priority){
 static const int widths[3][4]={{8,16,32,64},{16,32,32,64},{8,8,16,32}};
 static const int heights[3][4]={{8,16,32,64},{8,8,16,32},{16,32,32,64}};
 // Within one layout, the first OAM entry has precedence.
 for(int i=s->layout[0]-1;i>=0;i--){
  int a=s->layout[i*3+1]|(s->y&~255),b=s->layout[i*3+2]|(s->x&~511),c=s->layout[i*3+3]+s->base;
  if(((c>>10)&3)!=priority)continue;
  int shape=a>>14,size=(b>>14)&3;if(shape>=3||(a&8192)){errors++;continue;}
  int affine=a&256;if(!affine&&(a&512))continue;
  int w=widths[shape][size],h=heights[shape][size],dw=affine&&(a&512)?2*w:w,dh=affine&&(a&512)?2*h:h;
  int ox=((s->x&511)+(b&511))&511,oy=((s->y&255)+(a&255))&255;
  if(ox>=256)ox-=512;if(oy>=160)oy-=256;
  for(int y=0;y<dh;y++)for(int x=0;x<dw;x++){
   int dx=ox+x,dy=oy+y;if(dx<0||dx>=240||dy<0||dy>=160)continue;
   int sx=x,sy=y;
   if(affine){s16 *m=matrix[(b>>9)&31];sx=((m[0]*(x-dw/2)+m[1]*(y-dh/2))>>8)+w/2;sy=((m[2]*(x-dw/2)+m[3]*(y-dh/2))>>8)+h/2;}
   else{if(b&4096)sx=w-1-x;if(b&8192)sy=h-1-y;}
   if(sx<0||sx>=w||sy<0||sy>=h)continue;
   int tile=(c&1023)+(sy/8)*32+sx/8;
   int index=TilePixel(objVram,sizeof(objVram),tile,sx%8,sy%8);
   if(index)colors[dy*240+dx]=gPaletteBuffer[256+((c>>12)&15)*16+index];
  }
 }
}
void menu_video_finish_frame(void){}
void menu_video_difficulty_selection(int selection){difficultySelection=selection;}
static void TextLine(const char *s,int x,int y,int bank){
 int origin=x;
 for(;*s;s++){
  if(*s=='\n'){x=origin;y+=16;continue;}
  int code=(u8)*s;const u32 *rows=menuGlyphs[code].rows;
  for(int sy=0;sy<16;sy++)for(int sx=0;sx<16;sx++){
   int index=(rows[sy]>>(sx*2))&3;
   if(index&&x+sx<240&&y+sy<160)colors[(y+sy)*240+x+sx]=gPaletteBuffer[bank*16+index];
  }
  x+=menuGlyphs[code].width;
 }
}
static void DifficultyText(void){TextLine(difficultyDescriptions[difficultySelection%3],144,56,0);}

u8 *menu_video_pixels(void){
 for(int y=0;y<160;y++)for(int x=0;x<240;x++){
  int bg=BgPixel(bgMap,x,y);if(bg<0)bg=gPaletteBuffer[0];
  int fog=BgPixel(fogMap,x+fogLines[y],y+fogY);
  colors[y*240+x]=fog<0?bg:Blend(fog,bg,6,16);
 }
 for(int priority=3;priority>=0;priority--)for(int layer=15;layer>=0;layer--)
  for(int i=0;i<spriteCount;i++)if(sprites[i].layer==layer)DrawSprite(sprites+i,priority);
 if(difficultyScreen){
  for(int y=0;y<160;y++)for(int x=0;x<240;x++){
   int entry=panelMap[(y/8)*32+x/8];
   int sx=(entry&1024)?7-x%8:x%8,sy=(entry&2048)?7-y%8:y%8;
   int index=TilePixel(bgVram,32768,entry&1023,sx,sy);
   if(index)colors[y*240+x]=gPaletteBuffer[((entry>>12)&15)*16+index];
  }
  DifficultyText();
 }
 if(confirmChoice){
  const char *label=confirmOperation==8?"Erase":confirmOperation==4?"Copy":"Start";
  // Text palette is independent from OBJ panels, matching the original subbox.
  TextLine(label,56,136,4);TextLine("Cancel",100,136,4);
 }
 for(int i=0;i<240*160;i++){
  int color=fe8_display_enabled()?colors[i]:0;
  for(int c=0;c<3;c++){int v=(color>>(c*5))&31;rgba[i*4+c]=(v<<3)|(v>>2);}rgba[i*4+3]=255;
 }
 return rgba;
}
int menu_video_errors(void){return errors;}
void menu_video_test_scene(int mode){
 menu_video_reset();fe8_palette_reset();difficultyScreen=0;memset(panelMap,0,sizeof(panelMap));
 gPaletteBuffer[257]=31;gPaletteBuffer[258]=31744;gPaletteBuffer[273]=992;
 memset(objVram,0x11,32);static const u16 square[]={1,0,0,0};
 static const u16 affine[]={1,256,0,0};
 if(mode==2||mode==3)for(int y=0;y<8;y++)for(int x=2;x<4;x++)objVram[y*4+x]=0x22;
 if(mode==6)for(int y=0;y<8;y++)for(int x=0;x<2;x++)objVram[y*4+x]=0;
 if(mode==5)PutSpriteExt(4,510,254,square,0);
 else PutSpriteExt(4,mode==2?4|4096:4,4,mode==3?affine:square,mode==4?1024:0);
 if(mode==1)PutSpriteExt(4,6,6,square,4096);
 if(mode==4)PutSpriteExt(4,6,6,square,4096|3072);
}

u8 *menu_video_obj_buffer(void){return objVram;}


void menu_video_slot_title(int slot,int title){
 if(slot<0||slot>=3)return;
 if(title<0||title>=sizeof(saveTitleBitmaps)/sizeof(saveTitleBitmaps[0]))title=0x54;
 memcpy(objVram+0x6800+slot*0x800,saveTitleBitmaps[title],2048);
}
void menu_video_confirmation(int choice,int operation,int copyDestination){confirmChoice=choice;confirmOperation=operation;confirmCopy=copyDestination;}
