// Exercise the actual generated MC decoder against configured storage bounds.
#include "360_struc.h"
#include <cstdio>

extern "C" {
DATA360 oldstate = {}, newstate = {};
}

int main() {
    unsigned failures = 0, cases = 0;
    for (unsigned bits = 17; bits <= 23; ++bits) {
        const unsigned limit = (1u << bits) - 1;
        const unsigned addresses[] = {
            0, limit & ~7u, limit + 1, limit + 8,
            0x800000, 0x800008, 0xffffff
        };
        for (unsigned address : addresses) {
            memset(&oldstate, 0, sizeof(oldstate));
            memset(&newstate, 0, sizeof(newstate));
            // Hold both inputs fixed while the generated next-state terms settle.
            for (unsigned pass = 0; pass < 3; ++pass) {
                oldstate.MA.sab.F = address;
                oldstate.EXTERNAL_.reg_se_size.F = limit;
                process_MC();
                oldstate = newstate;
            }
            const bool expected = address <= limit;
            const bool selected = newstate.MC_INT.se_1_decoded != 0;
            ++cases;
            if (selected != expected) {
                ++failures;
                std::printf("FAIL limit=%06x address=%06x selected=%d expected=%d\n",
                            limit, address, selected, expected);
            }
        }
    }
    std::printf("Storage decode: %u cases, %u failures\n", cases, failures);
    return failures ? 1 : 0;
}
