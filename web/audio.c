#include "audio.h"
#include "save.h"
#include "units.h"
#include "events.h"
#include "constants/songs.h"
#define gPlaySt (*fe8_save_loaded_playst())
#include "generated/audio_chapters.h"
enum {MAP_BGM_BLUE=0,MAP_BGM_RED=1,MAP_BGM_GREEN=2,MAP_BGM_BLUE_GREEN_ALT=6,MAP_BGM_RED_ALT=7,EVFLAG_BGM_CHANGE=4,BATTLEMAP_KIND_SKIRMISH=1};
static const struct AudioChapter *GetROMChapterStruct(int id){return &audioChapters[id];}
static int CheckFlag(int flag){return fe8_event_flag(flag);}
static int GetBattleMapKind(void){return 0;} // Current runtime is story-only; no skirmish entry.
extern int CountUnitsInState(int faction,int state);
#include "generated/audio_map_routines.h"
int fe8_audio_map_song(void){if((u8)gPlaySt.chapterIndex>=79||(gPlaySt.faction!=0&&gPlaySt.faction!=64&&gPlaySt.faction!=128))return -1;return GetCurrentMapMusicIndex();}
void fe8_audio_map_start(void){int song=fe8_audio_map_song();if(song>=0)fe8_audio_bgm(song,3);}
// Ordered native->audio boundary. Repeated identical SE requests remain distinct.
// Overflow never changes gameplay or SRAM; the browser drains every game update.
static struct {int kind,a,b,c;} queue[128];
static int head,count,dropped,serial,lastSong=-1,savedSong=-1;
static void Push(int kind,int a,int b,int c){
 if(count==128){head=(head+1)%128;count--;dropped++;}
 int p=(head+count++)%128;queue[p].kind=kind;queue[p].a=a;queue[p].b=b;queue[p].c=c;serial++;
}
void fe8_audio_reset(void){head=count=dropped=serial=0;lastSong=savedSong=-1;Push(0,0,0,0);}
void fe8_audio_bgm(int song,int speed){if(song<0||song>=1000||song==lastSong||fe8_save_loaded_playst()->config.disableBgm)return;lastSong=song;savedSong=-1;Push(1,song,speed<0?6:speed,0);}
void fe8_audio_se(int song){if(song<0||song>=1000||fe8_save_loaded_playst()->config.disableSoundEffects)return;Push(2,song,0,0);}
void fe8_audio_fadein(int song,int speed){if(song<0||song>=1000||fe8_save_loaded_playst()->config.disableBgm)return;lastSong=song;Push(3,song,speed,0);}
void fe8_audio_fadeout(int speed){lastSong=0;Push(4,speed<0?6:speed,0,0);}
void fe8_audio_override(int song){if(fe8_save_loaded_playst()->config.disableBgm)return;savedSong=lastSong;lastSong=song;Push(5,song,0,0);}
void fe8_audio_restore(int speed){if(fe8_save_loaded_playst()->config.disableBgm)return;if(savedSong>=0){lastSong=savedSong;savedSong=-1;}Push(6,speed,0,0);}
void fe8_audio_volume(int from,int to,int frames){if(!to)lastSong=0;Push(7,from,to,frames);}
int fe8_audio_count(void){return count;}
int fe8_audio_read(int field){if(!count)return -1;return field==0?queue[head].kind:field==1?queue[head].a:field==2?queue[head].b:field==3?queue[head].c:-1;}
void fe8_audio_pop(void){if(count){head=(head+1)%128;count--;}}
int fe8_audio_info(int field){return field==0?serial:field==1?dropped:field==2?lastSong:-1;}
