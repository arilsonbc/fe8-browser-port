from pathlib import Path
import re
root=Path(__file__).resolve().parent.parent
# Read every web artifact before modifications, including generated/binary outputs.
for p in sorted((root/'web').rglob('*')):
    if p.is_file(): p.read_bytes()
def function(file,name):
    s=(root/file).read_text()
    m=re.search(r'(?:void|bool|struct GameCtrlProc \*)\s*'+re.escape(name)+r'\([^\n]*\)\s*\{',s)
    if not m: raise RuntimeError(name+' missing')
    pos=m.end();depth=1
    while depth:
        if s[pos]=='{':depth+=1
        if s[pos]=='}':depth-=1
        pos+=1
    return s[m.start():pos]
def struct(file,name):
    s=(root/file).read_text();m=re.search(r'struct '+name+r'\s*\{.*?\};',s,re.S)
    if not m:raise RuntimeError(name+' missing')
    return m.group()
hardware='src/hardware.c';bmlib='src/bmlib.c'
text='''// Generated original color setup and FadeCore routines. Do not edit.
#include "../palette.h"
#include "proc.h"
static int sModifiedPalette;
'''
for name in ['ColorFadeSetupFromColorToBlack','ColorFadeSetupFromBlack','ColorFadeSetupFromColorToWhite','ColorFadeSetupFromWhite','sub_8001A6C']:
    text+=function(hardware,name)+'\n'
text+='void CALLARM_ColorFadeTick(void) { sub_8001A6C(); }\n'
text+=struct('include/bmlib.h','FadeCoreProc')+'\n'
text+='''void FadeCore_Init(struct FadeCoreProc *proc);
void FadeCore_Loop(struct FadeCoreProc *proc);
bool FadeCore_Tick(struct FadeCoreProc *proc);
'''
s=(root/bmlib).read_text();start=s.index('struct FadeKindEnt {');end=s.index('void FadeCoreEndEach',start)
text+='static void FadeCore_CallTick(ProcPtr p) { (void)FadeCore_Tick(p); }\n'
text+=s[start:end].replace('PROC_CALL(FadeCore_Tick)', 'PROC_CALL(FadeCore_CallTick)').replace('void (* setup_color_fade)(s8 component_step);','void (* setup_color_fade)(u8 component_step);').replace('setup_color_fade = (void *) gUnknown_080D7964[kind].setup_color_fade;','setup_color_fade = gUnknown_080D7964[kind].setup_color_fade;').replace("// need to cast because parameter types don't match (int vs. i8)", '// WASM boundary: use the actual u8 setup signature, avoiding incompatible function casts.')
for name in ['FadeCore_Init','FadeCore_Loop','FadeCore_Tick','sub_80141B0','FadeInBlackSpeed20','sub_8013F40']:
    if name in ['FadeInBlackSpeed20','sub_8013F40']:continue
    text+=function(bmlib,name)+'\n'
