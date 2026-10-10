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

#include <cstring>

extern "C" {
#include "360_minstruc.h"

    inline char INT(char a) { return a; }
    inline char TD(char a) { return a; }
    inline char TD10NS(char a) { return a; }
    inline char RLY_PT(char a) { return a; }

    typedef struct __ROS {
        volatile bool valid;
        volatile int a;
        volatile int b;
        volatile int c;
        volatile int d;
        volatile int e;
        volatile int f;
        volatile int g;
        volatile int h;
        volatile int l;
        volatile int na;
        volatile int k;
        volatile int j;
        volatile int m;
        volatile int n;
        volatile int p;
        volatile int q;
        volatile int r;
        volatile int t;
        volatile int u;
        volatile int v;
        volatile int w;
        volatile bool bits[100];
    } _ROS;

    extern _ROS ros[4096];

    extern "C" int listing(int aa);

    inline void ROSMEM(BIT_0_11& old_a, BIT_0_99& new_b) {
        static int aaa = 0;
        new_b.F = new_b.F2 = 0;
        for (int i = 0; i < 64; i++) {
            new_b.F |= (ros[old_a.F].bits[99-i] ? 1LL : 0LL) << i;
        }
        for (int i = 64; i < 100; i++) {
            new_b.F2 |= (ros[old_a.F].bits[99-i] ? 1LL : 0LL) << (i - 64);
        }

        if (old_a.F != 0) {
            if (aaa != old_a.F)
                listing(old_a.F);
            aaa = old_a.F;
        }
    }

    inline char PHX(char old_in, char old_pulse, char old_out) {
        return old_pulse ? old_in : old_out;
    }

#define PH(oi,op,oo,no) no = PHX(oi,op,oo)

    inline void LS_MEMORY(BIT_0_4& read_address, BIT_0_4& write_address, char write_gate, BIT_0_31& write_data, BIT_0_3& write_parity ,BIT_0_31& read_data, BIT_0_3& read_parity) {
        if (write_gate) {
            newstate.ls_mem[write_address.F].F = write_data.F;
            newstate.ls_par[write_address.F].F = write_parity.F;
        }
        read_data.F = oldstate.ls_mem[read_address.F].F;
        read_parity.F = oldstate.ls_par[read_address.F].F;
    }

#define TDX(in, out, time) {\
  static char tdx[time-1]; \
  out = tdx[0]; \
  for (int i = 0;i<time-2;i++) tdx[i] = tdx[i+1]; \
  tdx[time-2] = in; \
}

#define NSSN(n, out, time) {\
  static long tm = 0; \
  static char o = 1; \
  if (tm) { \
    out=0; \
    tm--; \
    if ((time>1000000) && !tm) D_fprintf(lf, "%lld ns timer stopped\n",10LL*time); \
  } else { \
    if (o==1 && n==0) { \
      tm = time; \
      out = 0; \
      if (time>1000000) D_fprintf(lf, "%lld ns timer started\n",10LL*time); \
    } else { \
      out = 1; \
    } \
  } \
  o = n; \
}

#define SS(n, out, time) {\
  static long tm = 0; \
  static char o = 0; \
  if (tm) { \
    out=1; \
    tm--; \
    if ((time>1000000) && !tm) D_fprintf(lf, "%lld ns timer stopped\n",10LL*time); \
  } else { \
    if (o==0 && n==1) { \
      tm = time; \
      out = 1; \
      if (time>1000000) D_fprintf(lf, "%lld ns timer started\n",10LL*time); \
    } else { \
      out = 0; \
    } \
  } \
  o = n; \
}

#define SSN(n, out, time) {\
  static long tm = 0; \
  static char o = 0; \
  if (tm) { \
    out=0; \
    tm--; \
  } else { \
    if (o==0 && n==1) { \
      tm = time; \
      out = 0; \
    } else { \
      out = 1; \
    } \
  } \
  o = n; \
}

#define OSCX(i, o,time) { \
  static int tm = -10; \
  if (tm || !i) { \
    tm++; \
    o = (tm>=0 && tm<=time); \
    if (tm == time * 2) \
      tm = 0; \
  } else { \
    o = 1; \
  } \
}

#define TD20NS(oi,no) TDX(oi,no,2)
#define TD30NS(oi,no) TDX(oi,no,3)
#define TD35NS(oi,no) TDX(oi,no,4)
#define TD40NS(oi,no) TDX(oi,no,4)
#define TD50NS(oi,no) TDX(oi,no,5)
#define TD50_70NS(oi,no) TDX(oi,no,6)
#define TD55NS(oi,no) TDX(oi,no,6)
#define TD60NS(oi,no) TDX(oi,no,6)
#define TD65NS(oi,no) TDX(oi,no,7)
#define TD70NS(oi,no) TDX(oi,no,7)
#define TD80NS(oi,no) TDX(oi,no,8)
#define TD100NS(oi,no) TDX(oi,no,10)
#define TD110NS(oi,no) TDX(oi,no,11)
#define TD125NS(oi,no) TDX(oi,no,13)
#define TD200NS(oi,no) TDX(oi,no,19)
#define TD210NS(oi,no) TDX(oi,no,20)

#define DOUBLETD40NS(oi,no) TDX(oi,no,7)
#define DOUBLETD200NS(oi,no) TDX(oi,no,40)
#define DOUBLETD210NS(oi,no) TDX(oi,no,40)
#define DOUBLETD300NS(oi,no) TDX(oi,no,68)
#define DOUBLETD340NS(oi,no) TDX(oi,no,68)

#define NSSN525NS(oi,no) NSSN(oi,no,53)
#define NSSN1200NS(oi,no) NSSN(oi,no,120)
#define NSSN2800NS(oi,no) NSSN(oi,no,280)
#define NSSN3US(oi,no) NSSN(oi,no,300)

 // shortened by factor 1000 for C version
#define NSSN30MS(oi,no) NSSN(oi,no,3000)
// shortened by factor 1M
#define NSSN1000MS(oi,no) NSSN(oi,no,100)
// shortened by factor 100K
#define NSSN2500MS(oi,no) NSSN(oi,no,2500)

#define SS200NS(oi,no) SS(oi,no,19)
#define SS300NS(oi,no) SS(oi,no,30)
#define SS400NS(oi,no) SS(oi,no,40)
#define SS600NS(oi,no) SS(oi,no,60)

#define DOUBLESS200NS(oi,no) SS(oi,no,39)

#define SSN5500US(oi,no) SSN(oi,no,550000)

#define OSC5MC(oi,no) OSCX(oi,no,10)
#define OSC5_128MC(oi,no) OSCX(oi,no,9)

#define SE(...)
#define IOCE(...)
#define PANEL(...)

}
