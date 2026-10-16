// Shop Proc/dialogue/video presentation is adapted; pricing and mutations below
// come from the original routines extracted by prepare_shop.py.
extern int GetItemIndex(int item);
extern int GetItemCost(int item);
extern u32 GetPartyGoldAmount(void);
extern void SetPartyGoldAmount(u32 amount);
#define UNIT_ACTION_SHOPPED 0x17
#define BM_FLAG_PREPSCREEN (1<<4)
#define TILE_COMMAND_ARMORY 0x16
#define TILE_COMMAND_VENDOR 0x17
#define PL_SHOP_SELL_NOITEM 0
#define PL_SHOP_SELL 1
struct ProcShop {struct Unit *unit;u16 shopItems[20];int shopItemCount,unitItemCount,head_loc;};
static struct ProcShop nativeShop;
static int shopType,shopColumn,shopRow,shopConfirm,shopChanged,shopError;
static int GetTalkChoiceResult(void){return 1;}
#define TALK_CHOICE_YES 1
static void NativeShopGoto(struct ProcShop *p,int label){(void)p;(void)label;}
static void PlaySeDelayed(int song,int delay){(void)delay;fe8_audio_se(song);}
#include "generated/shop_routines.h"
#include "generated/shop_text.h"
int fe8_tactical_can_shop(void){if(mode!=3||!gActiveUnit||fe8_event_busy()||gPlaySt.chapterIndex!=6)return 0;return ArmoryCommandUsability(NULL,0)==MENU_ENABLED||VendorCommandUsability(NULL,0)==MENU_ENABLED;}
int fe8_tactical_begin_shop(void){
 if(!fe8_tactical_can_shop())return -1;
 memset(&nativeShop,0,sizeof(nativeShop));nativeShop.unit=gActiveUnit;
 for(int row=0;row<19;row++){int item=fe8_event_shop_item(gActiveUnit->xPos,gActiveUnit->yPos,row);if(item<=0)break;nativeShop.shopItems[row]=item;}
 UpdateShopItemCounts(&nativeShop);if(!nativeShop.shopItemCount)return -2;
 shopType=fe8_event_tile_command(gActiveUnit->xPos,gActiveUnit->yPos)==TILE_COMMAND_VENDOR;shopColumn=shopRow=shopChanged=shopError=0;shopConfirm=1;mode=25;return 0;
}
int fe8_tactical_shop_item(int column,int row){if(mode<25||mode>27||!gActiveUnit||column<0||column>1||row<0||row>=(column?GetUnitItemCount(gActiveUnit):nativeShop.shopItemCount))return 0;return column?gActiveUnit->items[row]:nativeShop.shopItems[row];}
int fe8_tactical_shop_price(int column,int row){int item=fe8_tactical_shop_item(column,row);return !item?-1:column?GetItemSellPrice(item):GetItemPurchasePrice(gActiveUnit,item);}
int fe8_tactical_shop_info(int field){if(mode<25||mode>27||!gActiveUnit)return -1;int item=fe8_tactical_shop_item(shopColumn,shopRow);return field==0?shopType:field==1?shopColumn:field==2?shopRow:field==3?(shopColumn?GetUnitItemCount(gActiveUnit):nativeShop.shopItemCount):field==4?fe8_tactical_shop_price(shopColumn,shopRow):field==5?item:field==6?(item&&(shopColumn?IsItemSellable(item):GetItemPurchasePrice(gActiveUnit,item)<=GetPartyGoldAmount())):field==7?shopChanged:field==8?shopError:field==9?shopConfirm:-1;}
int fe8_tactical_shop_select(int column,int row){if(mode!=25||!fe8_tactical_shop_item(column,row))return -1;shopColumn=column;shopRow=row;shopConfirm=1;shopError=0;mode=26;return 0;}
int fe8_tactical_shop_confirm(int yes){
 if((mode!=26&&mode!=27)||!gActiveUnit)return -1;
 shopError=0;if(!yes){mode=25;return 0;}
 int item=fe8_tactical_shop_item(shopColumn,shopRow);if(!item)return shopError=-2;
 nativeShop.head_loc=shopRow;UpdateShopItemCounts(&nativeShop);
 if(shopColumn){if(!IsItemSellable(item))return shopError=-3;Shop_HandleSellConfirmPrompt(&nativeShop);}
 else {
  if(GetItemPurchasePrice(gActiveUnit,item)>GetPartyGoldAmount())return shopError=-4;
  if(nativeShop.unitItemCount>=5){if(!HasConvoyAccess()||fe8_convoy_count()>=100)return shopError=-5;if(mode!=27){mode=27;shopConfirm=1;return 1;}if(fe8_convoy_add(item)<0)return shopError=-6;}
  else if(!UnitAddItem(gActiveUnit,item))return shopError=-7;
  HandleShopBuyAction(&nativeShop);
 }
 shopChanged=1;mode=25;int count=shopColumn?GetUnitItemCount(gActiveUnit):nativeShop.shopItemCount;if(shopRow>=count)shopRow=count?count-1:0;FindTargets();return 0;
}
static int CloseNativeShop(void){mode=3;FindTargets();return shopChanged?FinishAction(UNIT_ACTION_SHOPPED):0;}
static void NativeShopTick(int keys){
 if(keys&2){if(mode!=25){mode=25;shopError=0;}else CloseNativeShop();return;}
 if(mode==26||mode==27){if(keys&240)shopConfirm^=1;if(keys&1)fe8_tactical_shop_confirm(!shopConfirm);return;}
 if(keys&48){shopColumn^=1;shopRow=0;}
 int count=shopColumn?GetUnitItemCount(gActiveUnit):nativeShop.shopItemCount;
 if(count&&(keys&192))shopRow=(shopRow+((keys&128)?1:count-1))%count;
 if(keys&1){if(!count||fe8_tactical_shop_select(shopColumn,shopRow)<0)fe8_audio_se(0x6b);}
}
