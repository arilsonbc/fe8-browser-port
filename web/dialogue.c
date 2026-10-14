#include "dialogue.h"
#include "audio.h"
#include <string.h>
extern int fe8_event_info(int field);
struct PortraitAsset {int id;const u8 *image;int bytes;const u16 *palette;const u8 *mouth;int xMouth,yMouth;};
struct DialoguePage {int id,page;const char *text;int active;u8 faces[8],smiles[8];};
#include "generated/dialogue_assets.h"
static u16 bubbleMap[1024];
#define TILEMAP_INDEX_UNK(x,y) ((y)*32+(x))
#define TILEREF(tile,pal) ((tile)|((pal)<<12))
static u16 *BG_GetMapBuffer(int bg){(void)bg;return bubbleMap;}
#include "generated/dialogue_routines.h"
static const int facePositions[]={3,6,9,21,24,27,-8,38};
static const struct DialoguePage *current;
static int visible,clock,errors;
static const struct DialoguePage *FindPage(int id,int page){for(int i=0;i<sizeof(dialoguePages)/sizeof(*dialoguePages);i++)if(dialoguePages[i].id==id&&dialoguePages[i].page==page)return dialoguePages+i;return NULL;}
extern int fe8_event_actor_portrait(void);
static const struct PortraitAsset *FindPortrait(int id){if(id==255)id=fe8_event_actor_portrait();for(int i=0;i<sizeof(portraits)/sizeof(*portraits);i++)if(portraits[i].id==id)return portraits+i;return NULL;}
static void SyncPage(void){const struct DialoguePage *next=FindPage(fe8_event_info(2),fe8_event_info(3));if(next!=current){current=next;visible=clock=0;}}
void fe8_dialogue_reset(void){current=NULL;visible=clock=errors=0;}
// Original TalkMain sound ID (SONG_6E), scheduled by the adapted typewriter.
// Silent/alternate talk flags and original text-speed timing are not yet ported.
void fe8_dialogue_tick(void){SyncPage();clock++;if(current&&visible<strlen(current->text)&&!(clock&1)){if(current->text[visible]!='\n')fe8_audio_se(0x6e);visible++;}}
int fe8_dialogue_confirm(void){SyncPage();if(current&&visible<strlen(current->text)){visible=strlen(current->text);fe8_audio_se(0x6e);return 0;}return 1;}
static void Pixel(u8 *rgba,int x,int y,int color){if(x<0||y<0||x>=240||y>=160)return;u8 *p=rgba+(y*240+x)*4;for(int c=0;c<3;c++){int v=color>>(c*5)&31;p[c]=(v<<3)|(v>>2);}p[3]=255;}
static void Sprite(u8 *rgba,const u8 *data,int bytes,const u16 *pal,int x,int y,int w,int h,int tile,int flip){
 for(int dy=0;dy<h;dy++)for(int dx=0;dx<w;dx++){
  int px=flip?w-1-dx:dx,pos=(tile+(dy/8)*32+px/8)*32+(dy&7)*4+(px&7)/2;
  if(pos<0||pos>=bytes){errors++;continue;}int index=data[pos]>>((px&1)*4)&15;
  if(index)Pixel(rgba,x+dx,y+dy,pal[index]);
 }
}
static void Portrait(u8 *rgba,const struct PortraitAsset *p,int x,int flip,int smile,int speaking){
 // The six original gSprite_Face96x96 OAM pieces use 2D tile mapping.
 Sprite(rgba,p->image,p->bytes,p->palette,x-32,80,64,32,0,flip);
 Sprite(rgba,p->image,p->bytes,p->palette,x-32,112,64,32,8,flip);
 Sprite(rgba,p->image,p->bytes,p->palette,x-32,144,32,16,flip?0x50:0x10,flip);
 Sprite(rgba,p->image,p->bytes,p->palette,x,144,32,16,flip?0x10:0x50,flip);
 Sprite(rgba,p->image,p->bytes,p->palette,x-48,128,16,32,flip?0x16:0x14,flip);
 Sprite(rgba,p->image,p->bytes,p->palette,x+32,128,16,32,flip?0x14:0x16,flip);
 // Register2dChrMove copies a tightly packed 4x2 patch to the mouth OBJ.
 int phase=speaking?(clock/8%4):2,offset=(smile?0:24)+(phase==1||phase==3?8:phase==2?16:0);
 int mx=x+(flip?4-p->xMouth:p->xMouth-4)*8-16,my=80+p->yMouth*8;
 for(int y=0;y<16;y++)for(int dx=0;dx<32;dx++){
  int sx=flip?31-dx:dx,pos=(offset+(y/8)*4+sx/8)*32+(y&7)*4+(sx&7)/2;
  if(pos>=48*32){errors++;continue;}int index=p->mouth[pos]>>((sx&1)*4)&15;
  if(index)Pixel(rgba,mx+dx,my+y,p->palette[index]);
 }
}
static int TextWidth(const char *s){int width=0,max=0;for(;*s;s++){if(*s=='\n'){if(width>max)max=width;width=0;}else width+=talkGlyphs[(u8)*s].width;}return width>max?width:max;}
static void Render(u8 *rgba,const struct DialoguePage *page,int shown){
 if(!page)return;
 for(int slot=0;slot<8;slot++){const struct PortraitAsset *p=FindPortrait(page->faces[slot]);if(p)Portrait(rgba,p,facePositions[slot]*8,facePositions[slot]<=14,page->smiles[slot],slot==page->active&&shown<strlen(page->text));}
 int lines=1;for(const char *s=page->text;*s;s++)if(*s=='\n')lines++;
 int width=(TextWidth(page->text)+7)/8+2;if(width>28)width=28;if(width<6)width=6;
 int height=lines*2+2;if(height>10)height=10;
 int anchor=page->active>=0?facePositions[page->active]:15,tail=anchor<16?anchor+3:anchor-5;
 int x=tail-width/2;if(x<1)x=1;if(x+width>29)x=29-width;
 int y=9-height+1;if(y<0)y=0;
 memset(bubbleMap,0,sizeof(bubbleMap));PutTalkBubbleTm(1,x,y,width,height);
 if(page->active>=0&&tail>=0&&tail<29)PutTalkBubbleTail(1,tail,y+height-1,anchor<16?0:1);
 for(int py=0;py<160;py++)for(int px=0;px<240;px++){
  int tile=bubbleMap[(py/8)*32+px/8];if(!tile)continue;
  int tx=tile&0x400?7-(px&7):px&7,ty=tile&0x800?7-(py&7):py&7;
  int pos=((tile&1023)-16)*32+ty*4+tx/2;if(pos<0||pos>=sizeof(talkBubble)){errors++;continue;}
  int index=talkBubble[pos]>>((tx&1)*4)&15;if(index)Pixel(rgba,px,py,talkBubblePal[index]);
 }
 int tx=(x+1)*8,ty=(y+1)*8,origin=tx;
 for(int n=0;page->text[n]&&n<shown;n++){
  int code=(u8)page->text[n];if(code=='\n'){tx=origin;ty+=16;continue;}
  for(int gy=0;gy<16;gy++)for(int gx=0;gx<16;gx++){
   int index=talkGlyphs[code].rows[gy]>>(gx*2)&3;
   if(index&&tx+gx<(x+width-1)*8&&ty+gy<(y+height-1)*8)Pixel(rgba,tx+gx,ty+gy,talkTextPal[talkInk[index]]);
  }
  tx+=talkGlyphs[code].width;
 }
}
void fe8_dialogue_render(u8 *rgba){SyncPage();Render(rgba,current,visible);}
int fe8_dialogue_info(int field){SyncPage();switch(field){case 0:return current?current->active:-1;case 1:return visible;case 2:return current?strlen(current->text):0;case 3:return errors;default:if(current&&field>=10&&field<18)return current->faces[field-10];return -1;}}
int fe8_dialogue_test_render(u8 *rgba,int id,int page){const struct DialoguePage *p=FindPage(id,page);if(!p)return -1;clock=0;Render(rgba,p,strlen(p->text));return 0;}
