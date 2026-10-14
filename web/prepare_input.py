from pathlib import Path
import re

root = Path(__file__).resolve().parent.parent
source = (root / 'src/hardware.c').read_text()
header = (root / 'include/hardware.h').read_text()

def function(name):
    match = re.search(r'void ' + re.escape(name) + r'\([^\n]*\)\s*\{', source)
    if not match:
        raise RuntimeError('Routine not found: ' + name)
    start = match.start()
    pos = match.end()
    depth = 1
    while depth:
        if source[pos] == '{': depth += 1
        if source[pos] == '}': depth -= 1
        pos += 1
    return source[start:pos]

struct = re.search(r'struct KeyStatusBuffer\s*\{.*?\};', header, re.S)
if not struct: raise RuntimeError('KeyStatusBuffer not found')
text = '''// Generated from the original sources. Do not edit.
#include "compat.h"
typedef u16 bool16;
enum { A_BUTTON=1, B_BUTTON=2, SELECT_BUTTON=4, START_BUTTON=8,
DPAD_RIGHT=16, DPAD_LEFT=32, DPAD_UP=64, DPAD_DOWN=128,
R_BUTTON=256, L_BUTTON=512, DPAD_ANY=240 };
static u16 gKeyStatusIgnoredSt;
'''
text += struct.group() + '\n' + function('_UpdateKeyStatus') + '\n' + function('ResetKeyStatus')
text += '''
static struct KeyStatusBuffer state;
void fe8_input_reset(void) { state = (struct KeyStatusBuffer){0}; ResetKeyStatus(&state); }
void fe8_input_frame(int keys) { _UpdateKeyStatus(&state, keys & 1023); }
struct KeyStatusBuffer *fe8_input_status(void) { return &state; }
int fe8_input_held(void) { return state.heldKeys; }
int fe8_input_new(void) { return state.newKeys; }
int fe8_input_repeat(void) { return state.repeatedKeys; }
'''
out = root / 'web/generated'
out.mkdir(exist_ok=True)
(out / 'input.c').write_text(text)
print('Input: original routines extracted from hardware.c')
