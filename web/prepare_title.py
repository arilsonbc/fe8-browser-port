from pathlib import Path
import re
root = Path(__file__).resolve().parent.parent
source = (root / 'src/titlescreen.c').read_text()
match = re.search(r'void Title_IDLE\([^\n]*\)\s*\{', source)
if not match: raise RuntimeError('Title_IDLE missing')
pos = match.end(); depth = 1
while depth:
    if source[pos] == '{': depth += 1
    if source[pos] == '}': depth -= 1
    pos += 1
routine = source[match.start():pos]
def extract(name):
    match = re.search(r'void ' + name + r'\([^\n]*\)\s*\{', source)
    if not match: raise RuntimeError(name + ' missing')
    pos = match.end(); depth = 1
    while depth:
        if source[pos] == '{': depth += 1
        if source[pos] == '}': depth -= 1
        pos += 1
    return source[match.start():pos]
palette = (root / 'graphics/titlescreen/title_unk_palette_1.gbapal').read_bytes()
if len(palette) != 32: raise RuntimeError('Unexpected pulse palette size')
colors = [int.from_bytes(palette[i:i+2], 'little') for i in range(0,32,2)]
header = (root / 'include/opanim.h').read_text()
struct = re.search(r'struct TitleScreenProc\s*\{.*?\};', header, re.S)
if not struct: raise RuntimeError('TitleScreenProc missing')
text = '''// Generated: original Title_IDLE, with explicit platform boundaries.
#include "../compat.h"
#include "proc.h"
#include "../palette.h"
#include "../audio.h"
extern void fe8_game_set_action(int id);
extern void fe8_game_detach(void);
#include "input.c"
enum { GAME_ACTION_EVENT_RETURN=0, GAME_ACTION_CLASS_REEL=1, SONG_6D=0x6D };
static struct KeyStatusBuffer *gKeyStatusPtr=&state;
static int action=-1, soundRequest=-1;
static void PlaySoundEffect(int song) { soundRequest= song;fe8_audio_se(song); }
static void SetNextGameActionId(int next) { action=next;fe8_game_set_action(next); }
'''
text += 'static const u16 gPal_08AADBE8[]={' + ','.join(map(str,colors)) + '};\n'
text += '''static int pulseColor;
static int DivRem(int a,int b) { return a%b; }
// Rendering is composed by graphics.c; this boundary only captures palette writes.
static void CopyToPaletteBuffer(const u16 *src,int offset,int size) {
    if(offset==0x210 && size==2) pulseColor=*src;gPaletteBuffer[offset/2]=*src;
}
static const u16 gSprite_Title_FireEmblemLogo[]={0},gObject_16x16[]={0},
 gSprite_Title_SacredStonesBanner[]={0},gSprite_Title_PressStart[]={0},
 gSprite_Title_CopyrightInfo[]={0};
static void PutSpriteExt(int layer,int x,int y,const u16 *sprite,int tile) {
    (void)layer;(void)x;(void)y;(void)sprite;(void)tile;
}
'''
text += struct.group() + '\n' + extract('DrawTitleSprites_Init') + '\n' + extract('DrawTitleSprites_Loop') + '\n' + routine + '''
static void Draw(ProcPtr p) { DrawTitleSprites_Loop((struct TitleScreenProc*)p); }
static const struct ProcCmd drawScript[]={PROC_REPEAT(Draw),PROC_END};
static struct TitleScreenProc *drawProc;
static int phase=1,idleFrames;
static void Idle(ProcPtr p) { phase=1;Title_IDLE((struct TitleScreenProc*)p);idleFrames=((struct TitleScreenProc*)p)->timer_idle; }
static const struct ProcCmd titleScript[]={PROC_REPEAT(Idle),PROC_END};
static struct TitleScreenProc *titleProc;
void fe8_title_reset(void) {
    fe8_game_detach();Proc_Init();fe8_input_reset();action=-1;soundRequest=-1;phase=1;idleFrames=0;
    titleProc=Proc_Start(titleScript,PROC_TREE_3);
    titleProc->timer=0;titleProc->timer_idle=0;
    drawProc=Proc_Start(drawScript,titleProc);
    DrawTitleSprites_Init(drawProc);pulseColor=gPal_08AADBE8[15];
}
int fe8_title_tick(int keys) {
    fe8_input_frame(keys);
    if(action!=-1)return action;
    Proc_Run(gProcTreeRootArray[3]);
    return action;
}
extern void FadeInBlackSpeed20(ProcPtr p);
extern void sub_8013F40(ProcPtr p);
static void BeginFadeOut(ProcPtr p) { phase=2;sub_8013F40(p); }
static void EndChildren(ProcPtr p) {
    struct Proc *proc=p;
    while(proc->proc_child)Proc_End(proc->proc_child);
}
// Original mode-0 title path after graphics setup, with original locking fades.
static const struct ProcCmd fullTitleScript[]={
    PROC_CALL(FadeInBlackSpeed20),PROC_YIELD,
    PROC_REPEAT(Idle),PROC_CALL(BeginFadeOut),PROC_YIELD,
    PROC_CALL(EndChildren),PROC_END
};
void fe8_title_start(ProcPtr parent) {
    action=-1;soundRequest=-1;phase=0;idleFrames=0;
    titleProc=Proc_StartBlocking(fullTitleScript,parent);
    titleProc->timer=0;titleProc->timer_idle=0;
    drawProc=Proc_Start(drawScript,titleProc);
    DrawTitleSprites_Init(drawProc);pulseColor=gPal_08AADBE8[15];
}
int fe8_title_phase(void) { return phase; }
int fe8_title_pulse_color(void) { return pulseColor; }
int fe8_title_sound_request(void) { return soundRequest; }
int fe8_title_idle_frames(void) { return idleFrames; }
'''
(root / 'web/generated/title_logic.c').write_text(text)
print('Title: original idle routine and process structure extracted')
