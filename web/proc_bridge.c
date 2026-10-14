#include "compat.h"
#include "proc.h"

static int calls, ended, childDone, parentDone, ticks;
static void Call(ProcPtr p) { (void)p; calls++; }
static void End(ProcPtr p) { (void)p; ended++; }
static void ChildDone(ProcPtr p) { (void)p; childDone++; }
static void ParentDone(ProcPtr p) { (void)p; parentDone++; }
static void Tick(ProcPtr p) { (void)p; ticks++; }
static const struct ProcCmd script[] = {
    PROC_SET_END_CB(End), PROC_CALL(Call), PROC_SLEEP(2), PROC_CALL(Call), PROC_END
};
static const struct ProcCmd child[] = { PROC_SLEEP(2), PROC_CALL(ChildDone), PROC_END };
static const struct ProcCmd parent[] = {
    PROC_START_CHILD_BLOCKING(child), PROC_CALL(ParentDone), PROC_END
};
static const struct ProcCmd ticker[] = { PROC_REPEAT(Tick), PROC_END };
static void Frame(void) {
    int i;
    for (i = 0; i < 8; ++i) Proc_Run(gProcTreeRootArray[i]);
}
int fe8_proc_selftest(void) {
    int i;
    Proc_Init(); calls = ended = childDone = parentDone = 0;
    Proc_Start(script, PROC_TREE_3);
    if (calls != 1 || CountProcs(NULL) != 1) return 1;
    Frame(); Frame();
    if (calls != 1) return 2;
    Frame();
    if (calls != 2 || ended != 1 || CountProcs(NULL) != 0) return 3;
    Proc_Start(parent, PROC_TREE_3);
    if (CountProcs(NULL) != 2 || parentDone != 0) return 4;
    Frame(); Frame(); Frame();
    if (childDone != 1 || parentDone != 0) return 5;
    Frame();
    if (parentDone != 1 || CountProcs(NULL) != 0) return 6;
    // Exercise pool reuse without exceeding the original 64-process limit.
    for (i = 0; i < 100; ++i) {
        ProcPtr p = Proc_Start(ticker, PROC_TREE_3);
        if (Proc_Find(ticker) != p) return 7;
        Proc_End(p);
    }
    if (CountProcs(NULL) != 0) return 8;
    return 0;
}
void fe8_proc_demo_start(void) {
    Proc_Init(); ticks = 0;
    Proc_Start(ticker, PROC_TREE_3);
}
int fe8_proc_demo_frame(void) { Frame(); return ticks; }