text+='void sub_80141B0(void);\n'+function(bmlib,'FadeInBlackSpeed20')+'\n'+function(bmlib,'sub_8013F40')+'\n'
text+='''int fe8_fade_active(void) { return Proc_Find(ProcScr_FadeCore)!=NULL; }
void fe8_fade_test_start(int kind,int speed) {
    Proc_Init();fe8_palette_reset();
    for(int i=0;i<512;i++)gPaletteBuffer[i]=(i*67)&0x7fff;
    // Locking fades require a real process parent, never a root-tree sentinel.
    static const struct ProcCmd parentScript[]={PROC_BLOCK,PROC_END};
    ProcPtr parent=(kind==2||kind==3||kind==6||kind==7)?Proc_Start(parentScript,PROC_TREE_3):PROC_TREE_3;
    StartFadeCore(kind,speed,parent,NULL);
}
int fe8_fade_test_tick(void) { Proc_Run(gProcTreeRootArray[3]);return fe8_fade_active(); }
'''
(root/'web/generated/fade.c').write_text(text)
s=(root/'include/gamecontrol.h').read_text();enum=re.search(r'enum game_ctrl_proc_lable\s*\{.*?\};',s,re.S).group()
actions=re.search(r'enum[^\n]*\s*\{[^}]*GAME_ACTION_EVENT_RETURN[^}]*\};',(root/'include/types.h').read_text(),re.S).group()
text='''// Generated controller decision and selected original process commands.
#include "../palette.h"
#include "../map.h"
#include "../events.h"
#include "../tactical.h"
extern void fe8_units_reset(void);
#include "../save.h"
#include "../audio.h"
#include "constants/songs.h"
#include "proc.h"
'''+enum+'\n'+actions+'\n'+struct('include/gamecontrol.h','GameCtrlProc')+'\n'
text+='''extern void fe8_title_start(ProcPtr parent);
extern void fe8_menu_start(ProcPtr parent);
extern void fe8_menu_detach(void);
extern int fe8_menu_state(void);
extern void fe8_graphics_load_title_palettes(void);
extern void fe8_input_frame(int keys);
extern void fe8_input_reset(void);
extern int fe8_title_phase(void);
extern int fe8_input_new(void);
extern int fe8_input_repeat(void);
static int boundary=-1,volumeRequest=-1;
static struct GameCtrlProc *control;
static void StartBgmVolumeChange(int from,int to,int duration,int unused) {
    (void)unused;volumeRequest=to;fe8_audio_volume(from,to,duration);
}
'''
text+='static void GmDataInit(void){}\n'
text+=function('src/gamecontrol.c','GameControl_SwitchPostSaveMenu')+'\n'
text+=function('src/gamecontrol.c','sub_8009AD8')+'\n'+function('src/gamecontrol.c','GameControl_PostIntro')+'\n'
text+='''static void GameControl_EnableSoundEffects(ProcPtr p) { (void)p; }
static void StartTitleScreen_WithMusic(ProcPtr p) { fe8_palette_reset();fe8_graphics_load_title_palettes();fe8_title_start(p);fe8_audio_bgm(SONG_MAIN_THEME,0); }
static void StartTitleScreen_FlagFalse(ProcPtr p) { fe8_title_start(p); }
// Full opening is still unavailable; flag-true must not claim to play it.
static void StartTitleScreen_FlagTrue(ProcPtr p) { (void)p;boundary=5; }
static void StartSaveMenu(ProcPtr p) { fe8_menu_start(p); }
static void ReturnTitle(ProcPtr p) { Proc_Goto(p,LGAMECTRL_TITLE_DIRECT); }
static void ClassBoundary(ProcPtr p) { (void)p;boundary=4; }
static void IntroBoundary(ProcPtr p) { (void)p;boundary=5; }
static void ChapterBoundary(ProcPtr p){(void)p;boundary=11;fe8_chapter_start();}
static const struct ProcCmd gProcScr_GameControl[]={
'''
s=(root/'src/gamecontrol.c').read_text();start=s.index('PROC_LABEL(LGAMECTRL_TITLE_DIRECT)');end=s.index('    PROC_CALL(StartSaveMenu),',start)+len('    PROC_CALL(StartSaveMenu),')
text+=s[start:end]+'''
    PROC_YIELD,PROC_CALL(GameControl_SwitchPostSaveMenu),PROC_BLOCK,
PROC_LABEL(LGAMECTRL_EXEC_BM),PROC_CALL(ChapterBoundary),PROC_BLOCK,
PROC_LABEL(8),PROC_CALL(ChapterBoundary),PROC_BLOCK,
PROC_LABEL(12),PROC_CALL(IntroBoundary),PROC_BLOCK,
PROC_LABEL(14),PROC_CALL(ChapterBoundary),PROC_BLOCK,
PROC_LABEL(15),PROC_CALL(ChapterBoundary),PROC_BLOCK,
PROC_LABEL(LGAMECTRL_CLASS_REEL),PROC_CALL(ClassBoundary),PROC_BLOCK,
PROC_LABEL(LGAMECTRL_OP_ANIM),PROC_CALL(IntroBoundary),PROC_BLOCK,
PROC_END
};
'''
text+=function('src/gamecontrol.c','GetGameControl')+'\n'+function('src/gamecontrol.c','SetNextGameActionId')+'\n'
text+='''void fe8_game_set_action(int id) { if(control)SetNextGameActionId(id); }
void fe8_game_detach(void) { control=NULL;fe8_menu_detach(); }
void fe8_game_reset(void) {
    control=NULL;fe8_progress_reset();fe8_audio_reset();fe8_event_reset();fe8_units_reset();fe8_map_stop();fe8_menu_detach();Proc_Init();fe8_input_reset();fe8_palette_reset();
    fe8_graphics_load_title_palettes();boundary=-1;volumeRequest=-1;
    control=Proc_Start(gProcScr_GameControl,PROC_TREE_3);
    control->nextAction=GAME_ACTION_EVENT_RETURN;control->idle_status=0;
    // Fade setup captured the destination; suppress the pre-first-tick flash.
    for(int i=0;i<512;i++)gPaletteBuffer[i]=0;
}
int fe8_game_tick(int keys) {
    fe8_input_frame(keys);
    if(boundary==11&&!fe8_event_busy()&&!fe8_tactical_consumes_back()&&(fe8_input_new()&2)){fe8_game_reset();return 0;}
    if(boundary==11&&fe8_map_active()){Proc_Run(gProcTreeRootArray[4]);fe8_chapter_tick(fe8_input_new());fe8_map_tick(fe8_input_repeat());}
    if(boundary<0){Proc_Run(gProcTreeRootArray[3]);Proc_Run(gProcTreeRootArray[4]);}
    return boundary>=0?boundary:fe8_menu_state()>=0?fe8_menu_state():fe8_title_phase();
}
int fe8_game_state(void) { return boundary>=0?boundary:fe8_menu_state()>=0?fe8_menu_state():fe8_title_phase(); }
int fe8_game_volume_request(void) { return volumeRequest; }
int fe8_game_proc_count(void) { return CountProcs(NULL); }
'''
(root/'web/generated/gameflow.c').write_text(text)
print('Runtime: original palette fades and controller decision extracted')


