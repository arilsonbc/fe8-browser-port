#ifndef FE8_WEB_PALETTE_H
#define FE8_WEB_PALETTE_H
#include "compat.h"
#include <stdbool.h>
#define CONST_DATA
#define STRUCT_PAD(a,b) u8 pad_##a[(b)-(a)]
#define RED_VALUE(c) ((c)&31)
#define GREEN_VALUE(c) (((c)>>5)&31)
#define BLUE_VALUE(c) (((c)>>10)&31)
extern u16 gPaletteBuffer[512];
extern s8 gFadeComponentStep[32],gFadeComponents[1536];
void fe8_palette_reset(void);
void SetBackdropColor(int color);
void SetBlendDarken(int amount);
void SetBlendTargetA(int a,int b,int c,int d,int e);
void SetBlendBackdropA(int enable);
void SetDispEnable(int a,int b,int c,int d,int e);
int fe8_display_enabled(void);
#endif
