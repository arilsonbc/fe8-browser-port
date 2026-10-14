#include "tactical.h"
#include "proc.h"
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
#include <stdio.h>
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

static int mode=-1,selected,menuChoice,lastError,floodError,cantoActive,eventAction=1;
static int FinishAction(int action);
static void FindTargets(void);
static struct {int index,slot,x,y;} targets[255*5];
static int targetCount,targetChoice;

static u8 terrain[64][64],occupancy[64][64],movement[64][64],hiddenMap[64][64];
static u8 *gBmMapHidden[64];
#define HIDDEN_BIT_UNIT 1
#define MAP_MOVEMENT_EXTENDED 124
#define RECT_DISTANCE(x1,y1,x2,y2) (abs((x1)-(x2))+abs((y1)-(y2)))
static u8 *gBmMapTerrain[64],*gBmMapUnit[64],*gBmMapMovement[64],**gWorkingBmMap;
static u8 gWorkingTerrainMoveCosts[65];
static struct MovMapFillStateExt gMovMapFillStPool1[8192],gMovMapFillStPool2[8192],*poolEnd;
static struct MovMapFillState gMovMapFillState;
static void SetWorkingBmMap(u8 **map){gWorkingBmMap=map;}
static void BmMapFill(u8 **map,int value){for(int y=0;y<gBmMapSize.y;y++)memset(map[y],value,gBmMapSize.x);}
static void CallARM_FillMovementMap(void);
#define TILEMAP_INDEX(x,y) ((y)*32+(x))
#include "generated/menu_font.h"
struct TradeMenuProc {struct Unit *units[2];int hoverColumn,hoverRow,selectedColumn,selectedRow,hasTraded;};
enum {UNIT_ACTION_WAIT=1,UNIT_ACTION_COMBAT=2,UNIT_ACTION_STAFF=3,UNIT_ACTION_TRADED=0x1b};
static void TradeMenu_RefreshItemText(struct TradeMenuProc *proc){(void)proc;} // Native renderer reads inventory every frame.
extern void UnitRemoveInvalidItems(struct Unit *u);
extern int GetUnitItemCount(struct Unit *u);
static struct Unit *gSubjectUnit;
static int tradeCollectCount,tradeCollectPopulate,tradePeer,tradeColumn,tradeRow,tradeChosenColumn=-1,tradeChosenRow,hasTraded;
static void AddTarget(int x,int y,int index,int extra){(void)x;(void)y;(void)extra;if(tradeCollectCount>=255)return;if(tradeCollectPopulate)targets[tradeCollectCount]=(typeof(*targets)){index,-1,x,y};tradeCollectCount++;}
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
 for(int y=0;y<64;y++){gBmMapHidden[y]=hiddenMap[y];gBmMapTerrain[y]=terrain[y];gBmMapUnit[y]=occupancy[y];gBmMapMovement[y]=movement[y];for(int x=0;x<64;x++){int t=fe8_map_terrain(x,y);terrain[y][x]=t<0?0:t;}}
 for(int i=1;i<256;i++){struct Unit *u=GetUnit(i);if(UNIT_IS_VALID(u)&&!(u->state&(US_HIDDEN|US_DEAD|US_NOT_DEPLOYED|US_RESCUED))&&u->xPos>=0&&u->yPos>=0&&u->xPos<gBmMapSize.x&&u->yPos<gBmMapSize.y)occupancy[u->yPos][u->xPos]=i;}
}
void fe8_tactical_reset(void){cantoActive=0;eventAction=1;tradePeer=tradeColumn=tradeRow=tradeChosenRow=hasTraded=0;tradeChosenColumn=-1;fe8_recovery_reset();fe8_ai_reset();targetCount=targetChoice=0;gActiveUnit=NULL;gActiveUnitId=selected=menuChoice=lastError=floodError=0;mode=-1;memset(movement,255,sizeof(movement));memset(squaresMoved,0,sizeof(squaresMoved));}
int fe8_tactical_start(void){if(mode==0&&!fe8_event_busy())return 0;if(fe8_event_busy()||!fe8_map_active()||mode>0)return -1;SyncMaps();fe8_event_show_icons();gPlaySt.faction=FACTION_BLUE;if(!gPlaySt.chapterTurnNumber)gPlaySt.chapterTurnNumber=1;mode=0;selected=lastError=0;fe8_recovery_phase(FACTION_BLUE);fe8_audio_map_start();if(gPlaySt.chapterIndex==1||gPlaySt.chapterIndex==2)fe8_event_phase();return 0;}
int fe8_tactical_select(int index){
 if(mode!=0||gPlaySt.faction!=FACTION_BLUE||fe8_event_busy()||index<1||index>255)return -1;
 struct Unit *u=GetUnit(index);if(!UNIT_IS_VALID(u)||u->xPos<0||u->yPos<0||u->xPos>=gBmMapSize.x||u->yPos>=gBmMapSize.y||u->state&(US_DEAD|US_HIDDEN|US_NOT_DEPLOYED|US_RESCUED|US_IN_BALLISTA|US_UNDER_A_ROOF|US_BIT16))return -2;
 if(GetPlayerSelectKind(u)!=PLAYER_SELECT_CONTROL)return -3;
 int mov=UNIT_MOV(u);if(mov<0||mov>=MOVE_CMD_MAX_COUNT-1)return -4;
 SyncMaps();UnitBeginAction(u);hasTraded=0;gBmSt.taken_action=0;gActionData.unitActionType=0;selected=index;floodError=0;GenerateUnitMovementMapExt(u,mov);
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
int fe8_tactical_use_item(void){if(mode!=10||targetChoice>=targetCount||gActiveUnit->curHP>=GetUnitMaxHp(gActiveUnit))return -1;int result=fe8_recovery_item(selected,targets[targetChoice].slot);if(result<0)return result;fe8_audio_se(0x6a);mode=3;FinishAction(0x1a);return result;}
int fe8_tactical_can_visit(void){if(mode!=3||!gActiveUnit||fe8_event_busy())return 0;SyncMaps();return VisitCommandUsability(NULL,0)==MENU_ENABLED;}
int fe8_tactical_visit(void){if(!fe8_tactical_can_visit())return -1;int result=fe8_event_visit(selected);if(result<0)return result;fe8_audio_se(0x6a);eventAction=14;eventAction=16;mode=8;return 0;}
static int TalkTargets(int populate){if(mode!=3||!gActiveUnit||fe8_event_busy()||gActiveUnit->state&US_HAS_MOVED||gActiveUnit->statusIndex==3)return 0;int n=0;for(int y=gBmMapSize.y-1;y>=0;y--)for(int x=gBmMapSize.x-1;x>=0;x--){if(abs(x-gActiveUnit->xPos)+abs(y-gActiveUnit->yPos)!=1)continue;int index=fe8_unit_at(x,y);if(index<0||GetUnit(index)->statusIndex==2||GetUnit(index)->statusIndex==4||!fe8_event_talk_available(selected,index))continue;if(populate)targets[n]=(typeof(*targets)){index,-1};n++;}return n;}
static int RescueTargets(int populate,int drop){if(mode!=3||!gActiveUnit||fe8_event_busy()||gActiveUnit->state&US_HAS_MOVED)return 0;if(drop){if(!(gActiveUnit->state&US_RESCUING)||!UNIT_IS_VALID(GetUnit(gActiveUnit->rescue)))return 0;}else if(gActiveUnit->state&(US_RESCUING|US_IN_BALLISTA))return 0;SyncMaps();gSubjectUnit=gActiveUnit;tradeCollectCount=0;tradeCollectPopulate=populate;for(int y=gBmMapSize.y-1;y>=0;y--)for(int x=gBmMapSize.x-1;x>=0;x--){if(abs(x-gActiveUnit->xPos)+abs(y-gActiveUnit->yPos)!=1)continue;if(drop)TryAddToDropTargetList(x,y);else{int index=fe8_unit_at(x,y);if(index>0)TryAddUnitToRescueTargetList(GetUnit(index));}}return tradeCollectCount;}
int fe8_tactical_can_rescue(void){return RescueTargets(0,0)>0;}
int fe8_tactical_can_drop(void){return RescueTargets(0,1)>0;}
int fe8_tactical_begin_rescue(void){int n=RescueTargets(1,0);if(!n)return -1;targetCount=n;targetChoice=0;mode=14;fe8_map_focus(targets[0].x,targets[0].y);return 0;}
int fe8_tactical_begin_drop(void){int n=RescueTargets(1,1);if(!n)return -1;targetCount=n;targetChoice=0;mode=15;fe8_map_focus(targets[0].x,targets[0].y);return 0;}
int fe8_tactical_rescue(void){if(mode!=14||targetChoice>=targetCount)return -1;int target=targets[targetChoice].index;struct Unit *u=GetUnit(target);if(!UNIT_IS_VALID(u)||!CanUnitRescue(gActiveUnit,u))return -2;UnitRescue(gActiveUnit,u);fe8_unit_sync_position(target);fe8_audio_se(0x6a);mode=3;return FinishAction(9);}
int fe8_tactical_drop(void){if(mode!=15||targetChoice>=targetCount)return -1;int x=targets[targetChoice].x,y=targets[targetChoice].y,index=gActiveUnit->rescue;SyncMaps();if(occupancy[y][x]||!CanUnitCrossTerrain(GetUnit(index),terrain[y][x]))return -2;UnitDrop(gActiveUnit,x,y);fe8_unit_sync_position(index);fe8_audio_se(0x6a);mode=3;return FinishAction(10);}
int fe8_tactical_rescue_death(int actor,int target){struct Unit *old=gActiveUnit;int ids[2]={actor,target};gActiveUnit=GetUnit(actor);for(int side=0;side<2;side++){struct Unit *u=GetUnit(ids[side]);if(!UNIT_IS_VALID(u)||u->curHP||!(u->state&US_RESCUING))continue;int passenger=u->rescue;if(!UNIT_IS_VALID(GetUnit(passenger))){gActiveUnit=old;return -1;}SyncMaps();int x=-1,y=-1;floodError=0;UnitGetDeathDropLocation(u,&x,&y);if(floodError||x<0||y<0){gActiveUnit=old;return -2;}UnitDrop(u,x,y);fe8_unit_sync_position(passenger);fe8_audio_se(0xac);}gActiveUnit=old;SyncMaps();return 0;}
static int TransferTargets(int populate,int give){if(mode!=3||!gActiveUnit||fe8_event_busy()||gActiveUnit->state&US_HAS_MOVED||gBmSt.taken_action&4)return 0;if(give){if(!(gActiveUnit->state&US_RESCUING))return 0;}else if(gActiveUnit->state&US_RESCUING)return 0;SyncMaps();gSubjectUnit=gActiveUnit;tradeCollectCount=0;tradeCollectPopulate=populate;for(int y=gBmMapSize.y-1;y>=0;y--)for(int x=gBmMapSize.x-1;x>=0;x--){if(abs(x-gActiveUnit->xPos)+abs(y-gActiveUnit->yPos)!=1)continue;int index=fe8_unit_at(x,y);if(index<0)continue;if(give)TryAddUnitToGiveTargetList(GetUnit(index));else TryAddRescuedUnitToTakeTargetList(GetUnit(index));}return tradeCollectCount;}
int fe8_tactical_can_take(void){return TransferTargets(0,0)>0;}
int fe8_tactical_can_give(void){return TransferTargets(0,1)>0;}
int fe8_tactical_begin_take(void){int n=TransferTargets(1,0);if(!n)return -1;targetCount=n;targetChoice=0;mode=17;fe8_map_focus(targets[0].x,targets[0].y);return 0;}
int fe8_tactical_begin_give(void){int n=TransferTargets(1,1);if(!n)return -1;targetCount=n;targetChoice=0;mode=18;fe8_map_focus(targets[0].x,targets[0].y);return 0;}
static int TransferPassenger(int give){if(mode!=(give?18:17)||targetChoice>=targetCount)return -1;struct Unit *peer=GetUnit(targets[targetChoice].index),*from=give?gActiveUnit:peer,*to=give?peer:gActiveUnit;if(!UNIT_IS_VALID(from)||!UNIT_IS_VALID(to)||!(from->state&US_RESCUING)||to->state&US_RESCUING||!IsSameAllegiance(from->index,to->index)||abs(from->xPos-to->xPos)+abs(from->yPos-to->yPos)!=1)return -2;int passenger=from->rescue;if(!CanUnitRescue(to,GetUnit(passenger)))return -3;UnitGive(from,to);fe8_unit_sync_position(passenger);gActionData.unitActionType=give?12:11;gBmSt.taken_action|=4;mode=3;menuChoice=0;FindTargets();fe8_audio_se(0x6a);return 0;}
int fe8_tactical_take(void){return TransferPassenger(0);}
int fe8_tactical_give(void){return TransferPassenger(1);}
static int TradeTargets(int populate){if(mode!=3||!gActiveUnit||fe8_event_busy()||gActiveUnit->state&US_HAS_MOVED)return 0;gSubjectUnit=gActiveUnit;tradeCollectCount=0;tradeCollectPopulate=populate;for(int y=gBmMapSize.y-1;y>=0;y--)for(int x=gBmMapSize.x-1;x>=0;x--){if(abs(x-gActiveUnit->xPos)+abs(y-gActiveUnit->yPos)!=1)continue;int index=fe8_unit_at(x,y);if(index<0)continue;TryAddUnitToTradeTargetList(GetUnit(index));}if(gActiveUnit->state&US_RESCUING)TryAddUnitToTradeTargetList(GetUnit(gActiveUnit->rescue));return tradeCollectCount;}
int fe8_tactical_can_trade(void){return TradeTargets(0)>0;}
int fe8_tactical_begin_trade(void){int n=TradeTargets(1);if(!n)return -1;targetCount=n;targetChoice=0;mode=12;return 0;}
int fe8_tactical_trade_target(void){if(mode!=12||targetChoice>=targetCount)return -1;tradePeer=targets[targetChoice].index;tradeColumn=tradeRow=tradeChosenRow=0;tradeChosenColumn=-1;mode=13;return 0;}
int fe8_tactical_trade_info(int f){return f==0?tradePeer:f==1?tradeColumn:f==2?tradeRow:f==3?tradeChosenColumn:f==4?tradeChosenRow:f==5?hasTraded:-1;}
int fe8_tactical_trade_swap(int a,int b){if(mode!=13||a<0||a>=5||b<0||b>=5)return -1;struct Unit *peer=GetUnit(tradePeer);if(!UNIT_IS_VALID(peer)||!gActiveUnit||!IsSameAllegiance(selected,tradePeer)||(abs(peer->xPos-gActiveUnit->xPos)+abs(peer->yPos-gActiveUnit->yPos)!=1&&(!(gActiveUnit->state&US_RESCUING)||gActiveUnit->rescue!=tradePeer)))return -2;if(!gActiveUnit->items[a]&&!peer->items[b])return -3;struct TradeMenuProc proc={.units={gActiveUnit,peer},.hoverColumn=1,.hoverRow=b,.selectedColumn=0,.selectedRow=a};TradeMenu_ApplyItemSwap(&proc);hasTraded=proc.hasTraded;gBmSt.taken_action|=1;tradeChosenColumn=-1;tradeColumn=tradeRow=0;fe8_audio_se(0x6a);return 0;}
int fe8_tactical_trade_choose(int column,int row){if(mode!=13||column<0||column>1||row<0||row>=5)return -1;struct Unit *u=column?GetUnit(tradePeer):gActiveUnit;int count=GetUnitItemCount(u);if(row>count||tradeChosenColumn<0&&!u->items[row])return -2;tradeColumn=column;tradeRow=row;if(tradeChosenColumn<0){tradeChosenColumn=column;tradeChosenRow=row;tradeColumn=column^1;tradeRow=0;return 0;}if(column==tradeChosenColumn){tradeChosenRow=row;return 0;}return fe8_tactical_trade_swap(column?tradeChosenRow:row,column?row:tradeChosenRow);}
int fe8_tactical_can_talk(void){return TalkTargets(0)>0;}
int fe8_tactical_begin_talk(void){int n=TalkTargets(1);if(!n)return -1;targetCount=n;targetChoice=0;mode=9;fe8_map_focus(GetUnit(targets[0].index)->xPos,GetUnit(targets[0].index)->yPos);return 0;}
int fe8_tactical_talk(void){if(mode!=9||targetChoice>=targetCount)return -1;int result=fe8_event_talk(selected,targets[targetChoice].index);if(result<0)return result;fe8_audio_se(0x6a);eventAction=14;mode=8;return 0;}
int fe8_tactical_can_seize(void){return mode==3&&gActiveUnit&&gPlaySt.chapterIndex==1&&gPlaySt.chapterModeIndex>=1&&gPlaySt.chapterModeIndex<=3&&!fe8_event_busy()&&UnitActionMenu_CanSeize(NULL,0)==MENU_ENABLED;}
int fe8_tactical_seize(void){if(!fe8_tactical_can_seize())return -1;struct Unit *u=gActiveUnit;if(fe8_event_seize(selected)<0)return -2;u->state|=US_HAS_MOVED|US_UNSELECTABLE;fe8_audio_se(0x6a);mode=0;selected=0;gActiveUnit=NULL;return 0;}
static void FindTargets(void);
int fe8_tactical_move(int x,int y){
 if((mode!=1&&mode!=16)||x<0||y<0||x>=gBmMapSize.x||y>=gBmMapSize.y||!CanMoveActiveUnitTo(x,y))return -1;
 u8 path[MOVE_CMD_MAX_COUNT];floodError=0;GenerateBestMovementScript(x,y,path);if(floodError)return -2;
 u32 queue[128];int px=gActiveUnitMoveOrigin.x,py=gActiveUnitMoveOrigin.y,n=0;
 for(;n<MOVE_CMD_MAX_COUNT-1&&path[n]!=MOVE_CMD_HALT;n++){switch(path[n]){case 0:px--;break;case 1:px++;break;case 2:py++;break;case 3:py--;break;default:return -3;}queue[n*2]=px|(py<<6);queue[n*2+1]=0;}
 if(px!=x||py!=y)return -6;
 gActionData.moveCount=movement[y][x];menuChoice=0;
 if(n){if(fe8_unit_move_queue_index((u8)gActiveUnit->index,queue,n*2)<0)return -4;mode=2;}else if(cantoActive){FinishAction(UNIT_ACTION_WAIT);}else{mode=3;FindTargets();}
 fe8_map_focus(x,y);return 0;
}
static void FindTargets(void){
 targetCount=targetChoice=0;if(!gActiveUnit)return;
 for(int i=1;i<256;i++)for(int slot=0;slot<5;slot++)if(fe8_battle_forecast(selected,i,slot,4)>=0){targets[targetCount].index=i;targets[targetCount++].slot=slot;}
}
int fe8_tactical_begin_attack(void){if(mode!=3)return -1;FindTargets();if(!targetCount)return -2;mode=5;menuChoice=0;fe8_map_focus(GetUnit(targets[0].index)->xPos,GetUnit(targets[0].index)->yPos);return 0;}
int fe8_tactical_next_target(void){if((mode!=5&&mode!=9&&mode!=10&&mode!=11&&mode!=12&&mode!=14&&mode!=15&&mode!=17&&mode!=18)||!targetCount)return -1;targetChoice=(targetChoice+1)%targetCount;if(mode==15)fe8_map_focus(targets[targetChoice].x,targets[targetChoice].y);else{struct Unit *u=GetUnit(targets[targetChoice].index);fe8_map_focus(u->xPos,u->yPos);}return 0;}
int fe8_tactical_attack(void){if(mode!=5||targetChoice>=targetCount)return -1;int result=fe8_battle_attack(selected,targets[targetChoice].index,targets[targetChoice].slot);if(result<0){lastError=10-result;return result;}mode=6;return result;}
int fe8_tactical_forecast(int field){if(mode!=5||targetChoice>=targetCount)return -1;return fe8_battle_forecast(selected,targets[targetChoice].index,targets[targetChoice].slot,field);}
int fe8_tactical_cancel(void){
 if(mode==16){cantoActive=0;return FinishAction(UNIT_ACTION_WAIT);}
 if(mode==13){if(tradeChosenColumn>=0){tradeChosenColumn=-1;return 0;}mode=3;menuChoice=0;FindTargets();return 0;}
 if(mode==5||mode==9||mode==10||mode==11||mode==12||mode==14||mode==15||mode==17||mode==18){mode=3;menuChoice=0;FindTargets();fe8_map_focus(gActiveUnit->xPos,gActiveUnit->yPos);return 0;}

 if(mode==3&&gBmSt.taken_action)return FinishAction(0x1f);
 if(mode==3){fe8_unit_move_index((u8)gActiveUnit->index,gActiveUnitMoveOrigin.x,gActiveUnitMoveOrigin.y);while(fe8_units_busy())fe8_units_tick();mode=1;fe8_map_focus(gActiveUnitMoveOrigin.x,gActiveUnitMoveOrigin.y);return 0;}
 if(mode==1){gActiveUnit->state&=~US_HIDDEN;mode=0;selected=0;return 0;}
 if(mode==4){mode=0;return 0;}return -1;
}
int fe8_tactical_end_turn(void){if(mode!=0&&mode!=4)return -1;fe8_ai_reset();ClearActiveFactionGrayedStates();SwitchPhases();fe8_recovery_phase(gPlaySt.faction);selected=0;gActiveUnit=NULL;mode=7;fe8_event_phase();return 0;}
static int FinishAction(int action){if(!gActiveUnit)return -1;gActionData.unitActionType=action;MoveActiveUnit(gActiveUnit->xPos,gActiveUnit->yPos);fe8_event_actor(selected);fe8_event_post_action();SyncMaps();if(!cantoActive&&TryMakeCantoUnit(NULL)){cantoActive=1;floodError=0;GenerateUnitMovementMapExt(gActiveUnit,UNIT_MOV(gActiveUnit)-gActionData.moveCount);if(!floodError){mode=16;fe8_map_focus(gActiveUnit->xPos,gActiveUnit->yPos);return 0;}lastError=floodError;}cantoActive=0;gActiveUnit->state|=US_HAS_MOVED;mode=0;selected=0;gActiveUnit=NULL;if(!GetPhaseAbleUnitCount(FACTION_BLUE))fe8_tactical_end_turn();return 0;}
int fe8_tactical_wait(void){return mode==3?FinishAction(UNIT_ACTION_WAIT):-1;}
void fe8_tactical_tick(int keys){
 if(mode<0||fe8_event_busy())return;
 if(mode==8){mode=3;FinishAction(eventAction);return;}
 if(mode==2){if(!fe8_units_busy()){if(cantoActive)FinishAction(UNIT_ACTION_WAIT);else{mode=3;FindTargets();}}return;}
 if(mode==6){if(!fe8_battle_busy()){mode=3;FinishAction(fe8_battle_info(18)==2?UNIT_ACTION_STAFF:UNIT_ACTION_COMBAT);}return;}
 if(mode==13){if(keys&2){fe8_tactical_cancel();return;}if(keys&1){fe8_tactical_trade_choose(tradeColumn,tradeRow);return;}if(keys&48){if(tradeChosenColumn<0){tradeColumn^=1;int count=GetUnitItemCount(tradeColumn?GetUnit(tradePeer):gActiveUnit);if(tradeRow>=count)tradeRow=count?count-1:0;}}if(keys&192){struct Unit *u=tradeColumn?GetUnit(tradePeer):gActiveUnit;int count=GetUnitItemCount(u),rows=tradeChosenColumn<0?count:count<5?count+1:5;if(rows>0)tradeRow=(tradeRow+((keys&128)?1:rows-1))%rows;}return;}
 if(mode==5||mode==9||mode==10||mode==11||mode==12||mode==14||mode==15||mode==17||mode==18){if(keys&2){fe8_tactical_cancel();return;}if(keys&1){if(mode==9)fe8_tactical_talk();else if(mode==10)fe8_tactical_use_item();else if(mode==11)fe8_tactical_heal();else if(mode==12)fe8_tactical_trade_target();else if(mode==14)fe8_tactical_rescue();else if(mode==15)fe8_tactical_drop();else if(mode==17)fe8_tactical_take();else if(mode==18)fe8_tactical_give();else fe8_tactical_attack();return;}if(keys&240){targetChoice=(targetChoice+((keys&80)?1:targetCount-1))%targetCount;if(mode==15)fe8_map_focus(targets[targetChoice].x,targets[targetChoice].y);else{struct Unit *u=GetUnit(targets[targetChoice].index);fe8_map_focus(u->xPos,u->yPos);}}return;}
 if(mode==7){if(fe8_event_info(12))return;fe8_audio_map_start();if(gPlaySt.faction==FACTION_BLUE){mode=0;return;}if(fe8_ai_info(0)<0){fe8_ai_prepare();return;}if(fe8_ai_tick()==1){ClearActiveFactionGrayedStates();SwitchPhases();fe8_recovery_phase(gPlaySt.faction);fe8_ai_reset();fe8_event_phase();}return;}
 if(keys&2){fe8_tactical_cancel();return;}
 if(mode==3){int ids[12];const char *labels[12];int count=ActionMenu(ids,labels);if(menuChoice>=count)menuChoice=0;if(keys&1){switch(ids[menuChoice]){case 1:fe8_tactical_begin_attack();break;case 2:fe8_tactical_begin_talk();break;case 3:fe8_tactical_visit();break;case 4:fe8_tactical_seize();break;case 7:fe8_tactical_begin_heal();break;case 8:fe8_tactical_begin_item();break;case 9:fe8_tactical_begin_trade();break;case 10:fe8_tactical_begin_rescue();break;case 11:fe8_tactical_begin_drop();break;case 12:fe8_tactical_begin_take();break;case 13:fe8_tactical_begin_give();break;case 5:fe8_tactical_wait();break;default:fe8_tactical_cancel();break;}}else if(keys&192)menuChoice=(menuChoice+((keys&128)?1:count-1))%count;return;}
 if(mode==4){if(keys&1){if(menuChoice)fe8_tactical_cancel();else fe8_tactical_end_turn();}else if(keys&192)menuChoice^=1;return;}
 if(keys&1){int x=fe8_map_info(3),y=fe8_map_info(4);if(mode==1||mode==16)fe8_tactical_move(x,y);else{int index=fe8_unit_at(x,y);if(index>0&&GetPlayerSelectKind(GetUnit(index))==PLAYER_SELECT_CONTROL)fe8_tactical_select(index);else{mode=4;menuChoice=0;}}}
}
int fe8_tactical_consumes_back(void){return mode>=1&&mode<=18;}
int fe8_tactical_locks_cursor(void){return mode==2||mode==3||mode==4||mode==5||mode==6||mode==7||mode==8||mode==9||mode==10||mode==11||mode==12||mode==13||mode==14||mode==15||mode==17||mode==18;}
int fe8_tactical_show_active(int index){return selected==index&&mode>=1&&mode<=18;}
int fe8_tactical_info(int field){switch(field){case 0:return mode;case 1:return selected;case 2:return gPlaySt.faction;case 3:return gPlaySt.chapterTurnNumber;case 4:return menuChoice;case 5:return lastError;case 6:return GetPhaseAbleUnitCount(gPlaySt.faction);case 7:return gActionData.moveCount;case 8:return targetCount;case 9:return mode==5||mode==9||mode==10||mode==11||mode==12||mode==14||mode==17||mode==18?targets[targetChoice].index:0;case 16:return cantoActive;case 14:return mode==15?targets[targetChoice].x:-1;case 15:return mode==15?targets[targetChoice].y:-1;case 10:return mode==5||mode==10||mode==11||mode==12||mode==14||mode==17||mode==18?targets[targetChoice].slot:-1;default:return -1;}}
void fe8_tactical_render_range(u8 *rgba,int cameraX,int cameraY,int frame){
 if(mode!=1&&mode!=16)return;int phase=frame/2&31;
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
 if(targetCount)ADD_ACTION(1,tacticalAttack);if(fe8_tactical_can_heal())ADD_ACTION(7,tacticalStaff);if(fe8_tactical_can_item())ADD_ACTION(8,tacticalItem);if(fe8_tactical_can_trade())ADD_ACTION(9,tacticalTrade);if(fe8_tactical_can_rescue())ADD_ACTION(10,tacticalRescue);if(fe8_tactical_can_drop())ADD_ACTION(11,tacticalDrop);if(fe8_tactical_can_take())ADD_ACTION(12,tacticalTake);if(fe8_tactical_can_give())ADD_ACTION(13,tacticalGive);if(fe8_tactical_can_talk())ADD_ACTION(2,tacticalTalk);if(fe8_tactical_can_visit())ADD_ACTION(3,tacticalVisit);if(fe8_tactical_can_seize())ADD_ACTION(4,tacticalSeize);ADD_ACTION(5,tacticalWait);ADD_ACTION(6,tacticalCancel);
 #undef ADD_ACTION
 return n;
}
void fe8_tactical_render_menu(u8 *rgba){
 if(mode!=3&&mode!=4)return;int ids[12];const char *labels[12];int rows=mode==3?ActionMenu(ids,labels):2,height=rows*2+2;u16 map[1024]={0};DrawUiFrame(map,20,1,9,height,0,0);
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

// Native choice presentation; original frame/font/names and English message IDs retained.
void fe8_tactical_render_item_menu(u8 *rgba){
 int incoming=fe8_event_item_info(0);if(!incoming)return;int index=fe8_event_item_info(1),choice=fe8_event_item_info(2),supply=fe8_event_item_info(3);struct Unit *u=GetUnit(index);if(!UNIT_IS_VALID(u))return;u16 map[1024]={0};DrawUiFrame(map,3,1,24,18,0,0);
 for(int y=8;y<152;y++)for(int x=24;x<216;x++){int ref=map[(y/8)*32+x/8],dx=ref&1024?7-(x&7):x&7,dy=ref&2048?7-(y&7):y&7,pos=(ref&1023)*32+dy*4+dx/2;if(pos>=sizeof(gUiFrameImage))continue;int ink=gUiFrameImage[pos]>>((dx&1)*4)&15;if(ink)MenuPixel(rgba,x,y,gUiFramePaletteA[ink]);}
 const char *message=supply?tacticalInventorySupply:tacticalInventoryFull;char title[96];snprintf(title,sizeof(title),"%s",message);char *next=strchr(title,'.');if(next){*next=0;next++;while(*next==' ')next++;}MenuText(rgba,title,36,14,0);if(next)MenuText(rgba,next,36,30,0);
 for(int row=0;row<6;row++){int item=row==5?incoming:u->items[row];const char *name=tacticalItemNames[item&255];char uses[8];snprintf(uses,sizeof(uses),"%d",item>>8);if(row==choice)MenuText(rgba,">",30,50+row*16,1);MenuText(rgba,name?name:"",42,50+row*16,row==choice);MenuText(rgba,uses,186,50+row*16,row==choice);}
}

void fe8_tactical_render_trade(u8 *rgba){if(mode!=13||!gActiveUnit)return;struct Unit *units[2]={gActiveUnit,GetUnit(tradePeer)};u16 map[1024]={0};DrawUiFrame(map,1,3,14,15,0,0);DrawUiFrame(map,15,3,14,15,0,0);for(int y=24;y<144;y++)for(int x=8;x<232;x++){int ref=map[(y/8)*32+x/8],dx=ref&1024?7-(x&7):x&7,dy=ref&2048?7-(y&7):y&7,pos=(ref&1023)*32+dy*4+dx/2;if(pos>=sizeof(gUiFrameImage))continue;int ink=gUiFrameImage[pos]>>((dx&1)*4)&15;if(ink)MenuPixel(rgba,x,y,gUiFramePaletteA[ink]);}for(int col=0;col<2;col++){MenuText(rgba,tacticalTrade,20+col*112,30,0);for(int row=0;row<5;row++){int item=units[col]->items[row];if(col==tradeColumn&&row==tradeRow)MenuText(rgba,">",10+col*112,50+row*16,1);if(col==tradeChosenColumn&&row==tradeChosenRow)MenuText(rgba,">",10+col*112,50+row*16,0);const char *name=tacticalItemNames[item&255];MenuText(rgba,name?name:"",20+col*112,50+row*16,col==tradeColumn&&row==tradeRow);}}}
