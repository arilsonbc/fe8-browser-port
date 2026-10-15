#ifndef FE8_MAP_H
#define FE8_MAP_H
#include "compat.h"
int fe8_map_obstacle_hp(int x,int y);
int fe8_map_obstacle_max(int x,int y);
int fe8_map_obstacle_update(int x,int y,int hp);
int fe8_map_start(int chapter);
void fe8_map_stop(void);
void fe8_map_tick(int keys);
int fe8_map_active(void);
u8 *fe8_map_pixels(void);
int fe8_map_info(int field);
int fe8_map_terrain(int x,int y);
int fe8_map_tile(int x,int y,int part);
int fe8_map_change(int id,int revert);
int fe8_map_change_at(int x,int y);
void fe8_map_focus(int x,int y);
#endif
