#include "palette.h"
#include <string.h>
u16 gPaletteBuffer[512];
s8 gFadeComponentStep[32],gFadeComponents[1536];
static int displayEnabled=1,darken;
void fe8_palette_reset(void) {
    memset(gPaletteBuffer,0,sizeof(gPaletteBuffer));
    memset(gFadeComponentStep,0,sizeof(gFadeComponentStep));
    memset(gFadeComponents,0,sizeof(gFadeComponents));displayEnabled=1;darken=0;
}
void SetBackdropColor(int color) { gPaletteBuffer[0]=color; }
void SetBlendDarken(int amount) { darken=amount; }
// These flags are captured for the currently supported all-layer darken only.
void SetBlendTargetA(int a,int b,int c,int d,int e) {(void)a;(void)b;(void)c;(void)d;(void)e;}
void SetBlendBackdropA(int enable) {(void)enable;}
void SetDispEnable(int a,int b,int c,int d,int e) {displayEnabled=a||b||c||d||e;}
int fe8_display_enabled(void) {return displayEnabled && darken<16;}
u16 *fe8_palette_buffer(void) {return gPaletteBuffer;}
