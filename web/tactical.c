#include "tactical.h"
#include "battle.h"
#include "support.h"
#include "ai.h"
#include "recovery.h"
#include "audio.h"
#include "generated/battle_types.h"
#include "units.h"
#include "map.h"
#include "events.h"
#include "save.h"
#include "rng.h"
#include <string.h>
#include <stdlib.h>
#define gPlaySt (*fe8_save_loaded_playst())
#include "generated/tactical_assets.h"
extern struct Unit *GetUnit(int index);
extern int GetUnitMaxHp(struct Unit *unit);
struct Unit *gActiveUnit;
static u8 gActiveUnitId;
static struct Vec2 gActiveUnitMoveOrigin,gBmMapSize;
static struct {u8 subjectIndex,unitActionType,moveCount;} gActionData;
static struct {int gameStateBits,taken_action,unk3F;} gBmSt;
struct Trap {int xPos,yPos,type;};
#define TRAP_BALLISTA 1
static struct Trap *GetTrapAt(int x,int y){(void)x;(void)y;return NULL;}
static int CanCharacterBePrepMoved(int pid){(void)pid;return 0;}
static void sub_802C334(void){}
static int squaresMoved[256];
static void PidStatsAddSquaresMoved(int pid,int count){squaresMoved[pid&255]+=count;}
static void PidStatsSubFavval08(int pid){(void)pid;} // Usage statistics persistence remains a separate integration.
static void UnitFinalizeMovement(struct Unit *u){(void)u;} // Selection refuses rescue/ballista states.
static int mode=-1,selected,menuChoice,lastError,floodError;
static struct {int index,slot;} targets[255*5];
static int targetCount,targetChoice;

