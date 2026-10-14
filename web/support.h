#ifndef FE8_SUPPORT_H
#define FE8_SUPPORT_H
#include "units.h"
#include "generated/support_types.h"
int GetUnitSupporterCount(struct Unit *unit);
int GetUnitSupporterInitialExp(struct Unit *unit,int num);
int GetUnitSupportBonuses(struct Unit *unit,struct SupportBonuses *bonuses);
void ProcessTurnSupportExp(void);
void fe8_support_turn(void);
int fe8_support_info(int index,int field);
#endif
