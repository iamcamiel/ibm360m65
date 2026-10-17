#include "360_struc.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

DATA360 oldstate{}, newstate{};
FILE* lf = stdout;
bool logging = false, mon_cycle = false, mon_ros = false, mon_20 = false, mon_stor = false;

static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
static unsigned odd(unsigned long long value) {
    unsigned parity = 1;
    while (value) { parity ^= unsigned(value & 1); value >>= 1; }
    return parity;
}

int main(int argc, char** argv) {
    const unsigned flags = argc > 1 ? unsigned(std::atoi(argv[1])) : 0;
    const unsigned latency = argc > 2 ? unsigned(std::atoi(argv[2])) : 50;
    const bool held = argc > 3 && std::atoi(argv[3]);
    const bool halt_reply = argc > 4 && std::atoi(argv[4]);
    try {
        init_WA();
        newstate.MC._select_se_1_even = 1;
        unsigned seq = 0, expected_command = 0, expected_address = 0;
        unsigned accepts = 0, advances = 0, accept_ticks = 0, advance_ticks = 0, keys = 0;
        unsigned old_accept = 0, old_advance = 0;
        unsigned long long expected_write = 0, expected_read = 0;
        bool published = false, replied = false, halted = false;
        const auto tick = [&]() {
            if (halted) return; // Fixture halt freezes both evaluations and SPECIAL calls.
            oldstate = newstate;
            process_WA(); process_WA_clock();
            oldstate = newstate;
            process_WA();
            const unsigned accept = !newstate.WA._accept_se_1;
            const unsigned advance = newstate.WA.advance_sdbo;
            accepts += accept && !old_accept; accept_ticks += accept;
            advances += advance && !old_advance; advance_ticks += advance;
            old_accept = accept; old_advance = advance;
            if (newstate.WA.key_advance) {
                require(replied && (flags & 4), "premature/unrequested key advance");
                require(newstate.WA.out_key.F == 26 && newstate.WA.out_key_parity == odd(26), "wrong storage key/parity");
                ++keys;
            }
            if (published) {
                require(unsigned(newstate.WA.reg_se_cmd.F) == expected_command && unsigned(newstate.WA.reg_se_addr.F) == expected_address,
                    "request or payload changed after publication");
                if (flags & 1) require((static_cast<unsigned long long>(unsigned(newstate.WA.reg_se_wdata_hi.F)) << 32 | unsigned(newstate.WA.reg_se_wdata_lo.F)) == expected_write,
                    "store data not held");
            }
        };
        const auto ticks = [&](unsigned n) { for (unsigned i = 0; i < n; ++i) tick(); };
        ticks(100);
        require(!accept_ticks && !advance_ticks && unsigned(newstate.WA.reg_se_cmd.F) >> 30 == 0, "phantom transaction at startup");
        for (unsigned transaction = 0; transaction < 12; ++transaction) {
            published = false; replied = false;
            accepts = advances = accept_ticks = advance_ticks = keys = old_accept = old_advance = 0;
            newstate.MC.store_to_se = flags & 1;
            newstate.MC.set_key = bool(flags & 2);
            newstate.MC.insert_key = bool(flags & 4);
            newstate.MC.cancel = bool(flags & 8);
            newstate.MC.test_and_set = bool(flags & 16);
            newstate.MA.sab.F = 0x123458 + transaction * 8;
            newstate.MB.mark.F = (0xa5 + transaction) & 255;
            newstate.MB.in_key.F = 21;
            expected_write = 0x0123456789abcdefULL + transaction;
            expected_read = 0xfedcba9876543210ULL + transaction;
            newstate.MB.sdbi.F = expected_write;
            ticks(3);
            newstate.MC._select_se_1_even = 0;
            expected_address = ((0xa5 + transaction) & 255) << 24 | (0x123458 + transaction * 8);
            expected_command = ((seq + 1) & 3) << 30 | 21U << 25 | flags;
            for (unsigned watchdog = 0; watchdog < 100; ++watchdog) {
                tick();
                if (unsigned(newstate.WA.reg_se_cmd.F) >> 30 != seq) break;
            }
            seq = (seq + 1) & 3;
            require(unsigned(newstate.WA.reg_se_cmd.F) == expected_command && unsigned(newstate.WA.reg_se_addr.F) == expected_address,
                "missing/repeated sequence or incorrect request payload");
            published = true;
            if (!held) newstate.MC._select_se_1_even = 1;
            newstate.MA.sab.F = 0xffffff;
            newstate.MB.mark.F = 0;
            newstate.MB.in_key.F = 0;
            newstate.MB.sdbi.F = ~0ULL;
            newstate.EXTERNAL_.reg_se_resp.F = ((seq - 1) & 3) << 30 | 0x35000000;
            ticks(latency);
            if (!(flags & 1)) require(!accepts && !advances, "read accepted before matching response");
            if (halt_reply) { halted = true; ticks(3); }
            newstate.EXTERNAL_.reg_se_resp.F = seq << 30 | 0x35000000;
            newstate.EXTERNAL_.reg_se_rdata_hi.F = unsigned(expected_read >> 32);
            newstate.EXTERNAL_.reg_se_rdata_lo.F = unsigned(expected_read);
            if (halt_reply) { ticks(7); halted = false; }
            replied = true;
            ticks(150);
            require(accepts == 1 && advances == 1 && accept_ticks == 20 && advance_ticks == 20,
                "completion count/width differs");
            if (!(flags & 1)) {
                require(newstate.WA.sdbo.F == expected_read, "wrong read data");
                unsigned parity = 0;
                for (unsigned byte = 0; byte < 8; ++byte) parity |= odd((expected_read >> (8 * byte)) & 255) << byte;
                require(unsigned(newstate.WA.sdbo_parity.F) == parity, "wrong data parity");
            }
            if (flags & 4) require(keys > 0, "missing matched key advance");
            if (held) ticks(150);
            published = false;
            newstate.MC._select_se_1_even = 1;
            ticks(20);
        }
        std::printf("MEMORY_BRIDGE_NATIVE_PASS flags=%u latency=%u transactions=12\n", flags, latency);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "MEMORY_BRIDGE_NATIVE_FAILURE flags=%u: %s\n", flags, error.what());
        return 1;
    }
}
