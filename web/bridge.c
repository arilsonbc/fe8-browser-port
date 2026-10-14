#include "compat.h"
#include "rng.h"

int fe8_rng_selftest(void)
{
    u16 original[3], restored[3];
    int i, next;
    InitRN(0);
    StoreRNState(original);
    if (original[0] != 0xA36E || original[1] != 0x924E || original[2] != 0xB784)
        return 1;
    next = NextRN();
    LoadRNState(original);
    if (NextRN() != next) return 2;
    StoreRNState(restored);
    if (restored[1] != original[0] || restored[2] != original[1]) return 3;
    for (i = 0; i < 10000; ++i) {
        next = NextRN_100();
        if (next < 0 || next > 99) return 4;
    }
    return 0;
}
