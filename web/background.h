#ifndef FE8_BACKGROUND_H
#define FE8_BACKGROUND_H
#include "compat.h"
int fe8_background_set(int id);
int fe8_background_info(int field);
void fe8_background_render(u8 *rgba);
#endif
