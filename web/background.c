#include "background.h"
struct BackgroundAsset {int id,width,height,bytes;const u8 *gfx;const u16 *tsa,*pal;};
#include "generated/background_assets.h"
static const struct BackgroundAsset *current;
static int errors;
int fe8_background_set(int id){if(id==-1){current=0;errors=0;return 0;}for(int i=0;i<sizeof(backgrounds)/sizeof(*backgrounds);i++)if(backgrounds[i].id==id){current=backgrounds+i;return 0;}return -1;}
int fe8_background_info(int f){return f==0?(current?current->id:-1):f==1?errors:-1;}
void fe8_background_render(u8 *rgba){
 if(!current||!rgba)return;
 for(int y=0;y<160;y++)for(int x=0;x<240;x++){
  // Original TmApplyTsa walks the source left-to-right, bottom row first.
  int tile=current->tsa[(current->height-1-y/8)*current->width+x/8];
  int dx=tile&1024?7-(x&7):x&7,dy=tile&2048?7-(y&7):y&7;
  int offset=(tile&1023)*32+dy*4+dx/2;
  if(offset>=current->bytes||(tile>>12)>=8){errors++;continue;}
  int ink=current->gfx[offset]>>((dx&1)*4)&15,color=current->pal[(tile>>12)*16+ink];
  u8 *p=rgba+(y*240+x)*4;for(int c=0;c<3;c++){int v=color>>(c*5)&31;p[c]=(v<<3)|(v>>2);}p[3]=255;
 }
}
