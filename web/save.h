#ifndef FE8_SAVE_H
#define FE8_SAVE_H
#include "generated/save_types.h"
void fe8_save_checkpoint_reset(void);
int fe8_save_checkpoint_capture(void);
int fe8_save_checkpoint_info(int field);
int fe8_save_checkpoint_commit(void);
void fe8_save_playtime_tick(void);
const u8 *fe8_save_loaded_flags(void);
void fe8_save_ensure(void);
void fe8_save_read_playst(int slot,struct PlaySt *p);
int fe8_save_suspend_slot(void);
int fe8_save_valid(int slot);
int fe8_save_chapter(int slot);
int fe8_save_title(int slot);
int fe8_save_difficulty(int slot);
int fe8_save_time(int slot);
int fe8_save_last_slot(void);
int fe8_save_create(int slot,int difficulty);
int fe8_save_copy(int source,int destination);
int fe8_save_erase(int slot);
int fe8_save_load(int slot);
u8 *fe8_save_buffer(void);
u8 *fe8_save_import_buffer(void);
int fe8_save_import(int length);
int fe8_save_revision(void);
int fe8_save_global_valid(void);
int fe8_save_errors(void);
int fe8_save_import_warnings(void);
void fe8_save_reset_empty(void);
int fe8_convoy_count(void);
int fe8_convoy_item(int index);
int fe8_convoy_add(int item);
int fe8_convoy_remove(int index);
struct PlaySt *fe8_save_loaded_playst(void);
int fe8_save_read_units(int slot,struct GameSavePackedUnit *out);
#endif
