#ifndef FE8_TACTICAL_H
#define FE8_TACTICAL_H
#include "compat.h"
struct Unit;
int fe8_tactical_probe_movement(struct Unit *unit,int x,int y,int limit,int id,u8 *out);
void fe8_tactical_reset(void);
void fe8_tactical_render_menu(u8 *rgba);
void fe8_tactical_render_item_menu(u8 *rgba);
int fe8_tactical_start(void);
void fe8_tactical_tick(int keys);
int fe8_tactical_select(int index);
int fe8_tactical_move(int x,int y);
int fe8_tactical_cancel(void);
int fe8_tactical_begin_attack(void);
int fe8_tactical_attack(void);
int fe8_tactical_next_target(void);
int fe8_tactical_forecast(int field);
int fe8_tactical_wait(void);
int fe8_tactical_can_heal(void);
int fe8_tactical_begin_heal(void);
int fe8_tactical_heal(void);
int fe8_tactical_can_item(void);
int fe8_tactical_begin_item(void);
int fe8_tactical_use_item(void);
int fe8_tactical_can_visit(void);
int fe8_tactical_visit(void);
int fe8_tactical_can_talk(void);
int fe8_tactical_begin_talk(void);
int fe8_tactical_talk(void);
int fe8_tactical_can_seize(void);
int fe8_tactical_seize(void);
int fe8_tactical_end_turn(void);
int fe8_tactical_info(int field);
int fe8_tactical_cost(int x,int y);
int fe8_tactical_consumes_back(void);
int fe8_tactical_locks_cursor(void);
int fe8_tactical_show_active(int index);
void fe8_tactical_render_range(u8 *rgba,int cameraX,int cameraY,int frame);
int fe8_tactical_npc_move(int index,int x,int y);
#endif