static u8 terrain[64][64],occupancy[64][64],movement[64][64];
static u8 *gBmMapTerrain[64],*gBmMapUnit[64],*gBmMapMovement[64],**gWorkingBmMap;
static u8 gWorkingTerrainMoveCosts[65];
static struct MovMapFillStateExt gMovMapFillStPool1[8192],gMovMapFillStPool2[8192],*poolEnd;
static struct MovMapFillState gMovMapFillState;
static void SetWorkingBmMap(u8 **map){gWorkingBmMap=map;}
static void BmMapFill(u8 **map,int value){for(int y=0;y<gBmMapSize.y;y++)memset(map[y],value,gBmMapSize.x);}
static void CallARM_FillMovementMap(void);
#define TILEMAP_INDEX(x,y) ((y)*32+(x))
#include "generated/menu_font.h"
#include "generated/tactical_routines.h"
static void CallARM_FillMovementMap(void){
 // Equivalent native loop in the original ARM core's order. No ARM instructions are executed.
 for(int wave=0;wave<4096;wave++){
  gMovMapFillState.src=(wave&1)?gMovMapFillStPool2:gMovMapFillStPool1;
  gMovMapFillState.dst=(wave&1)?gMovMapFillStPool1:gMovMapFillStPool2;
  poolEnd=gMovMapFillState.dst+8191;
  if(gMovMapFillState.src->connexion==4)return;
  for(int n=0;n<8192&&gMovMapFillState.src->connexion!=4;n++,gMovMapFillState.src++){
   int direction=gMovMapFillState.src->connexion;
   if(direction!=2)sub_801A570(3,0,-1);
   if(direction!=3)sub_801A570(2,0,1);
   if(direction!=1)sub_801A570(0,-1,0);
   if(direction!=0)sub_801A570(1,1,0);
   if(floodError)return;gMovMapFillState.dst->connexion=4;
  }
 }
 floodError=3;
}
static void SyncMaps(void){
 gBmMapSize.x=fe8_map_info(1);gBmMapSize.y=fe8_map_info(2);
 memset(occupancy,0,sizeof(occupancy));
 for(int y=0;y<64;y++){gBmMapTerrain[y]=terrain[y];gBmMapUnit[y]=occupancy[y];gBmMapMovement[y]=movement[y];for(int x=0;x<64;x++){int t=fe8_map_terrain(x,y);terrain[y][x]=t<0?0:t;}}
 for(int i=1;i<256;i++){struct Unit *u=GetUnit(i);if(UNIT_IS_VALID(u)&&!(u->state&(US_HIDDEN|US_DEAD|US_NOT_DEPLOYED|US_RESCUED))&&u->xPos>=0&&u->yPos>=0&&u->xPos<gBmMapSize.x&&u->yPos<gBmMapSize.y)occupancy[u->yPos][u->xPos]=i;}
}
void fe8_tactical_reset(void){fe8_recovery_reset();fe8_ai_reset();targetCount=targetChoice=0;gActiveUnit=NULL;gActiveUnitId=selected=menuChoice=lastError=floodError=0;mode=-1;memset(movement,255,sizeof(movement));memset(squaresMoved,0,sizeof(squaresMoved));}
int fe8_tactical_start(void){if(mode==0&&!fe8_event_busy())return 0;if(fe8_event_busy()||!fe8_map_active()||mode>0)return -1;SyncMaps();gPlaySt.faction=FACTION_BLUE;if(!gPlaySt.chapterTurnNumber)gPlaySt.chapterTurnNumber=1;mode=0;selected=lastError=0;fe8_recovery_phase(FACTION_BLUE);fe8_audio_map_start();if(gPlaySt.chapterIndex==1||gPlaySt.chapterIndex==2)fe8_event_phase();return 0;}
int fe8_tactical_select(int index){
 if(mode!=0||gPlaySt.faction!=FACTION_BLUE||fe8_event_busy()||index<1||index>255)return -1;
 struct Unit *u=GetUnit(index);if(!UNIT_IS_VALID(u)||u->xPos<0||u->yPos<0||u->xPos>=gBmMapSize.x||u->yPos>=gBmMapSize.y||u->state&(US_DEAD|US_HIDDEN|US_NOT_DEPLOYED|US_RESCUED|US_RESCUING|US_IN_BALLISTA|US_UNDER_A_ROOF|US_BIT16))return -2;
 if(GetPlayerSelectKind(u)!=PLAYER_SELECT_CONTROL)return -3;
 int mov=UNIT_MOV(u);if(mov<0||mov>=MOVE_CMD_MAX_COUNT-1)return -4;
 SyncMaps();UnitBeginAction(u);selected=index;floodError=0;GenerateUnitMovementMapExt(u,mov);
 if(floodError){u->state&=~US_HIDDEN;selected=0;lastError=floodError;return -5;}
 mode=1;fe8_map_focus(u->xPos,u->yPos);return 0;
}
int fe8_tactical_cost(int x,int y){return mode<0||x<0||y<0||x>=gBmMapSize.x||y>=gBmMapSize.y?-1:movement[y][x];}
#include "constants/characters.h"
struct MenuItemDef;
enum {MENU_ENABLED=1,MENU_DISABLED=2,MENU_NOTSHOWN=3,TILE_COMMAND_SEIZE=0x11,TILE_COMMAND_VISIT=0x10};
static int GetAvailableTileEventCommand(s8 x,s8 y){return fe8_event_tile_command(x,y);}
#include "generated/progress_seize.h"
static int IsUnitMagicSealed(struct Unit *u){return u->statusIndex==3;} // Current roster has no magic-seal aura; silence checked here.
#include "generated/progress_visit.h"
static int ActionMenu(int *ids,const char **labels);
static int HealTargets(int populate){if(mode!=3||!gActiveUnit||fe8_event_busy()||gActiveUnit->state&US_HAS_MOVED)return 0;int n=0;for(int i=1;i<256;i++)for(int slot=0;slot<5;slot++)if(fe8_battle_can_heal(selected,i,slot)){if(populate)targets[n]=(typeof(*targets)){i,slot};n++;}return n;}
int fe8_tactical_can_heal(void){return HealTargets(0)>0;}
int fe8_tactical_begin_heal(void){int n=HealTargets(1);if(!n)return -1;targetCount=n;targetChoice=0;mode=11;fe8_map_focus(GetUnit(targets[0].index)->xPos,GetUnit(targets[0].index)->yPos);return 0;}
int fe8_tactical_heal(void){if(mode!=11||targetChoice>=targetCount)return -1;int result=fe8_battle_heal(selected,targets[targetChoice].index,targets[targetChoice].slot);if(result<0)return result;fe8_audio_se(0x6a);mode=6;return result;}
static int ItemTargets(int populate){if(mode!=3||!gActiveUnit||fe8_event_busy()||gActiveUnit->curHP>=GetUnitMaxHp(gActiveUnit)||gActiveUnit->state&US_HAS_MOVED)return 0;int n=0;for(int slot=0;slot<5;slot++){int id=gActiveUnit->items[slot]&255;if(id!=ITEM_VULNERARY&&id!=ITEM_ELIXIR)continue;if(populate)targets[n]=(typeof(*targets)){selected,slot};n++;}return n;}
int fe8_tactical_can_item(void){return ItemTargets(0)>0;}
int fe8_tactical_begin_item(void){int n=ItemTargets(1);if(!n)return -1;targetCount=n;targetChoice=0;mode=10;return 0;}
int fe8_tactical_use_item(void){if(mode!=10||targetChoice>=targetCount||gActiveUnit->curHP>=GetUnitMaxHp(gActiveUnit))return -1;int result=fe8_recovery_item(selected,targets[targetChoice].slot);if(result<0)return result;fe8_audio_se(0x6a);mode=3;fe8_tactical_wait();return result;}
int fe8_tactical_can_visit(void){if(mode!=3||!gActiveUnit||fe8_event_busy())return 0;SyncMaps();return VisitCommandUsability(NULL,0)==MENU_ENABLED;}
int fe8_tactical_visit(void){if(!fe8_tactical_can_visit())return -1;int result=fe8_event_visit(selected);if(result<0)return result;fe8_audio_se(0x6a);mode=8;return 0;}
static int TalkTargets(int populate){if(mode!=3||!gActiveUnit||fe8_event_busy()||gActiveUnit->state&US_HAS_MOVED||gActiveUnit->statusIndex==3)return 0;int n=0;for(int y=gBmMapSize.y-1;y>=0;y--)for(int x=gBmMapSize.x-1;x>=0;x--){if(abs(x-gActiveUnit->xPos)+abs(y-gActiveUnit->yPos)!=1)continue;int index=fe8_unit_at(x,y);if(index<0||GetUnit(index)->statusIndex==2||GetUnit(index)->statusIndex==4||!fe8_event_talk_available(selected,index))continue;if(populate)targets[n]=(typeof(*targets)){index,-1};n++;}return n;}
int fe8_tactical_can_talk(void){return TalkTargets(0)>0;}
int fe8_tactical_begin_talk(void){int n=TalkTargets(1);if(!n)return -1;targetCount=n;targetChoice=0;mode=9;fe8_map_focus(GetUnit(targets[0].index)->xPos,GetUnit(targets[0].index)->yPos);return 0;}
int fe8_tactical_talk(void){if(mode!=9||targetChoice>=targetCount)return -1;int result=fe8_event_talk(selected,targets[targetChoice].index);if(result<0)return result;fe8_audio_se(0x6a);mode=8;return 0;}
int fe8_tactical_can_seize(void){return mode==3&&gActiveUnit&&gPlaySt.chapterIndex==1&&gPlaySt.chapterModeIndex>=1&&gPlaySt.chapterModeIndex<=3&&!fe8_event_busy()&&UnitActionMenu_CanSeize(NULL,0)==MENU_ENABLED;}
int fe8_tactical_seize(void){if(!fe8_tactical_can_seize())return -1;struct Unit *u=gActiveUnit;if(fe8_event_seize(selected)<0)return -2;u->state|=US_HAS_MOVED|US_UNSELECTABLE;fe8_audio_se(0x6a);mode=0;selected=0;gActiveUnit=NULL;return 0;}
static void FindTargets(void);
int fe8_tactical_move(int x,int y){
 if(mode!=1||x<0||y<0||x>=gBmMapSize.x||y>=gBmMapSize.y||!CanMoveActiveUnitTo(x,y))return -1;
 u8 path[MOVE_CMD_MAX_COUNT];floodError=0;GenerateBestMovementScript(x,y,path);if(floodError)return -2;
 u32 queue[128];int px=gActiveUnitMoveOrigin.x,py=gActiveUnitMoveOrigin.y,n=0;
 for(;n<MOVE_CMD_MAX_COUNT-1&&path[n]!=MOVE_CMD_HALT;n++){switch(path[n]){case 0:px--;break;case 1:px++;break;case 2:py++;break;case 3:py--;break;default:return -3;}queue[n*2]=px|(py<<6);queue[n*2+1]=0;}
 if(px!=x||py!=y)return -6;
 gActionData.moveCount=movement[y][x];menuChoice=0;
 if(n){if(fe8_unit_move_queue_index((u8)gActiveUnit->index,queue,n*2)<0)return -4;mode=2;}else{mode=3;FindTargets();}
 fe8_map_focus(x,y);return 0;
}
static void FindTargets(void){
 targetCount=targetChoice=0;if(!gActiveUnit)return;
 for(int i=1;i<256;i++)for(int slot=0;slot<5;slot++)if(fe8_battle_forecast(selected,i,slot,4)>=0){targets[targetCount].index=i;targets[targetCount++].slot=slot;}
}
int fe8_tactical_begin_attack(void){if(mode!=3)return -1;FindTargets();if(!targetCount)return -2;mode=5;menuChoice=0;fe8_map_focus(GetUnit(targets[0].index)->xPos,GetUnit(targets[0].index)->yPos);return 0;}
int fe8_tactical_next_target(void){if((mode!=5&&mode!=9&&mode!=10&&mode!=11)||!targetCount)return -1;targetChoice=(targetChoice+1)%targetCount;struct Unit *u=GetUnit(targets[targetChoice].index);fe8_map_focus(u->xPos,u->yPos);return 0;}
int fe8_tactical_attack(void){if(mode!=5||targetChoice>=targetCount)return -1;int result=fe8_battle_attack(selected,targets[targetChoice].index,targets[targetChoice].slot);if(result<0){lastError=10-result;return result;}mode=6;return result;}
int fe8_tactical_forecast(int field){if(mode!=5||targetChoice>=targetCount)return -1;return fe8_battle_forecast(selected,targets[targetChoice].index,targets[targetChoice].slot,field);}
int fe8_tactical_cancel(void){
 if(mode==5||mode==9||mode==10||mode==11){mode=3;menuChoice=0;FindTargets();fe8_map_focus(gActiveUnit->xPos,gActiveUnit->yPos);return 0;}

 if(mode==3){fe8_unit_move_index((u8)gActiveUnit->index,gActiveUnitMoveOrigin.x,gActiveUnitMoveOrigin.y);while(fe8_units_busy())fe8_units_tick();mode=1;fe8_map_focus(gActiveUnitMoveOrigin.x,gActiveUnitMoveOrigin.y);return 0;}
 if(mode==1){gActiveUnit->state&=~US_HIDDEN;mode=0;selected=0;return 0;}
 if(mode==4){mode=0;return 0;}return -1;
}
int fe8_tactical_end_turn(void){if(mode!=0&&mode!=4)return -1;fe8_ai_reset();ClearActiveFactionGrayedStates();SwitchPhases();fe8_recovery_phase(gPlaySt.faction);selected=0;gActiveUnit=NULL;mode=7;fe8_event_phase();return 0;}
int fe8_tactical_wait(void){if(mode!=3)return -1;gActiveUnit->state|=US_HAS_MOVED;MoveActiveUnit(gActiveUnit->xPos,gActiveUnit->yPos);mode=0;fe8_event_actor(selected);selected=0;gActiveUnit=NULL;fe8_event_post_action();if(!GetPhaseAbleUnitCount(FACTION_BLUE))fe8_tactical_end_turn();return 0;}
void fe8_tactical_tick(int keys){
 if(mode<0||fe8_event_busy())return;
 if(mode==8){mode=3;fe8_tactical_wait();return;}
 if(mode==2){if(!fe8_units_busy()){mode=3;FindTargets();}return;}
 if(mode==6){if(!fe8_battle_busy()){mode=3;fe8_tactical_wait();}return;}
 if(mode==5||mode==9||mode==10||mode==11){if(keys&2){fe8_tactical_cancel();return;}if(keys&1){if(mode==9)fe8_tactical_talk();else if(mode==10)fe8_tactical_use_item();else if(mode==11)fe8_tactical_heal();else fe8_tactical_attack();return;}if(keys&240){targetChoice=(targetChoice+((keys&80)?1:targetCount-1))%targetCount;struct Unit *u=GetUnit(targets[targetChoice].index);fe8_map_focus(u->xPos,u->yPos);}return;}
 if(mode==7){if(fe8_event_info(12))return;fe8_audio_map_start();if(gPlaySt.faction==FACTION_BLUE){mode=0;return;}if(fe8_ai_info(0)<0){fe8_ai_prepare();return;}if(fe8_ai_tick()==1){ClearActiveFactionGrayedStates();SwitchPhases();fe8_recovery_phase(gPlaySt.faction);fe8_ai_reset();fe8_event_phase();}return;}
 if(keys&2){fe8_tactical_cancel();return;}
 if(mode==3){int ids[10];const char *labels[10];int count=ActionMenu(ids,labels);if(menuChoice>=count)menuChoice=0;if(keys&1){switch(ids[menuChoice]){case 1:fe8_tactical_begin_attack();break;case 2:fe8_tactical_begin_talk();break;case 3:fe8_tactical_visit();break;case 4:fe8_tactical_seize();break;case 7:fe8_tactical_begin_heal();break;case 8:fe8_tactical_begin_item();break;case 5:fe8_tactical_wait();break;default:fe8_tactical_cancel();break;}}else if(keys&192)menuChoice=(menuChoice+((keys&128)?1:count-1))%count;return;}
 if(mode==4){if(keys&1){if(menuChoice)fe8_tactical_cancel();else fe8_tactical_end_turn();}else if(keys&192)menuChoice^=1;return;}
 if(keys&1){int x=fe8_map_info(3),y=fe8_map_info(4);if(mode==1)fe8_tactical_move(x,y);else{int index=fe8_unit_at(x,y);if(index>0&&GetPlayerSelectKind(GetUnit(index))==PLAYER_SELECT_CONTROL)fe8_tactical_select(index);else{mode=4;menuChoice=0;}}}
}
int fe8_tactical_consumes_back(void){return mode>=1&&mode<=11;}
int fe8_tactical_locks_cursor(void){return mode==2||mode==3||mode==4||mode==5||mode==6||mode==7||mode==8||mode==9||mode==10||mode==11;}
int fe8_tactical_show_active(int index){return selected==index&&mode>=1&&mode<=11;}
int fe8_tactical_info(int field){switch(field){case 0:return mode;case 1:return selected;case 2:return gPlaySt.faction;case 3:return gPlaySt.chapterTurnNumber;case 4:return menuChoice;case 5:return lastError;case 6:return GetPhaseAbleUnitCount(gPlaySt.faction);case 7:return gActionData.moveCount;case 8:return targetCount;case 9:return mode==5||mode==9||mode==10||mode==11?targets[targetChoice].index:0;case 10:return mode==5||mode==10||mode==11?targets[targetChoice].slot:-1;default:return -1;}}
void fe8_tactical_render_range(u8 *rgba,int cameraX,int cameraY,int frame){
 if(mode!=1)return;int phase=frame/2&31;
 for(int y=0;y<160;y++)for(int x=0;x<240;x++){
  int wx=x+cameraX,wy=y+cameraY,bx=wx>>4,by=wy>>4;if(bx>=gBmMapSize.x||by>=gBmMapSize.y||movement[by][bx]>=MAP_MOVEMENT_MAX)continue;
  int dx=wx&15,dy=wy&15,pos=((dy/8)*2+dx/8)*32+(dy&7)*4+(dx&7)/2,index=gUnknown_08A02EB4[pos]>>((dx&1)*4)&15;if(!index)continue;
  int color=gUnknown_08A02F34[phase+index-1];u8 *p=rgba+(y*240+x)*4;
  for(int c=0;c<3;c++){int v=(((color>>(c*5))&31)*10+(p[c]>>3)*6)>>4;if(v>31)v=31;p[c]=(v<<3)|(v>>2);}
 }
}

