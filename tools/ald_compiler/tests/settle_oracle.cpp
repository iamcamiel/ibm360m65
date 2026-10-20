#include "360_struc.h"
#include <cstdio>
_ALD oldstate{}, newstate{};

static _ALD step(_ALD state, int input, bool gate, bool rst, bool halt, bool second) {
    newstate = state;
    newstate.EXTERNAL_.input.F = input;
    newstate.EXTERNAL_.gate = gate;
    if (rst) init_ald();
    else if (!halt) {
        oldstate = newstate;
        process_ald();
        process_ald_clock();
        if (second) { oldstate = newstate; process_ald(); }
    }
    return newstate;
}

int main() {
    init_ald();
    _ALD good = newstate, bad = newstate;
    unsigned random = 0x1234abcd;
    int differences = 0;
    for (int i = 0; i < 1024; ++i) {
        random = random * 1664525u + 1013904223u;
        int input = (random >> 24) & 15;
        bool gate = (random >> 29) & 1;
        bool rst = i == 0 || i % 97 == 0;
        bool halt = i % 23 < 5;
        good = step(good, input, gate, rst, halt, true);
        bad = step(bad, input, gate, rst, halt, false);
        if (good.TESTA.result_a != bad.TESTA.result_a || good.TESTB.result_b != bad.TESTB.result_b)
            ++differences;
        std::printf("%d %d %d %d %d %d %d %d %d %d\n", rst, halt, input, gate,
            good.TESTA.result_a, good.TESTA._result_n, good.TESTB.result_b,
            good.TESTA.clock_result, good.TESTA.vector_up.F & 15, good.TESTA.vector_down.F & 15);
    }
    std::fprintf(stderr,"ONE_PASS_DIVERGENCES=%d\n", differences);
    return differences == 0;
}
