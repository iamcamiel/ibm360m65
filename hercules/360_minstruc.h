// * IBM 360 Model 65 Emulator
// * Copyright (C) 2024 Camiel Vanderhoeven
// *
// * This program is free software: you can redistribute it and/or modify
// * it under the terms of the GNU General Public License as published by
// * the Free Software Foundation, either version 3 of the License, or
// * (at your option) any later version.
// *
// * This program is distributed in the hope that it will be useful,
// * but WITHOUT ANY WARRANTY; without even the implied warranty of
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// * GNU General Public License for more details.
// *
// * You should have received a copy of the GNU General Public License
// * along with this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma warning (disable : 4807)

// Native-model startup at 100 MHz. Keep POR beyond the 3.2 us console
// single-shot, then let the 256-word local-store reset loop finish before IPL.
#define M65_NATIVE_POWER_RESET_CYCLES 500
#define M65_NATIVE_STARTUP_CYCLES 10000

#include "ald.h"
#include <stdio.h>

typedef struct _DATA360 {
  _ALD_INT ald_int;

    BIT_0_31 ls_mem[32];
    BIT_0_3 ls_par[32];

} DATA360;

extern DATA360 oldstate;
extern DATA360 newstate;

extern FILE* lf;
extern bool logging;
extern bool mon_cycle;
extern bool mon_ros;
extern bool mon_20;
extern bool mon_stor;

#if defined(STANDALONE)
#define D_fprintf(a, ...) { fprintf(a, __VA_ARGS__); printf(__VA_ARGS__); }
#else
extern void logmsg(char* a, ...);
#define D_fprintf(a, b, ...)  if (logging) { fprintf(a, b, __VA_ARGS__); logmsg(b,__VA_ARGS__); }
#endif

void full_init();
void single_cycle();
bool m65_fault_pending();
void m65_latch_fault(const char* reason);

#if defined(COMPARE_M65)
void record_65_write(int sea, int wh, int wl);
void record_65_write_char(int addr, char data);
void record_65_set_key(int sec, int sea);
void record_herc_write(int addr, int len, char* data);
void record_herc_write_char(int addr, char data);
// Record a field stored directly into the CPU's absolute main-storage array.
#define RECORD_HERC_ABSOLUTE_FIELD(regs, field) \
    record_herc_write((int)((char*)(field) - (char*)(regs)->mainstor), \
                      sizeof(field), (char*)(field))
void record_herc_set_key(int addr, char data);
void record_65_io(int);
void record_herc_io(int);
void write_compare();
void io_compare();
#endif
