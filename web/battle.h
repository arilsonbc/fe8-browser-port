#ifndef FE8_BATTLE_H
#define FE8_BATTLE_H
#include "compat.h"
int fe8_battle_obstacle_forecast(int actor,int x,int y,int slot,int field);
int fe8_battle_obstacle_attack(int actor,int x,int y,int slot);
void fe8_battle_reset(void);
int fe8_battle_set_script(const u32 *queue,int length);
int fe8_battle_start(int actor,int target,int item,int ballista,const u32 *queue,int length);
int fe8_battle_can_heal(int actor,int target,int slot);
int fe8_battle_heal(int actor,int target,int slot);
int fe8_battle_steal(int actor,int target,int slot);
int fe8_battle_attack(int actorIndex,int targetIndex,int slot);
int fe8_battle_forecast(int actorIndex,int targetIndex,int slot,int field);
void fe8_battle_tick(void);
int fe8_battle_busy(void);
int fe8_battle_info(int field);
void fe8_battle_render(u8 *rgba,int cameraX,int cameraY);
int fe8_battle_simulate(int actorIndex,int targetIndex,int x,int y,int slot);
int fe8_battle_simulation_info(int field);
struct BattleUnit;
int fe8_battle_simulation_copy(struct BattleUnit *actor,struct BattleUnit *target);
#endif
