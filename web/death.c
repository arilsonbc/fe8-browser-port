#include "death.h"
#include "audio.h"
#include "units.h"
#include "save.h"
#include "events.h"
#include "constants/songs.h"
#define gPlaySt (*fe8_save_loaded_playst())
extern struct Unit *GetUnit(int index),*GetUnitFromCharId(int pid);
static int lastPid,lastMessage,lastSong=-1,count;
static int GetEventTriggerState(int flag){return fe8_event_flag(flag);}
static void SetFlag(int flag){fe8_event_set_flag(flag);}
static void SetPidDefeatedFlag(u8 pid,int flag){(void)pid;SetFlag(flag);} // Story-only runtime; extra-map protect-character exception not enabled.
static void StartBgm(int song,void *proc){(void)proc;lastSong=song;fe8_audio_bgm(song,3);}
static void CallBattleQuoteEventInBattle(int msg){lastMessage=msg;fe8_event_quote(msg);}
static void NativeDeathEvent(u32 address){if(fe8_event_call(address,0)<0)fe8_event_quote(-1);}
#include "generated/death_assets.h"
#include "generated/death_routines.h"
void fe8_death_reset(void){lastPid=lastMessage=count=0;lastSong=-1;}
void fe8_death_after_battle(int actor,int target){
 struct Unit *a=GetUnit(actor),*b=GetUnit(target),*u=!a->curHP?a:!b->curHP?b:NULL;if(!u||!UNIT_IS_VALID(u))return;
 int pid=u->pCharacterData->number;if(!CheckBattleDefeatTalk(pid))return;lastPid=pid;count++;DisplayDefeatTalkForPid(pid);
}
int fe8_death_info(int field){return field==0?lastPid:field==1?lastMessage:field==2?lastSong:field==3?count:-1;}