static void MenuPixel(u8 *rgba,int x,int y,int color){if(x<0||y<0||x>=240||y>=160)return;u8 *p=rgba+(y*240+x)*4;for(int c=0;c<3;c++){int v=color>>(c*5)&31;p[c]=(v<<3)|(v>>2);}p[3]=255;}
static void MenuText(u8 *rgba,const char *s,int x,int y,int highlight){for(;*s;s++){int code=(u8)*s;for(int dy=0;dy<16;dy++)for(int dx=0;dx<16;dx++){int ink=menuGlyphs[code].rows[dy]>>(dx*2)&3;if(ink)MenuPixel(rgba,x+dx,y+dy,menuTextPalette[ink+(highlight?9:0)]);}x+=menuGlyphs[code].width;}}
static int ActionMenu(int *ids,const char **labels){int n=0;
 #define ADD_ACTION(id,text) do{ids[n]=id;labels[n++]=text;}while(0)
 if(targetCount)ADD_ACTION(1,tacticalAttack);if(fe8_tactical_can_heal())ADD_ACTION(7,tacticalStaff);if(fe8_tactical_can_item())ADD_ACTION(8,tacticalItem);if(fe8_tactical_can_talk())ADD_ACTION(2,tacticalTalk);if(fe8_tactical_can_visit())ADD_ACTION(3,tacticalVisit);if(fe8_tactical_can_seize())ADD_ACTION(4,tacticalSeize);ADD_ACTION(5,tacticalWait);ADD_ACTION(6,tacticalCancel);
 #undef ADD_ACTION
 return n;
}
void fe8_tactical_render_menu(u8 *rgba){
 if(mode!=3&&mode!=4)return;int ids[10];const char *labels[10];int rows=mode==3?ActionMenu(ids,labels):2,height=rows*2+2;u16 map[1024]={0};DrawUiFrame(map,20,1,9,height,0,0);
 for(int y=8;y<8+height*8;y++)for(int x=160;x<232;x++){int ref=map[(y/8)*32+x/8],dx=ref&1024?7-(x&7):x&7,dy=ref&2048?7-(y&7):y&7,pos=(ref&1023)*32+dy*4+dx/2;if(pos>=sizeof(gUiFrameImage))continue;int index=gUiFrameImage[pos]>>((dx&1)*4)&15;if(index)MenuPixel(rgba,x,y,gUiFramePaletteA[index]);}
 if(mode==3){for(int row=0;row<rows;row++)MenuText(rgba,labels[row],174,16+row*16,menuChoice==row);}else{MenuText(rgba,tacticalEnd,174,16,!menuChoice);MenuText(rgba,tacticalCancel,174,32,menuChoice);}
}

