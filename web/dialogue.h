#ifndef FE8_DIALOGUE_H
#define FE8_DIALOGUE_H
#include "compat.h"
void fe8_dialogue_reset(void);
void fe8_dialogue_tick(void);
void fe8_dialogue_render(u8 *rgba);
int fe8_dialogue_confirm(void);
int fe8_dialogue_info(int field);
#endif
