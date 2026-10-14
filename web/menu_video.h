#ifndef FE8_MENU_VIDEO_H
#define FE8_MENU_VIDEO_H
#include "palette.h"
void menu_video_reset(void);
void menu_video_load(int difficulty);
void menu_video_begin(void);
void menu_video_fog(const u16 *lines,int y);
void PutSpriteExt(int layer,int x,int y,const u16 *layout,int oam2);
void SetObjAffine(int index,int pa,int pb,int pc,int pd);
void menu_slot_palette(int selected);
void menu_slot_hard_palette(int frame,int selected);
void menu_video_slot_title(int slot,int title);
void menu_video_confirmation(int choice,int operation,int copyDestination);
u8 *menu_video_pixels(void);
int menu_video_errors(void);
u8 *menu_video_obj_buffer(void);
void menu_video_finish_frame(void);
void menu_video_test_scene(int mode);
void menu_video_difficulty_selection(int selection);
#endif
