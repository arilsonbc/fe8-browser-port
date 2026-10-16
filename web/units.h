#ifndef FE8_UNITS_H
#define FE8_UNITS_H
#include "generated/unit_types.h"
void fe8_units_reset(void);
void fe8_units_chapter_cleanup(void);
int fe8_units_load_event(u32 address,int animated,int cutscene);
int fe8_unit_steal(int actor,int target,int slot);
int fe8_units_load(u32 address,int animated);
int fe8_unit_spawn_definition(const struct UnitDefinition *def,int cutscene);
int fe8_units_pack_blue(struct GameSavePackedUnit *out);
int fe8_units_restore(int slot);
void fe8_units_tick(void);
int fe8_units_busy(void);
void fe8_units_render(u8 *rgba,int cameraX,int cameraY,int clock);
int fe8_unit_change_faction(int index,int faction);
int fe8_units_count(void);
int fe8_unit_at(int x,int y);
int fe8_unit_info(int index,int field);
int fe8_unit_move(int pid,int x,int y);
void fe8_units_clear_faction(int faction);
struct Unit *fe8_unit_by_pid(int pid);
int fe8_unit_make_item(int item);
int fe8_unit_give(int pid,int item);
int fe8_unit_move_queue(int pid,const u32 *queue,int length);
void fe8_unit_sync_position(int index);
void fe8_unit_remove(int pid);
void fe8_unit_remove_index(int index);
int fe8_unit_move_index(int index,int x,int y);
int fe8_unit_move_queue_index(int index,const u32 *queue,int length);
int fe8_units_deploy_existing(u32 address);
int fe8_units_load_event_config(u32 address,int animated,int cutscene,int noReda);
#endif
