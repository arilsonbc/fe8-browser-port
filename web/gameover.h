#ifndef FE8_GAMEOVER_H
#define FE8_GAMEOVER_H
#include "compat.h"
void fe8_gameover_reset(void);
int fe8_gameover_start(void);
void fe8_gameover_tick(int keys);
int fe8_gameover_active(void);
int fe8_gameover_info(int field);
int fe8_gameover_confirm(void);
u8 *fe8_gameover_pixels(void);
#endif
