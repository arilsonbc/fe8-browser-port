#include "generated/menu_types.h"
#include "menu_video.h"
#include "generated/menu_assets.h"
#include "generated/menu_layouts.h"
#include "constants/songs.h"
#include "rng.h"
#include "save.h"
#include "audio.h"
#include <string.h>
extern struct KeyStatusBuffer *fe8_input_status(void);
extern void fe8_game_set_action(int action);
static struct KeyStatusBuffer *gKeyStatusPtr;
static struct SaveMenuProc *menu;
static struct SaveDrawProc draw;
static struct SaveDrawCursorProc cursor;
static struct DifficultyMenuSpritesProc difficultySprites;
static struct Unknown020007E0 gUnknown_020007E0[20];
static u16 scrollLines[160];static int scrollY;
static int screenKind,stage=-1,sound=-1,request=-1,validMask=-1,extrasMask=2,saveResult;
static int IsSaveValid(int slot){return validMask>=0?(slot>=0&&slot<3&&((validMask>>slot)&1)):fe8_save_valid(slot);}
static int ReadLastGameSaveId(void){return validMask>=0?0:fe8_save_last_slot();}
static int IsExtraLinkArenaEnabled(int unused){(void)unused;return extrasMask&1;}
static int IsExtraSoundRoomEnabled(void){return 1;}
static int IsExtraSupportViewerEnabled(void){return extrasMask&4;}
static int IsExtraFreeMapEnabled(void){return extrasMask&16;}
static int IsExtraBonusClaimEnabled(void){return extrasMask&32;}
static int DivRem(int a,int b){return a%b;}
static int DivArm(int divisor,int value){return divisor?value/divisor:0;}
static int _DivArm2(int t,int duration,int unused){(void)unused;return t*4096/duration;}
static void EnablePaletteSync(void){}
static u16 *GetBgVerticalScrollBuffer(int unused,bool enabled){(void)unused;(void)enabled;return scrollLines;}
static void BG_SetPosition(int bg,int x,int y){(void)bg;(void)x;scrollY=y;}
static void FlipBgVerticalScroll(void){menu_video_fog(scrollLines,scrollY);}
static void PlaySoundEffect(int id){sound=id;fe8_audio_se(id);}
static void StartBgmVolumeChange(int a,int b,int c,int d){(void)d;fe8_audio_volume(a,b,c);}
static void ChangeBgm(int a,int b,int c,int d,ProcPtr p){(void)p;if(a)fe8_audio_bgm(a,0);fe8_audio_volume(b,c,d);}
static bool SaveMenuWaitHelpBoxAnim(ProcPtr p){(void)p;return false;}
static void SaveMenuHandleHelpBox(ProcPtr p){(void)p;}
static void SaveMenuDrawSubSelBox(ProcPtr p,int enable){struct SaveMenuProc *m=p;if(!enable)m->cursor_config=0;}
static void CopyGameSave(int from,int to){request=2;saveResult=validMask>=0?0:fe8_save_copy(from,to);}
static void InvalidateGameSave(int slot){request=4;saveResult=validMask>=0?0:fe8_save_erase(slot);}
static void ReadGameSave(int slot){saveResult=validMask>=0?0:fe8_save_load(slot);}
static void WriteGameSave(int slot){(void)slot;saveResult=-7;request=6;stage=9;} // Runtime game state not yet ported.
static void SaveMenuWriteNewGame(struct SaveMenuProc *p){request=1;saveResult=validMask>=0?0:fe8_save_create(p->sus_slot,p->difficulty);}
static void SaveMenu_NewGame(ProcPtr p){Proc_Goto(p,PL_SAVEMENU_NEW_GAME);}
static void sub_80A9D20(ProcPtr p,int dir){(void)p;(void)dir;}
static void DrawDifficultyModeText(ProcPtr p){(void)p;}
static void sub_80AC034(ProcPtr p){(void)p;}
void sub_80AC680(struct DifficultyMenuSpritesProc *p,int x,int y);
static void SaveMenu_SetDifficultyChoice(int choice,int unused){(void)unused;menu->difficulty=choice;}
#define OBJ_VRAM0 (menu_video_obj_buffer())
#define TILE_SIZE_4BPP 32
static void Decompress(const void *src,void *dest){
 if(src==Img_GameMainMenuObjs)memcpy(dest,src,sizeof(Img_GameMainMenuObjs));
}
static void FormatTime(int frames,u16 *hours,u16 *minutes,u16 *seconds){
 int time=frames/60;*seconds=time%60;*minutes=(time/60)%60;*hours=time/3600;
}
#include "generated/menu_routines.h"
extern void NewFadeIn(int speed,ProcPtr parent);
extern void NewFadeOut(int speed,ProcPtr parent);
extern bool FadeInExists(void),FadeOutExists(void);
static void FadeIn(ProcPtr p){NewFadeIn(8,p);}
static void FadeOut(ProcPtr p){NewFadeOut(8,p);}
static void WaitFade(ProcPtr p){if(!FadeInExists()&&!FadeOutExists())Proc_Break(p);}
static void EnterMain(ProcPtr p){(void)p;stage=3;request=-1;menu->scroll_cnt=0;}
static void EnterExtras(ProcPtr p){(void)p;stage=6;}
static void EnterSlots(ProcPtr p){(void)p;stage=8;menu->cursor_config=0;menu_slot_palette(menu->sus_slot);}
static void BeginTransition(ProcPtr p){(void)p;stage=10;}
static void Initialize(ProcPtr p){
 fe8_save_ensure();menu=p;saveResult=0;gKeyStatusPtr=fe8_input_status();stage=10;screenKind=0;sound=request=-1;
 menu_video_load(0);SetDispEnable(0,0,0,0,0);menu->sus_slot=0;menu->sus_slot_cur=validMask>=0?255:fe8_save_suspend_slot();menu->cursor_slot=255;menu->unk_44=0xf0;
 menu->main_select=0;menu->extra_select=0;menu->cursor_config=0;menu->unk_2f=menu->unk_46=0;
 for(int i=0;i<3;i++){menu->chapter_idx[i]=IsSaveValid(i)?(validMask>=0?0:fe8_save_title(i)):255;menu->played_time[i]=validMask>=0?0:fe8_save_time(i);}
 InitSaveMenuChoice(menu);menu->main_sel_bitfile=SaveMenuGetBitfile(menu->main_options,0);
 menu->jump_label=PL_SAVEMENU_MAIN_LOOP;
 memset(&draw,0,sizeof(draw));memset(&cursor,0,sizeof(cursor));draw.proc_parent=menu;draw.unk_34=&cursor;cursor.proc_parent=&draw;
 draw.unk_29=1;menu->savedraw=&draw;SaveDrawCursor_Init(&cursor);sub_80AB548();
}
static void DrawFrame(void){
 menu_video_begin();
 if(screenKind==1){DrawDifficultySprites_Loop(&difficultySprites);menu_video_difficulty_selection(difficultySprites.current_selection);}
 else {
  sub_80AAE90(&draw);sub_80AAF6C(&draw);
  if(menu->unk_2f){
   sub_80AADE0(&draw);sub_80AA7EC(&draw);
   for(int i=0;i<3;i++){
    PutSpriteExt(4,OAM1_X(248-menu->unk_2f),32+i*32,SpriteArray_08A209F0[i],OAM2_PAL(i*2+10));
    PutSpriteExt(4,OAM1_X(248-menu->unk_2f),40+i*32,SpriteArray_08A209E4[i],OAM2_PAL(i*2+11));
   }
   if(stage==8||stage==9){sub_80AB4F4(1,28,32+menu->sus_slot*32,&draw);menu_slot_palette(menu->sus_slot);}
  }
  // Original palette animation of inactive menu panels and particles.
  sub_80AAB78(menu->unk_2f==0,draw.unk_2a);
  if(draw.unk_2a%4==0)gPaletteBuffer[0x111]=Pal_08A28088[(draw.unk_2a&63)>>2];
  menu_video_confirmation(menu->cursor_config,menu->main_sel_bitfile,menu->cursor_slot!=255);
  if(menu->cursor_config){PutSpriteExt(4,menu->cursor_config==1?40:84,124,Sprite_08A20570,0x3000);}
  if(menu->unk_2f)menu_slot_hard_palette(draw.unk_2a,menu->sus_slot);
   SaveDrawCursor_Loop(&cursor);sub_80AB56C(draw.unk_2a);draw.unk_2a++;
 }
 SaveDraw_ScrollFogBG(&draw);menu_video_finish_frame();
}
static void Drawing(ProcPtr p){(void)p;DrawFrame();}
static const struct ProcCmd drawingScript[]={PROC_REPEAT(Drawing),PROC_END};
static void StartDrawing(ProcPtr p){Proc_Start(drawingScript,p);}
static void DifficultyInit(ProcPtr p){
 struct DifficultyMenuProc *d=p;d->current_selection=0;d->unk_2c=0;stage=10;screenKind=1;menu_video_load(1);SetDispEnable(0,0,0,0,0);
 memset(&difficultySprites,0,sizeof(difficultySprites));
 DrawDifficultySprite_Init(&difficultySprites);
 difficultySprites.unk_2e=32;
}
static void DifficultyReady(ProcPtr p){(void)p;stage=7;}
static void DifficultyDone(ProcPtr p){(void)p;screenKind=0;menu_video_load(0);menu->scroll_cnt=0;
 if(menu->difficulty==3){menu->unk_2f=0;menu->jump_label=PL_SAVEMENU_MAIN_LOOP;}else{menu->unk_2f=220;menu->jump_label=PL_SAVEMENU_SAVE_SLOT_SEL;}
}
// Original difficulty key handler calls Proc_Goto on its own child process.
static void DifficultyLoop(ProcPtr p){
 struct DifficultyMenuProc *d=p;d->sprites_proc=&difficultySprites;
 DifficultySelect_Loop_KeyHandler(d);
}
static const struct ProcCmd difficultyScript[]={
 PROC_CALL(DifficultyInit),PROC_SLEEP(1),PROC_CALL(FadeIn),PROC_REPEAT(WaitFade),PROC_CALL(DifficultyReady),PROC_REPEAT(DifficultyLoop),
 PROC_LABEL(1),PROC_SLEEP(10),PROC_GOTO(2),
 PROC_LABEL(2),PROC_CALL(FadeOut),PROC_REPEAT(WaitFade),PROC_END
};
static void StartDifficulty(ProcPtr p){
 struct DifficultyMenuProc *d=Proc_StartBlocking(difficultyScript,p);d->sprites_proc=&difficultySprites;
}
static void ToSlotPhase(ProcPtr p){SaveMenu_PostDifficultHandler(p);}
static void RefreshSlots(void){
 for(int i=0;i<3;i++){
  menu->chapter_idx[i]=IsSaveValid(i)?(validMask>=0?0:fe8_save_title(i)):255;
  menu->played_time[i]=validMask>=0?0:fe8_save_time(i);
  menu_video_slot_title(i,menu->chapter_idx[i]);
 }
 InitSaveMenuChoice(menu);if(menu->main_select>=menu->unk_31)menu->main_select=0;
}
static void RefreshSlotsCall(ProcPtr p){(void)p;RefreshSlots();}
static void NewGameBoundary(ProcPtr p){
 if(validMask>=0){stage=9;return;}
 if(saveResult<0){request=6;stage=9;return;}
 RefreshSlots();menu->cursor_config=0;menu->cursor_slot=255;menu->scroll_cnt=0;
 if(menu->main_sel_bitfile==MAIN_MENU_OPTION_NEW_GAME)Proc_Goto(p,PL_SAVEMENU_NEW_GAME);
 else Proc_Goto(p,PL_SAVEMENU_BACK_TO_MAIN);
}
static void SoundRoomBoundary(ProcPtr p){(void)p;request=3;stage=9;}
static void BoundaryLoop(ProcPtr p){if(gKeyStatusPtr->newKeys&B_BUTTON){int wasSound=request==3;request=-1;menu->scroll_cnt=0;Proc_Goto(p,wasSound?PL_SAVEMENU_10:PL_SAVEMENU_BACK_TO_MAIN);}}
static void Exit(ProcPtr p){
 struct SaveMenuProc *m=p;
 if(m->main_sel_bitfile&MAIN_MENU_OPTION_EXIT)fe8_game_set_action(5);
 else if(validMask<0&&fe8_save_load(m->sus_slot)==0)fe8_game_set_action(m->main_sel_bitfile==MAIN_MENU_OPTION_NEW_GAME?0:m->sus_slot+1);
 else fe8_game_set_action(5);
 stage=-1;menu=NULL;
}
static const struct ProcCmd menuScript[]={
 PROC_NAME("savemenu-web"),PROC_CALL(Initialize),PROC_CALL(RefreshSlotsCall),PROC_CALL(StartDrawing),PROC_SLEEP(1),PROC_CALL(FadeIn),PROC_REPEAT(WaitFade),
 PROC_LABEL(PL_SAVEMENU_MAIN_LOOP),PROC_CALL(EnterMain),PROC_REPEAT(SameMenu_CtrlLoop),PROC_GOTO(PL_SAVEMENU_NEW_GAME),
 PROC_LABEL(PL_SAVEMENU_DIFFICULTY_SEL),PROC_CALL(BeginTransition),PROC_CALL(FadeOut),PROC_REPEAT(WaitFade),PROC_CALL(StartDifficulty),PROC_YIELD,PROC_CALL(DifficultyDone),PROC_CALL(FadeIn),PROC_REPEAT(WaitFade),PROC_CALL(ToSlotPhase),
 PROC_LABEL(PL_SAVEMENU_SAVE_SLOT_SEL),PROC_CALL(EnterSlots),PROC_REPEAT(SaveMenu_SaveSlotSelectLoop),PROC_BLOCK,
 PROC_LABEL(PL_SAVEMENU_BACK_TO_MAIN),PROC_CALL(BeginTransition),PROC_REPEAT(SaveMenuScrollBackToMain),PROC_GOTO(PL_SAVEMENU_MAIN_LOOP),
 PROC_LABEL(PL_SAVEMENU_8),PROC_CALL(BeginTransition),PROC_REPEAT(sub_80A9A68),
 PROC_LABEL(PL_SAVEMENU_10),PROC_CALL(EnterExtras),PROC_REPEAT(sub_80A9B90),PROC_BLOCK,
 PROC_LABEL(PL_SAVEMENU_9),PROC_CALL(BeginTransition),PROC_REPEAT(sub_80A9AB0),PROC_BLOCK,
 PROC_LABEL(PL_SAVEMENU_SCROLL_SLOT),PROC_CALL(BeginTransition),PROC_REPEAT(SaveMenuScrollSlot),PROC_GOTO(PL_SAVEMENU_SAVE_SLOT_SEL),
 PROC_LABEL(PL_SAVEMENU_SLOT_SELECTED),PROC_CALL(NewGameBoundary),PROC_REPEAT(BoundaryLoop),PROC_BLOCK,
 PROC_LABEL(PL_SAVEMENU_EXEC_EXTRA_MISC_OPTION),PROC_CALL(SoundRoomBoundary),PROC_REPEAT(BoundaryLoop),PROC_BLOCK,
 PROC_LABEL(PL_SAVEMENU_NEW_GAME),PROC_CALL(BeginTransition),PROC_CALL(FadeOut),PROC_REPEAT(WaitFade),PROC_CALL(Exit),PROC_END,
 PROC_LABEL(PL_SAVEMENU_EXIT_FADE),PROC_CALL(Exit),PROC_END,
 PROC_LABEL(PL_SAVEMENU_12),PROC_CALL(SoundRoomBoundary),PROC_REPEAT(BoundaryLoop),PROC_BLOCK
};
void fe8_menu_start(ProcPtr parent){Proc_StartBlocking(menuScript,parent);}
void fe8_menu_detach(void){menu=NULL;stage=-1;}
int fe8_menu_state(void){return stage>=0&&(FadeInExists()||FadeOutExists())?10:stage;}
int fe8_menu_selection(void){if(!menu)return -1;return stage==7?difficultySprites.current_selection:stage==6?menu->extra_select:stage==8||stage==9?menu->sus_slot:menu->main_select;}
int fe8_menu_options(void){return menu?menu->main_options:0;}
int fe8_menu_sound(void){return sound;}
int fe8_menu_request(void){return request;}
int fe8_menu_difficulty(void){return menu?menu->difficulty:-1;}
u8 *fe8_menu_pixels(void){return menu_video_pixels();}
void fe8_menu_test_profile(int mask,int extra){validMask=mask<0?-1:mask&7;extrasMask=extra|2;}
int fe8_menu_save_error(void){return saveResult;}
int fe8_menu_operation(void){return menu?menu->main_sel_bitfile:0;}
int fe8_menu_confirmation(void){return menu?menu->cursor_config:0;}

