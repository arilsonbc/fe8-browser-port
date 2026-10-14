#include "support.h"
#include "save.h"
#include <stdlib.h>
#define gPlaySt (*fe8_save_loaded_playst())
#define RECT_DISTANCE(x1,y1,x2,y2) (abs((x1)-(x2))+abs((y1)-(y2)))
extern struct Unit *GetUnit(int index);
static struct {int gameStateBits;} gBmSt; // Arena/preview mode is not entered by the current chapter runtime.
#include "generated/support_assets.h"
#include "generated/support_routines.h"
void fe8_support_turn(void){ProcessTurnSupportExp();}
int fe8_support_info(int index,int field){if(index<1||index>255||!UNIT_IS_VALID(GetUnit(index)))return -1;struct Unit *u=GetUnit(index);struct SupportBonuses b;GetUnitSupportBonuses(u,&b);switch(field){case 0:return GetUnitSupporterCount(u);case 1:return GetUnitTotalSupportLevel(u);case 2:return b.bonusAttack;case 3:return b.bonusDefense;case 4:return b.bonusHit;case 5:return b.bonusAvoid;case 6:return b.bonusCrit;case 7:return b.bonusDodge;default:if(field>=10&&field<17)return GetUnitSupporterCharacter(u,field-10);return -1;}}