int fe8_tactical_probe_movement(struct Unit *u,int x,int y,int limit,int id,u8 *out){
 if(!u||!UNIT_IS_VALID(u)||!out||!fe8_map_active()||limit<0||limit>124||x<0||y<0||x>=fe8_map_info(1)||y>=fe8_map_info(2))return -1;
 u8 backup[4096],costs[65];memcpy(backup,movement,4096);memcpy(costs,gWorkingTerrainMoveCosts,65);u8 **working=gWorkingBmMap;struct MovMapFillState fill=gMovMapFillState;int oldError=floodError;
 SyncMaps();SetWorkingMoveCosts(GetUnitMovementCost(u));SetWorkingBmMap(gBmMapMovement);floodError=0;GenerateMovementMap(x,y,limit,id);int result=floodError?-2:0;if(!result)memcpy(out,movement,4096);
 memcpy(movement,backup,4096);memcpy(gWorkingTerrainMoveCosts,costs,65);gWorkingBmMap=working;gMovMapFillState=fill;floodError=oldError;return result;
}

int fe8_tactical_npc_move(int index,int x,int y){
 if(mode!=7||index<1||index>255)return -1;struct Unit *u=GetUnit(index);SyncMaps();if(!UNIT_IS_VALID(u)||x<0||y<0||x>=gBmMapSize.x||y>=gBmMapSize.y)return -2;
 struct Unit *old=gActiveUnit;u8 oldId=gActiveUnitId;struct Vec2 origin=gActiveUnitMoveOrigin;gActiveUnit=u;gActiveUnitId=index;gActiveUnitMoveOrigin.x=u->xPos;gActiveUnitMoveOrigin.y=u->yPos;floodError=0;GenerateUnitMovementMapExt(u,UNIT_MOV(u));
 if(movement[y][x]>120||(occupancy[y][x]&&occupancy[y][x]!=index)){gActiveUnit=old;gActiveUnitId=oldId;gActiveUnitMoveOrigin=origin;return -3;}
 u8 path[MOVE_CMD_MAX_COUNT];GenerateBestMovementScript(x,y,path);int px=u->xPos,py=u->yPos,n=0;u32 queue[128];
 for(;n<MOVE_CMD_MAX_COUNT-1&&path[n]!=MOVE_CMD_HALT;n++){switch(path[n]){case 0:px--;break;case 1:px++;break;case 2:py++;break;case 3:py--;break;default:floodError=4;break;}queue[n*2]=px|(py<<6);queue[n*2+1]=0;}
 gActiveUnit=old;gActiveUnitId=oldId;gActiveUnitMoveOrigin=origin;if(floodError||px!=x||py!=y)return -4;return n?fe8_unit_move_queue_index(index,queue,n*2):0;
}
