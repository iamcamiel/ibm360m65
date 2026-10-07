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

#include "360_struc.h"
#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>
#include <strstream>
#include <sstream>
#include <map>

#pragma warning (disable: 4244 4018)

extern "C" {
    DATA360 oldstate;
    DATA360 newstate;

    FILE* lf;
    bool logging = true;
    bool mon_cycle = false;
    bool mon_ros = false;
    bool mon_20 = false;
    bool mon_stor = false;

    _ROS ros[4096] = { 0 };

    char* aa[16] = { "",            "B",          "B,IC",         "A,B",        "AB17",      "AB",    "AB38", "B38M",
                     "J9B8",        "MS->AB",     "IC",           "A",          "J49B",      "B8",    "AB18", "{A15}" };
    int   ao[16] = { 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0 };
    char* bb[4] = { "",            "->S",        "->T",          "->S,T" };
    char* cc[32] = { "",            "PSW->ST",    "PSW->S23",     "ST04->PSW",  "T",         "DT",    "JWT",  "T,D",
                     "MS1->T",      "MS->ST",     "MS0->T",       "ARD-KEYS->D","DWS",       "{C13}", "ST",   "D",
                     "ATRSEL^RESET","T4->PSW",    "DATA-KEYS->ST","TB->^MCW",   "MS->Q",     "TIF",   "G",    "^LM->N,Q->R*IC",
                     "DTS",         "{C25}",      "MS->T*D(21)",  "{C27}",      "S0->PSW",   "MS0->S","^K",   "^B" };
    int   co[32] = { 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 3, 0, 2, 1,
                     0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 0, 0, 0, 0, 1, 2 };
    char* dd[8] = { "",            "NEOP",       "EEOP",         "BEOP",       "DIRCNTL->F","F0",    "F1",   "F" };
    int   d_[8] = { 0, 0, 0, 0, 0, 2, 2, 2 };
    char* ee[32] = { "",        "E2+1",    "E3+1",    "E",       "E2,3-1",  "E2-1",    "E3-1",    "E23-1",
                     "CON->E3", "D1->E3",  "R->E",    "R",       "Q01->R",  "Q23->R",  "Q45->R",  "Q67->R",
                     "^NX->V",  "0001->V0","0010->V0","0011->V0","0100->V0","0101->V0","0110->V0","0111->V0",
                     "1000->V0","1001->V0","1010->V0","1011->V0","1100->V0","1101->V0","1110->V0","1111->V0" };
    char* e_ = " CCACCCCDDDADDDDBBBBBBBBBBBBBBBB";
    char* ff[64] = { "",             "RESET",       "SIGNS->STATS", "1->STAA*J47=0","0->STAA*J47<>0","1->STAB*W=0 0","SAVE-SIGNS",   "1->STOP-LOOP",
                     "0->STAD",      "1->STAD",     "A(07)=0->INH", "A<>0->INH",    "MPLY-END",      "^RDD-T-O-CHK", "IC->ABC",      "0->ABC",
                     "^1->IVSPEC",   "1->TCL-TGR",  "DVD-CHECK",    "1->INV-OP-TGR","1->INTREQ-TGR", "1->OFLO/UFLO", "SAJOS",        "^512CRY->ICLT",
                     "SASCR",        "0->INT-TGR",  "1->ASC*J57=0", "0->ASC,XEC",   "0->TIME-STEP",  "1->TIME-GATE", "0->TIME-GATE", "INH-W-PAR-CK",
                     "^0->CC",       "^1->CC",      "^2->CC",       "^3->CC",       "STAC->STAF",    "^512CRY->DLT", "IF-INV-TGR",   "1->STAB*J31=1",
                     "RSLT-SIGN->LS","1->SCAN-MODE","1->XEC-TGR",   "J7->STC",      "0->STP,STPLP",  "0->STAG",      "J7->ABC",      "D->STC",
                     "ABC,STC-1",    "ABC-1",       "STC-1",        "0->STC",       "ABC,STC+1",     "ABC+1",        "STC+1",        "3->STC",
                     "E3->ABC,STC",  "E3->ABC",     "E3->STC",      "1->STC(00)",   "EDIT-CTL",      "^ABC-1,STC+1", "{F62}",        "SET-CR" };
    char* gg[32] = { "",             "1->STAA*W<>0","1->STAA*J18=0","FXPOFLO->STAB","^SE-DEF->F",    "0->STAH",      "DECOFLO->STAB","1->STAB,STAG",
                     "1->STAG",      "Q->R*D",      "^1->INTR-GATE","^0->INTR-GATE","{G12}",         "1->STAH",      "1->STAB*J2632","RASCR",
                     "0->IC(21,22)", "1->IC(21-22)","2->IC(21-22)", "3->IC(21,22)", "DVDL0",         "DVDL1",        "1->INST-MSREQ","0->BR-INV-ADR",
                     "INV-ADR-TGR",  "INH-MS-PROT", "SET-KEY",      "INSERT-KEY",   "^CD-ID->F",     "^PIR->F",      "SEL-MPL*E3",   "TEST-AND-SET" };
    char* hh[128] = { "",            "^T->EXTREG", "^T->PSBAR",    "^T->SELREG", "RG*Q0",     "RG*Q2", "RG*Q4","RG*Q6",
                     "RF*E2_o_1",   "WF*E2_o_1",  "RF*E2",        "WF*E2",      "RG*E2_o_1", "WG*E2_o_1","RG*E2","WG*E2",
                     "RF*E3_o_1",   "RF*E3",      "RG*E3_o_1",    "W*E11-15",   "R*E3",      "{H21}", "{H22}","R*E11-15",
                     "RF*R2",       "RG*R2",      "R*24",         "W*24",       "{H28}",     "^T->DARMSK","{H30}","^ST04->ATR",
                     "R^GP0",       "R^GP1",      "R^GP2",        "R^GP3",      "R^GP4",     "R^GP5", "R^GP6","R^GP7",
                     "R^GP8",       "R^GP9",      "R^GP10",       "R^GP11",     "R^GP12",    "R^GP13","R^GP14","R^GP15",
                     "R^FP0",       "R^FP1",      "R^FP2",        "R^FP3",      "R^FP4",     "R^FP5", "R^FP6","R^FP7",
                     "{H56}",       "{H57}",      "{H58}",        "{H59}",      "{H60}",     "{H61}", "{H62}","{H63}",
                     "^^^",       "R^EXTREG",   "R^PSBAR",      "{H67}",      "{H68}",     "{H69}", "{H70}","{H71}",
                     "{H72}",       "{H73}",      "{H74}",        "{H75}",      "{H76}",     "{H77}", "{H78}","{H79}",
                     "{H80}",       "{H81}",      "{H82}",        "{H83}",      "{H84}",     "R^SELREG","{H86}","{H87}",
                     "{H88}",       "{H89}",      "{H90}",        "{H91}",      "R^CKREG2",  "R^DARMSK","R^CCR","R^ATR1",
                     "W^GP0",       "W^GP1",      "W^GP2",        "W^GP3",      "W^GP4",     "W^GP5", "W^GP6","W^GP7",
                     "W^GP8",       "W^GP9",      "W^GP10",       "W^GP11",     "W^GP12",    "W^GP13","W^GP14","W^GP15",
                     "W^FP0",       "W^FP1",      "W^FP2",        "W^FP3",      "W^FP4",     "W^FP5", "W^FP6","W^FP7",
                     "{H120}",      "{H121}",     "{H122}",       "{H123}",     "{H124}",    "{H125}","{H126}","{H127}" };
    char* jj[128] = { "0",           "1",          "W-CRY",        "T(32)",      "JCRY28",    "LS-PB", "^TCS",  "ABC=0",
                     "J47=0.OFLO",  "J57=0",      "J47=0",        "BCNM_o_",    "^LMT",      "IC3=3", "]F04.", "D2=3",
                     "E3<>1",       "E3<>3",      "]STAC",        "MCW04",      "XECTGR",    "STO-PB","E(03)", "STC=7",
                     "]E3.RR",      "DIS-PB",     "HSMOVE",       "^ATRSEL",    "UFLO",      "M/DVD", "IPL^PSW","IC-PB",
                     "E3<>4",       "E2=E3",      "STT-PB",       "^FLT",       "HLD^IN",    "SB-PB", "D22=1",  "ABC=7",
                     "^T(63)",      "^IOCE",      "^IOERR",       "^STATE0",    "^REG-SET",  "^512CRY","^SAS=18","{J47}",
                     "E3=255",      "SAS=0",      "STAB",         "{J51}",      "RELI/O",    "SAS=13","{J55}",  "STC<>3",
                     "{J56}",       "{J57}",      "{J58}",        "{J59}",      "{J60}",     "{J61}", "{J62}",  "{J63}",
                     "4<E3",        "ABC<>3",     "STAH",         "CONTIN",     "W=0",       "TCS/ST","{J70}",  "3<STC",
                     "{J72}",       "{J73}",      "{J74}",        "{J75}",      "{J76}",     "{J77}", "{J78}",  "{J79}",
                     "J47=1",       "STAA",       "DECDIV",       "^TIC",       "STAF=C",    "ROS-PB","JCRY4",  "{J87}",
                     "{J88}",       "{J89}",      "{J90}",        "{J91}",      "{J92}",     "{J93}", "{J94}",  "{J95}",
                     "DREG(18-23)", "NEXT-INST*IC","DECIMAL",     "IC(21-22)",  "W1=(01-15)","STAE,F1SGN+","E(04-07)->ROA","E(02-07)->ROA",
                     "EDIT",        "FLPT-UN,COMP","^W(01-02)",   "LOGIC-COMP", "STAD,STAG", "J1=0/1,J18=0","SAS2,3,4","J1=0,J17=0",
                     "{J112}",      "{J113}",     "{J114}",       "{J115}",     "{J116}",    "{J117}","{J118}", "{J119}",
                     "FLR",         "NEXT-INST*D","J(58-63)",     "A1=0,J5=0)", "ALT,MS-TEST","^J47<>0.^]CRY","{J126}","{J127}" };
    char* kk[32] = { "0",           "1",          "E3=15",        "E2=0/",      "E2=15",     "STAD",  "D(21)","^RSS",
                     "STAG",        "PSW39",      "DEC",          "STAE",       "]RR.C",     "W1=15", "J47<>0","W1=1",
                     "E23=0",       "WCRY",       "E=0/",         "E3=0",       "E3=0/",     "INTRP", "F1<>1", "F1<>9",
                     "T->RAR",      "EXCEP",      "6<E23",        "MOVE",       "UFMSK",     "{K29}", "{K30}", "SPEC" };
    char* ll[16] = { "",            "STOP1",      "STOP2",        "^MS-REQ-LOG","SET-MARK-0-7","SET-MARK-0-3","SET-MARK*STC","SET-MARK*J61",
                    "MS-REQ*IC-3", "MS-REQ*IC-4","MS-REQ*D-3",   "MS-REQ*D-4", "MS*IC-3*D=11","MS-REQ*SCAN4","^RQ-XY-STO*D","{L15}" };
    char* mm[32] = { "+0",          "{M1}",       "{M2}",         "{M3}",       "+^DECAB",   "-U,CRY","{M6}", "-^U",
                     "{M8}",        "{M9}",       "{M10}",        "{M11}",      "?U*E1",     "+U1,U0","{M14}","+U1,0",
                     "{M16}",       "+U0,0",      ".U",           "+U",         "vU",        "+15,U0","_o_U", "+15,U1",
                     "+6U",         "+6U0,0",     "+6U0,S",       "+0,U1",      "-U",        "+0,U0", "-U0,0","+0,U" };
    char* nn[16] = { "0",           "{N1}",       "V",            "0,V1",       "V0,+",      "QUOT",  "0,V",  "9,V1",
                     "-64",         "{N9}",       "V0,0",         "-1",         "V0,-",      "{N13}", "V0,V0","1" };
    char* pp[8] = { "->",          "SCAN-BYPASS","R->",          "{P3}",       "->HOLD",    "L->",   ",C8",  "^R1->" };
    char* qq[8] = { "",            "+P",         "+8",           "+1",         "-16",       "+TIME", "-8",   "-1" };
    char* rr[2] = { "AB->U",       "F->U" };
    char* tt[16] = { "",            "BL2",        "IC",           "DEC",        "F1",        "ABL2",  "8",    "B8J9",
                     "A",           "B8",         "A13",          "B",          "AB",        "^2",    "AB17", "B489" };
    char* uu[32] = { "+0",          "-TL1",       "-D",           "+D",         "+S",        "-DTL1", "+TL1", "+DTL1",
                     "+T45R",       "+T67",       "-D+7",         "+DJ13",      "-DT",       "-T",    "+T",   "+DT",
                     "+^K",         "^FMT0*E13-15","^FMTN*E13-15","^FMTW*E14-15","+^2",      "+^32",  "{U22}","{U23}",
                     "+^T45R",      "+^T67",      "{U26}",        "{U27}",      "{U28}",     "{U29}", "{U30}","{U31}" };
    char* vv[8] = { "0",           "E3",         "E2",           "E23",        "Q7",        "Q5",    "Q3",   "Q1" };
    char* ww[16] = { "",            "^13->ADR-SQCR","^LMT1",      "^LMT2",      "SAMPLE^VALUE","^MS->LM","{W6}","{W7}",
                     "R^ATR2",      "^LOAD-REG",  "NO->V",        "R^GREG",     "R^EXTBUS",  "{W13}", "{W14}","{W15}" };
    char* w_ = " CCCCD  LDBLLLB ";

    void list_rosword(int a, bool fmt = false) {
        if (!mon_ros) return;

        _ROS& rw = ros[a];

        if (fmt) {
            D_fprintf(lf, "%03X: A%d B%d C%d D%d E%d F%d G%d H%d J%d K%d L%d M%d N%d P%d Q%d R%d T%d U%d V%d W%d NA%03x\n",
                a, rw.a, rw.b, rw.c, rw.d, rw.e, rw.f, rw.g, rw.h, rw.j, rw.k, rw.l, rw.m, rw.n, rw.p, rw.q, rw.r, rw.t, rw.u, rw.v, rw.w, rw.na << 2);
        }

        D_fprintf(lf, "     %03X: ", a);
        if (rw.a && !ao[rw.a]) { D_fprintf(lf, "%s%s%s", fmt ? "D: " : "", aa[rw.a], fmt ? "\n     " : " ; "); }
        if (rw.c && !co[rw.c]) { D_fprintf(lf, "%s%s%s", fmt ? "D: " : "", cc[rw.c], fmt ? "\n     " : " ; "); }
        if (rw.d && !d_[rw.d]) { D_fprintf(lf, "%s%s%s", fmt ? "C: " : "", dd[rw.d], fmt ? "\n     " : " ; "); }

        if (rw.t || rw.v || rw.q || rw.u || rw.p || ao[rw.a] & 1 || co[rw.c] & 1) {
            if (fmt) D_fprintf(lf, "A: ");
            if (rw.t) {
                if (rw.v) {
                    D_fprintf(lf, "%s,%s", tt[rw.t], vv[rw.v]);
                }
                else {
                    D_fprintf(lf, "%s", tt[rw.t]);
                }
            }
            else {
                D_fprintf(lf, "%s", vv[rw.v]);
            }
            if (rw.q) {
                if (rw.u) {
                    D_fprintf(lf, "%s,%s", qq[rw.q], uu[rw.u]);
                }
                else {
                    D_fprintf(lf, "%s", qq[rw.q]);
                }
            }
            else {
                D_fprintf(lf, "%s", uu[rw.u]);
            }
            D_fprintf(lf, " %s", pp[rw.p]);
            if (ao[rw.a] & 1) {
                if (co[rw.c] & 1) {
                    D_fprintf(lf, " %s,%s", aa[rw.a], cc[rw.c]);
                }
                else {
                    D_fprintf(lf, " %s", aa[rw.a]);
                }
            }
            else if (co[rw.c] & 1) {
                D_fprintf(lf, " %s", cc[rw.c]);
            }
            D_fprintf(lf, fmt ? "\n     " : " ; ");
        }
        if (rw.r || rw.n || rw.m || co[rw.c] & 2 || d_[rw.d] & 2) {
            if (fmt) D_fprintf(lf, "B: ");
            D_fprintf(lf, "%s %s%s ->", rr[rw.r], nn[rw.n], mm[rw.m]);
            if (co[rw.c] & 2) {
                if (d_[rw.d] & 2) {
                    D_fprintf(lf, " %s,%s", cc[rw.c], dd[rw.d]);
                }
                else {
                    D_fprintf(lf, " %s", cc[rw.c]);
                }
            }
            else if (d_[rw.d] & 2) {
                D_fprintf(lf, " %s", dd[rw.d]);
            }
            D_fprintf(lf, fmt ? "\n     " : " ; ");
        }
        if (rw.e) {
            if (fmt) D_fprintf(lf, "%c: ", e_[rw.e]);
            D_fprintf(lf, "%s%s", ee[rw.e], fmt ? "\n     " : " ; ");
        }
        if (rw.h || rw.b) { D_fprintf(lf, "%s%s %s%s", fmt ? "L: " : "", hh[rw.h], bb[rw.b], fmt ? "\n     " : " ; "); }
        if (rw.l) D_fprintf(lf, "%s%s%s", fmt ? "   " : "", ll[rw.l], fmt ? "\n     " : " ; ");
        if (rw.f) D_fprintf(lf, "%s%s%s", fmt ? ((rw.f == 14 || rw.f == 43 || rw.f == 46 || rw.f == 47 || rw.f == 56 || rw.f == 57 || rw.f == 58) ? "D: " : "C: ") : "", ff[rw.f], fmt ? "\n     " : " ; ");
        if (rw.g) D_fprintf(lf, "%s%s%s", fmt ? ((rw.g == 28 || rw.g == 29) ? "D: " : "C: ") : "", gg[rw.g], fmt ? "\n     " : " ; ");
        if (rw.w) {
            if (fmt) D_fprintf(lf, "%c: ", w_[rw.w]);
            D_fprintf(lf, "%s%s", ww[rw.w], fmt ? "\n     " : " ; ");
        }
        D_fprintf(lf, "%s%03X %s %s%s", fmt ? "R: " : "==> ", rw.na << 2, kk[rw.k], jj[rw.j], fmt ? "\n\n" : "\n");
    }

    char* uop(char x, int y) {
        switch (x) {
        case 'a': return aa[y];
        case 'b': return bb[y];
        case 'c': return cc[y];
        case 'd': return dd[y];
        case 'e': return ee[y];
        case 'f': return ff[y];
        case 'g': return gg[y];
        case 'h': return hh[y];
        case 'j': return jj[y];
        case 'k': return kk[y];
        case 'l': return ll[y];
        case 'm': return mm[y];
        case 'n': return nn[y];
        case 'p': return pp[y];
        case 'q': return qq[y];
        case 'r': return rr[y];
        case 't': return tt[y];
        case 'u': return uu[y];
        case 'v': return vv[y];
        case 'w': return ww[y];
        }
        return "??";
    }

    extern "C" int listing(int aa) {
        list_rosword(aa);
        return ros[aa].valid ? 1 : 0;
    }

    extern "C" void init_ros() {
        //  sf = fopen("m65s.log","w");
        bool fault = false;
        int ld = 0;
        int vd = 0;

        std::ifstream fin("ros.txt");

        int pr_addr = 0;
        while (fin.peek() != std::iostream::traits_type::eof()) {
            std::string buffer;
            std::getline(fin, buffer);
            if (buffer.length() < 10) continue;
            std::stringstream ss;
            int adr;
            int dta[100];
            ss << buffer;
            ss >> std::hex >> adr;
            if (pr_addr && adr <= pr_addr) {
                D_fprintf(lf, "Address %03x < %03x\n", adr, pr_addr);
                fault = true;
            }
            pr_addr = adr;
            int d = 0;
            int z = 0;
            dta[0] = 0;
            _ROS& rw = ros[adr];

            for (int j = 4; j < buffer.length() && z < 100; j++) {
                switch (buffer[j]) {
                case '0':
                    dta[d] <<= 1;
                    rw.bits[z++] = false;
                    break;
                case '1':
                    dta[d] <<= 1;
                    dta[d]++;
                    rw.bits[z++] = true;
                    break;
                case ' ':
                    d++;
                    dta[d] = 0;
                    break;
                default:
                    printf("Invalid character (%c) in ROS word %03x at %d\n", buffer[j], adr, j);
                    fault = true;
                }
            }


            rw.a = dta[3];
            rw.b = dta[4];
            rw.c = dta[5];
            rw.d = dta[6];
            rw.e = dta[8] | (dta[22] << 4);
            rw.f = dta[9];
            rw.g = dta[10];
            rw.h = dta[12] | (dta[11] << 5);
            rw.l = dta[13];
            rw.na = (dta[14] << 6) | (dta[15] << 2) | dta[16];
            rw.k = dta[17];
            rw.j = dta[18];
            rw.m = dta[19];
            rw.n = dta[20];
            rw.p = dta[21];
            rw.q = dta[23];
            rw.r = dta[25];
            rw.t = dta[26];
            rw.u = dta[28] | (dta[29] << 4);
            rw.v = dta[30];
            rw.w = dta[2];
            rw.valid = (dta[1] == 0);
            ld++;
            //    if (rw.valid) 
            vd++;

            if (rw.valid) {
                bool pty = false;
                for (int i = 2; i < 43; i++) {
                    pty ^= rw.bits[i];
                }
                if (!pty) {
                    printf("ROS word %03x parity 2-42 mismatch.\n", adr);
                    fault = true;
                }
                pty = false;
                for (int i = 43; i < 69; i++) {
                    pty ^= rw.bits[i];
                }
                pty ^= rw.bits[85];
                if (!pty) {
                    printf("ROS word %03x parity 43-68 mismatch.\n", adr);
                    fault = true;
                }
                pty = false;
                for (int i = 69; i < 100; i++) {
                    if (i != 85) pty ^= rw.bits[i];
                }
                if (!pty) {
                    printf("ROS word %03x parity 69-99 mismatch.\n", adr);
                    fault = true;
                }
            }
        }
        if (fault) {
            printf("ROS read errors!!\n");
            //getchar();
            exit(1);
        }

        printf("%d ROS words loaded (%d valid) out of 2816.\n", ld, vd);
        /*
        FILE * vhdl;
        vhdl = fopen("c:\\ros.vhdl","w");
        for (int i=0; i<2816; i++) {
          int rar = ((i & 0xf80)>>5)
            | ((i & 0x7c)<<5)
            | (i & 0x3);
          fprintf(vhdl, "   \"");
          for (int j=0; j<100; j++) fprintf(vhdl,"%s", ros_data[rar].bits[j]?"1":"0");
          fprintf(vhdl, "\",  -- ROS word %03x\n",rar);
        }
        fclose(vhdl);
        */

        //  ros_data[0x091].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 091 to fix TEST UNDER MASK
        //  ros_data[0x098].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 098 to fix LOAD MULTIPLE
        //  ros_data[0x108].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 108 to fix LOAD HALFWORD
        //  ros_data[0x100].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 100 to fix STORE HALFWORD
        ////  ros_data[0x115].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 115 to fix COMPARE LOGICAL
        //  ros_data[0x118].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 118 to fix LOAD
        //  ros_data[0x128].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 128 to fix LOAD FP LONG
        //  ros_data[0x219].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 219 to fix STORE
        //  ros_data[0x21a].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 21A to fix STORE
        //  ros_data[0x21d].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 21D to fix STORE
        //  ros_data[0x21e].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 21E to fix STORE
        //  ros_data[0x221].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 221 to fix COMPARE LOGICAL IMMEDIATE
        //  ros_data[0x222].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 222 to fix OR IMMEDIATE
        //  ros_data[0x225].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 225 to fix OR IMMEDIATE
        //  ros_data[0x226].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 226 to fix COMPARE LOGICAL IMMEDIATE
        //  //ros_data[0x231].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 231 to fix PACK
        //  ros_data[0x3ac].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 3AC to fix MOVE CHARACTER
        //  ros_data[0x3ad].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 3AD to fix MOVE ZONES
        //  ros_data[0x3ae].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 3AE to fix MOVE CHARACTER
        //  ros_data[0x3b4].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 3B4 to fix PACK
        //  ros_data[0x52e].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 52E to fix MOVE CHARACTER
        //  ros_data[0x79c].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 79C to fix MOVE CHARACTER
        //  ros_data[0x930].l = 10; // @KLUDGE: INSERT MS-REQ*D-3 into ROS WORD 79C to fix MOVE CHARACTER (w/overlap)
        //
        //  ros_data[0x186].a = 0;  // @KLUDGE: REMOVE ->B from ROS WORD 186 to fix BRANCH COUNT REGISTER
        //  ros_data[0x38e].q = 0;  // @KLUDGE: REMOVE +8 from ROS WORD 38E to fix MOVE CHARACTER
        //  ros_data[0x3a6].q = 0;  // @KLUDGE: REMOVE -8 from ROS WORD 3A6 to fix EXCLUSIVE OR CHARACTER
        //  ros_data[0x8af].q = 0;  // @KLUDGE: REMOVE +8 from ROS WORD 8AF to fix BRANCH COUNT
        //  ros_data[0x935].t = 0;  // @KLUDGE: REMOVE 8 from ROS WORD 935 to fix BRANCH AND LINK REGISTER
        //  ros_data[0x9ae].j = 0;  // @KLUDGE: REMOVE D2=3 from ROS WORD 9AE to fix BRANCH CONDITIONAL
        //
        //  ros_data[0x43e].l = 0;
        //  ros_data[0x3be].l = 6; // @KLUDGE: move SET-MARK*STC later to fix STORE HALFWORD
        //
        //  ros_data[0x351].q = 6; // @KLUDGE change -16 to -8 in ROS WORD 351 to fix SERVICE CALL

        //  ros.rosar = 0x00b; // machine reset

        //for (int a = 0; a < 4096; a++) {
        //  if (ros_data[a].valid) list_rosword(a, true);
        //}
        //getchar();
        //printf("==========\n");
        memset(&oldstate, 0, sizeof(oldstate));

    }

    void full_init() {
        const char* cycle_trace = std::getenv("M65_TRACE_CYCLES");
        const char* ros_trace = std::getenv("M65_TRACE_ROS");
        const char* interval_timer = std::getenv("M65_INTERVAL_TIMER");
        const bool disable_interval_timer = interval_timer && std::strcmp(interval_timer, "0") == 0;
        mon_cycle = cycle_trace && std::strcmp(cycle_trace, "1") == 0;
        mon_ros = ros_trace && std::strcmp(ros_trace, "1") == 0;

        init_ros();

        init_ald();

        newstate.EXTERNAL_.switches_0.B11 = true;
        newstate.EXTERNAL_.switches_0.B12 = true;
        newstate.EXTERNAL_.switches_0.B13 = true;
        newstate.EXTERNAL_.switches_0.B14 = true;
        newstate.EXTERNAL_.switches_1.B3 = true;
        newstate.EXTERNAL_.switches_1.B4 = true;
        newstate.EXTERNAL_.switches_6.B6 = true;
        newstate.EXTERNAL_.switches_6.B7 = true;
        newstate.EXTERNAL_.switches_6.B8 = true;
        newstate.EXTERNAL_.switches_6.B9 = true;
        newstate.EXTERNAL_.switches_6.B14 = true;
        newstate.EXTERNAL_.switches_6.B15 = true;
        newstate.EXTERNAL_.switches_6.B16 = true;
        newstate.EXTERNAL_.switches_6.B17 = true;
        newstate.EXTERNAL_.switches_6.B18 = true;

        newstate.EXTERNAL_.switches_7.B15 = true;
        newstate.EXTERNAL_.switches_7.B3 = true;
        newstate.EXTERNAL_.switches_7.B14 = true;
        newstate.EXTERNAL_.switches_7.B6 = true;
        newstate.EXTERNAL_.switches_7.B9 = true;
        newstate.EXTERNAL_.switches_7.B0 = true;
        // Active-low console DISABLE TIMER switch (ALD PK051/KW371).
        newstate.EXTERNAL_.switches_7.B13 = !disable_interval_timer;
        newstate.EXTERNAL_.switches_7.B10 = true;
        newstate.EXTERNAL_.switches_7.B1 = true;
        newstate.EXTERNAL_.switches_7.B4 = true;
        newstate.EXTERNAL_.switches_7.B12 = true;
        newstate.EXTERNAL_.switches_7.B8 = true;
        newstate.EXTERNAL_.switches_7.B11 = true;
        newstate.EXTERNAL_.switches_7.B2 = true;

        // BELOW ARE 360 specific

        newstate.PK_PL_INT._storage_indicate_5M8 = !oldstate.EXTERNAL_.switches_7.B7;

        newstate.PK_PL_INT.stop_on_storage_check = !oldstate.EXTERNAL_.switches_7.B5;




        newstate.KW.pass_pulse_tgr = true;
        newstate.KW._pass_pulse_tgr = false;

        newstate.RX.rosar.B8 = newstate.RX.rosar.B10 = newstate.RX.rosar.B11 = true;
        newstate.RX._rosar.B8 = newstate.RX._rosar.B10 = newstate.RX._rosar.B11 = false;

        newstate.EXTERNAL_.power_on_reset = true;


        // DISABLE CE CHECKS
        newstate.EXTERNAL_.switches_7.B11 = false;

    }

    void mon(char i) {
        D_fprintf(lf, "%c ", i ? '+' : '|');
    }

    void cycle_mon() {
        if (!mon_cycle) return;
        D_fprintf(lf, "SW=%06x ", newstate.EXTERNAL_.switches_0.F);
//        D_fprintf(lf, "UA=%02x ", newstate.PK_PL.ce_load_unit_addr_bit.F);
//        D_fprintf(lf, "SEL/STO/SET/INS/CAN: ");
//        mon(!newstate.MC._select_se_1_even);
//        mon(newstate.MC.store_to_se);
//        mon(newstate.MC.set_key);
//        mon(newstate.MC.insert_key);
//        mon(newstate.MC.cancel);

        //D_fprintf(lf, "ACC/ADV: ");
        //mon(!newstate.WA._accept_se_1);
        //mon(newstate.WA.advance_sdbo);

        //D_fprintf(lf, "SDBO=%016llx ", newstate.WA.sdbo.F);

        //D_fprintf(lf, "SEL/BUSY: ");
        //mon(!newstate.MC._select_se_1_even);
        //mon(newstate.WA_INT.still_busy);
        //mon(newstate.WA_INT.do_sel);
        //D_fprintf(lf, "STO/INS/SET/CANCEL: ");
        //mon(newstate.MC.store_to_se);
        //mon(newstate.WA_INT.store);
        //mon(newstate.MC.insert_key);
        //mon(newstate.WA_INT.insert_key);
        //mon(newstate.MC.set_key);
        //mon(newstate.WA_INT.set_key);
        //mon(newstate.MC.cancel);
        //mon(newstate.WA_INT.cancel);
        //D_fprintf(lf, "CMD: %x ", newstate.WA.reg_se_cmd.F);
        //D_fprintf(lf, "ADDR: %x ", newstate.WA.reg_se_addr.F);
        //D_fprintf(lf, "RESP: %x ", newstate.EXTERNAL_.reg_se_resp.F);
        //mon(newstate.WA_INT.se_counter_out.B0);
        //mon(newstate.WA_INT.se_counter_out.B1);
        //mon(newstate.EXTERNAL_.reg_se_resp.B31);
        //mon(newstate.EXTERNAL_.reg_se_resp.B30);
        //mon(newstate.WA_INT.se_counter_eq);
        //D_fprintf(lf, "PULSES: ");
        //mon(newstate.WA_INT.se_counter_pulse);
        //mon(newstate.WA_INT.delay_sel);
        //mon(newstate.WA_INT.accept_pulse);
        //mon(newstate.WA_INT.advance_pulse);
        //D_fprintf(lf, "ACC/ADV: ");
        //mon(newstate.WA_INT.accept);
        //mon(newstate.WA.advance_sdbo);
        //mon(!newstate.KD._turn_on_i_fetch_1_at_eop);
        //mon(newstate.KD_INT._temp_kd101_2c_ag_nor);
        //mon(newstate.KD_INT.temp_kd101_3b_an_andnot);   // too early?
        //mon(newstate.KD_INT.ros_decode_branch_eop);
        //D_fprintf(lf, "  ");
        //mon(newstate.KM_INT.temp_i_f_forced_ad);
        //mon(!newstate.KM_INT.temp559);
        //mon(!newstate.KD._i_fetch_storage_req_in_prog);
        //mon(!newstate.KD._turn_on_i_fetch_1_at_eop);

        //mon(newstate.RR.r.B2);
        //mon(newstate.RE_INT.gate_r_00M07_to_e00M07Pp);
        //mon(newstate.RE_INT._reset_e_00M07_O_parity);
        //mon(newstate.RE_INT.clock_p2);
        //D_fprintf(lf, "E=%04x ", newstate.RE.e_bit.F);
        //mon(newstate.RX_INT.p4_gate);
        //mon(newstate.DS.normal_set_rosar_bit_7);
        //mon(newstate.RX_INT._inhibit_next_address);
        //mon(newstate.DS.j97Pj121Oeff_r01_to_rosar_7);
        //mon(newstate.DS.j103_to_rosar_6_7_10);
        //mon(newstate.RE.e_bit.B3);
        //mon(newstate.KM.rosar_bit_7Od_reg_17_yiaei);
        //mon(newstate.RX_INT._scanMmc_inh_int_cond);
        //mon(newstate.RX_INT.yiaei_end_op);
        //mon(newstate.KM.block_rosar_O_set_bit_7_yiasi);
        //mon(newstate.RX_INT.yiasi);
        //mon(newstate.RX_INT.temp1194);
        //mon(newstate.RX_INT._clock_p3_blocked);

        //mon(newstate.KD.i_fetch_2_lth);
        //mon(newstate.KD.rx_and_e12M15_eq_0_or_rs);
        //mon(!newstate.KM_INT._specif_error_dPstPe_reg);
        //mon(!newstate.KM_INT._specif_error_pal);
        //mon(newstate.KM_INT._temp_d_reg);
        //mon(newstate.KM_INT.e_reg_decode_inst_a);
        //mon(newstate.KM_INT.e_reg_decode_inst_b);
        //mon(newstate.KM_INT.e_reg_decode_inst_c);

        //D_fprintf(lf, "AB=%01llx%016llx ", newstate.RA_RB.ab_bit.F2,newstate.RA_RB.ab_bit.F);
        //D_fprintf(lf, "ABC=%02x ", newstate.CW.abc_incr_equal.F);
        //D_fprintf(lf, "SIBA=%02x ", newstate.AR_INT.siba.F);
        //D_fprintf(lf, "SA=%02x ", -1 - newstate.AR._sadda.F);
        //D_fprintf(lf, "ST=%016llx ", newstate.RS_RT.st_bit.F);
        //D_fprintf(lf, "STC=%02x ", newstate.CS.stc_incr_equal.F);
//        mon(newstate.AP.inhibit_st_to_sibb);
        //mon(newstate.RS_RT_INT.clock_p2_uninh);
        //mon(newstate.RS_RT_INT._clock_p1_uninh);
        //D_fprintf(lf, "%02x ", newstate.RS_RT_INT.gt_st_byte_to_sibb.F);
        //D_fprintf(lf, "SIBB=%02x ", newstate.AR.sibb.F);
        //D_fprintf(lf, "SB=%02x ", -1 - newstate.AR._saddb.F);
        ////D_fprintf(lf, "HS=%02x ", -1 - newstate.AS_INT._half_sum.F);
        //D_fprintf(lf, "FS=%02x ", newstate.AS_INT.sum.F);
        //mon(newstate.AS_INT._clock_p3);
        //mon(newstate.AS.clock_p2);
        //D_fprintf(lf, "SAL=%02x ", newstate.AS.saddl.F);
        //mon(newstate.RW_INT.gate_st_to_psw_0_to_7);
        //mon(newstate.RW_INT._gate_st_to_psw_0_to_7);
        //D_fprintf(lf, "PSW=%010llx ", newstate.RW.psw_bit.F);
        //mon(newstate.RS_RT_INT.gt_pswMs00M15t32M39);
        //mon(newstate.RS_RT_INT._rst_pswMs00M15t32M39);

        //D_fprintf(lf, "1: ");
        //mon(newstate.KZ.dig_examine_lth);
        //mon(!newstate.AR._invalid_digit_sba_00M03);
        //mon(newstate.KS_INT.clock_p2P140);
        //D_fprintf(lf, "2: ");
        //mon(newstate.DR.temp_dr081_f_l.B2);
        //mon(newstate.KS_INT.vfl_inv_sign);
        //mon(newstate.KS_INT.clock_p2P140);
        //D_fprintf(lf, "3: ");
        //mon(newstate.DR.temp_dr081_f_l.B6);
        //mon(newstate.KS._fl_pt);
        //mon(newstate.AR.invalid_sign_sbb_04M07);
        //mon(newstate.KS_INT.clock_p2P140);
        //D_fprintf(lf, "4: ");
        //mon(newstate.AR.sign_cor_sba_04M07);
        //mon(newstate.AR._invalid_digit_sba_04M07);
        //mon(newstate.KS_INT.clock_p2P140);
        //D_fprintf(lf, "5: ");
        //mon(newstate.AR.invalid_digit_sa_00M03);
        //mon(newstate.KS_INT.clock_p2P140);
        //D_fprintf(lf, "6: ");
        //mon(newstate.AR.invalid_digit_sa_04M07);
        //mon(newstate.KS_INT.clock_p2P140);
        //D_fprintf(lf, "L: ");
        //mon(newstate.KS_INT._stat_tgr_reset);
        //D_fprintf(lf, "STAE: ");
        //mon(newstate.KS.stae_bus);

//        mon(newstate.DR.set_staa_if_sal00M07_not_eq_0);
//        mon(!newstate.AR._save_sa_carry);
//        mon(newstate.KS.staa);
//        mon(newstate.DR.gt_r_00M15_to_e_00M15);
//        mon(newstate.KM._exceptional_cond_to_i_fetch);
//        mon(newstate.RE_INT.clock_p0_minus_2);
//        mon(newstate.RE_INT._reset_e_00M07_O_parity);
//        mon(newstate.RE_INT.gate_r_00M07_to_e00M07Pp);
        //D_fprintf(lf,"1: ");
        //mon(newstate.DR.temp_dr081_f_l.B5);
        //mon(newstate.KS_INT.sal_0M7_equal_zero);
        //mon(newstate.KS_INT.stat_clock_p2);
        //D_fprintf(lf, "2: ");
        //mon(newstate.DR.temp_dr131_g_l.B6);
        //mon(newstate.KS_INT.dec_overflow);
        //mon(newstate.KS_INT.stat_clock_p2);
        //D_fprintf(lf, "3: ");
        //mon(newstate.DR.temp_dr081_f_l.B39);
        //mon(newstate.AP.paddl.B31);
        //mon(newstate.KS_INT.stat_clock_p2);
        //D_fprintf(lf, "4: ");
        //mon(newstate.DR.temp_dr131_g_l.B7);
        //mon(newstate.RA_RB.ab_bit.B32);
        //mon(newstate.KS_INT.clock_p2P140);
        //D_fprintf(lf, "5: ");
        //mon(newstate.DR.temp_dr131_g_l.B3);
        //mon(newstate.AP.fixed_point_overflow);
        //mon(newstate.KS_INT.stat_clock_p2);
        //D_fprintf(lf, "6: ");
        //mon(newstate.DR.temp_dr131_g_l.B14);
        //mon(newstate.AP.left_shift_o_v_check);
        //mon(newstate.KS_INT.stat_clock_p2);
        //D_fprintf(lf, "L: ");
        //mon(newstate.KS_INT._stat_tgr_reset);
        //mon(newstate.KS_INT._reset_stab);
        //mon(newstate.KS.stab_tgr);
        //D_fprintf(lf, "STAB: ");
        //mon(!newstate.KS._stab_tgr);
        //mon(newstate.KS.stab_tgr);
//        mon(newstate.AS.stah);
        //mon(newstate.LS_INT._clock_p0M1_cd4);
        //mon(newstate.LS_INT.clock_p0);
        //mon(newstate.LS_INT.clock_p2);
        //mon(!newstate.DR._ros_fld_37M42_eq_001xxx);
        //mon(newstate.RE_INT.gt_e_8M11_to_lal_01M04);

        //D_fprintf(lf, "E-LAL=%02x ", -1 - newstate.RE._e_to_lal.F);
        //D_fprintf(lf, "Q-LAL=%02x ", -1 - newstate.RQ._q_or_r_to_lal.F);
        //mon(!newstate.DR._read_P_write_lswr);
        //D_fprintf(lf, "LAL=%02x ",newstate.LS_INT.lal_bit.F);
        //D_fprintf(lf, "LAR=%02x ",newstate.LS_INT.lar.F);
        //D_fprintf(lf, "WAR=%02x ", newstate.LS_INT.wr_decode_reg.F);
        //mon(newstate.DR.write_local_store);
        //mon(newstate.LS_INT.temp_gt_wr);
        //mon(newstate.LS_INT.delayed_clock);
        //mon(newstate.LS_INT.mem_write_gate);
//        D_fprintf(lf, "%08x ", newstate.LS.ls_busMM.F);
//        mon(newstate.RS_RT_INT.gt_lsMs);
//        mon(newstate.FE_INT.gate_st32M63_to_ext_00M31);
//        D_fprintf(lf, "EXT=%08x ", newstate.FE.ext_reg.F);
//        D_fprintf(lf, "%02x ", -1-newstate.CS._stc_incr_equal.F);
//        mon(newstate.RS_RT_INT.clock_p2_uninh);
//        mon(newstate.RS_RT_INT._clock_p1_uninh);
//        mon(newstate.RE_INT.gate_r_00M07_to_e00M07Pp);
//        mon(newstate.RE_INT.gate_r8M15_to_e8M15);
//        mon(!newstate.WA._accept_se_1);
//        mon(newstate.EXTERNAL_.se_status.B10);
//        mon(newstate.KM_INT.process_stor_advance);
//        mon(!newstate.KM_INT._temp_store_to_ms);
//        mon(!newstate.KM._cpu_store_in_progress);

        //mon(newstate.KW._error_log_required);
        //mon(newstate.KU_INT._pulsed_split_log_to_soros_set);
        //mon(newstate.KU_INT.temp676);
        //mon(newstate.KU_INT.temp677);
        //mon(newstate.KU_INT.soros_tgr);
        //mon(newstate.KU_INT.sync_latch);
        //mon(newstate.KU.ros_test_P_flt_lth);
        //mon(newstate.KU.soros_O_sync_lth);
        //mon(newstate.KU_INT.soros_and_not_ros_testPflt);
        //mon(newstate.MC_INT.scan_sync_tgr);
        //mon(newstate.KD_INT._inhibit_operand_requests);
//        mon(!newstate.KD._storage_req_d_3_cycles);
        //mon(!newstate.KD._storage_req_d_4_cycles);
        //mon(newstate.MC_INT.d_storage_req);
        //mon(newstate.KD.cpu_clock_p4);
        //mon(newstate.MC_INT._clock_p1);
 //        mon(newstate.MC_INT._temp869); 
        // mon(newstate.MC_INT.d_sync_tgr);

        //mon(!newstate.MC_INT._d_sync_lth);
//        mon(newstate.MC_INT._ic_sync_lth);
//        mon(newstate.MC_INT._scan_sync_lth);
        //mon(newstate.MC_INT.cpu_request_sync_lth);
//        mon(newstate.MC_INT._cpu_2_tgr);
        //mon(newstate.MC_INT.cpu_2_tgr);

        //mon(newstate.KN_INT.temp_gt_pt);
        //mon(newstate.KN.any_intrpt_priority);
        //mon(newstate.MC_INT._clock_p1);
        //mon(newstate.MC_INT.clock_p0);
        //mon(newstate.MC_INT.cpu_2_lth);
        //mon(newstate.MC_INT.temp_test_for_inv_adr);
//        mon(newstate.MC_INT.any_select_lth_not_inhibited);
        //mon(newstate.MC_INT._intrpt_priority_gated);
        //mon(newstate.MC_INT.invalid_address_bus);
//        mon(newstate.MC_INT.bcu_clean_up_sel_successful);

//        mon(newstate.MC_INT.clock_p0);
//        mon(newstate.MC_INT._master_reset_1);
//        mon(newstate.MC_INT._reset_cpu_request_tgrs);
        //mon(newstate.KS_INT.scan_word_5);
        //mon(newstate.AP.pal_40M63_equals_0);
        //mon(newstate.KS_INT.saszPsajo);
        //mon(newstate.AP.pal_32M39_equals_0);
        //mon(newstate.KS_INT.stat_clock_p2);
        //mon(newstate.DR.temp_dr081_f_l.B4);
        //mon(newstate.AP.pal_40M63_equals_0);
        //mon(newstate.AP.pal_32M39_equals_0);
        //mon(newstate.KS_INT.staa_lth);
        //mon(newstate.AP.pal_64M67_0);
        //mon(newstate.AP.pal_7M31_equals_0);
        //mon(newstate.KS_INT.temp576);
        //mon(newstate.AS.sal_0M3_or_4M7_sum_not_zero);
        //mon(newstate.KZ.edit_set_staa);
        //mon(newstate.DR.temp_dr131_g_l.B1);
        //mon(newstate.KS_INT.sal_0M7_not_zero);
        //mon(newstate.KS_INT._stat_tgr_reset);
        //mon(newstate.KS_INT._reset_staa);
        //mon(newstate.KS_INT.temp577);
        //D_fprintf(lf, "   ");
        //mon(newstate.KS.staa);
        //mon(newstate.RW_INT.stat_h);
        //mon(newstate.RW_INT.stat_a);
        //mon(newstate.DN.test_under_mask);
        //mon(newstate.RW_INT.temp_rw341_3c_as);
        //mon(newstate.RW_INT.result_positive);
        //mon(newstate.DN.translate_P_edit);
        //mon(newstate.RW_INT.temp1158);
        //mon(newstate.KM.floating_point_be4);
        //mon(newstate.RW_INT._stat_a);
        //mon(newstate.RW_INT._temp_rw341_2f_ab);
//        mon(newstate.RW_INT._temp1157);
        //mon(newstate.KX._iMo_condition_code_10);
        //mon(newstate.RW_INT._set_cc_10_dec_compPadd_type);
        //mon(newstate.RW_INT._code_10_compare_log_si_P_ss);

        //mon(newstate.DN_INT.e4e5_eq.B3);
        //mon(newstate.RE.e_bit.B6);
        //mon(!newstate.DN_INT._temp_dn071_2d_ar_nand);
        //mon(newstate.DN.unnormalized_flt_pt_op);
        //mon(newstate.DN_INT.e0e2_eq.B0);
        //mon(newstate.DN_INT.e0e3_eq.B1);
        //mon(!newstate.DN._add_P_sub_logical);
        //mon(newstate.RW_INT.temp_add_P_sub_logical);

        //D_fprintf(lf, " CC10 ");
        //mon(newstate.RW_INT.condition_code_bit_10);
        //mon(!newstate.KX._iMo_condition_code_10);
//        mon(!newstate.RW_INT._set_cc_10_dec_compPadd_type);
//        mon(!newstate.RW_INT._code_10_compare_log_si_P_ss);
//        mon(!newstate.RW_INT._code_10_compPcomp_logPadd_log);
//        mon(!newstate.RW_INT._temp_rw341_6m_af);

//        D_fprintf(lf, " CC01 ");
//        mon(newstate.RW_INT.condition_code_bit_01);
//        mon(!newstate.RW_INT._temp_rw341_6m_af);
//        mon(!newstate.KX._iMo_condition_code_01);
//        mon(!newstate.RW_INT._set_cc_01_dec_compPadd_type);
//        mon(!newstate.RW_INT._set_01_on_andPorPexclusive_or);
//        mon(!newstate.RW_INT._cd01PtstOset_P_comp_log_siPss);
//        mon(!newstate.RW_INT._code_01_compPcomp_logPadd_log);
        //D_fprintf(lf, " LOG10 ");
        //mon(newstate.RW_INT.temp_add_P_sub_logical);
        //mon(newstate.RW_INT.stat_a);
        //mon(newstate.RW_INT.temp_ab_bit_31);
//        D_fprintf(lf, " LOG01 ");
//        mon(newstate.RW_INT.stat_a);
//        mon(newstate.RW_INT.temp_rw343_2k_bm);
//        mon(newstate.RW_INT.stat_h);
//        D_fprintf(lf, " STAH ");
//        mon(newstate.AS.stah);
//        D_fprintf(lf, " ");
//        mon(newstate.DR.temp_dr131_g_l.B13);
//        mon(newstate.AS_INT.clock_p2P140);
//        D_fprintf(lf, " ");
//        mon(newstate.AS_INT.clock_p2_e);
//        mon(newstate.AS_INT.saddl_carry_out_of_group_2_aw4);
//        mon(newstate.AS_INT.temp_as105_4e_az_not);
//        D_fprintf(lf, " ");
//        mon(newstate.AS_INT._temp172);
//        mon(newstate.AS_INT._temp173);
        //newstate.AS_INT._temp172 = (!(oldstate.DS.branch_on_atr_select_in && oldstate.AS.clock_p0) && !(oldstate.KC.machine_reset_gate_a_O_b || (oldstate.DR.lth_iMfetch_reset && oldstate.AS.clock_p0)));
        //newstate.AS_INT._temp173 = !((oldstate.AS_INT.temp_as105_4e_az_not && oldstate.AS.clock_p0) || (oldstate.AS_INT.clock_p2P140 && oldstate.DR.temp_dr131_g_l.B5));
//        D_fprintf(lf, " STAH from carry ");
//        mon(newstate.AS_INT.temp_as105_4e_az_not);
//        mon(newstate.AS_INT._clock_p3);
//        mon(newstate.AR.dec_cor_00M03_set_stats_a_e);
//        mon(!newstate.AR._save_sa_carry);
//        mon(newstate.AS_INT.clock_p2_e);

        //mon(newstate.RW_INT._temp_rw341_6m_af);
        //mon(newstate.DR.set_condition_code_10);
        //mon(newstate.RW_INT.temp_rw352_3g_ap);
        //mon(newstate.RW_INT.temp_rw352_3f_an);
        //mon(newstate.JA._wrd_ioce_logout);
        //mon(newstate.RW_INT.condition_code_bit_10);
        //mon(newstate.RW_INT.gate_st_to_psw_34_to_39);
        //mon(newstate.RW_INT._temp_rw352_2f_aj);
        //mon(newstate.RW_INT._temp_rw352_2g_am);
        //mon(newstate.RW_INT._gate_st_to_psw_34_to_39);
        //
        //D_fprintf(lf, "M1M1=%01x ", newstate.DP.m_bus.F);
//        D_fprintf(lf, "CC=%01x ", newstate.RW_INT.temp_cond_eq.F);
        //mon(newstate.DN.branch_on_condition_ar4);
        //mon(newstate.RW._condition_met_for_br_on_cond);
        //mon(!newstate.KD_INT._bocOcond_not_met);
        //mon(newstate.KD_INT.all_branch_instructions);
        //mon(newstate.KD_INT.ros_reg_decode_if_reset);
        //mon(newstate.DN._rr_Oe12M15_eq_0);
        //mon(newstate.KD_INT.ros_reg_decode_if_reset);
        //mon(newstate.DN.rx_or_rs_format);
        //mon(newstate.KD_INT.d_21M22_eq_01_P_10);
        //mon(newstate.KD_INT.execute_in_progress_tgr_ap4);


        //mon(!newstate.KD._turn_on_i_fetch_1_at_eop);
        //mon(!newstate.KD_INT._temp_kd101_4f_a5_nand);
        //mon(newstate.KD_INT.temp440);
        //mon(newstate.KD_INT.clock_p2);
        //D_fprintf(lf, "IF1: ");
        //mon(!newstate.KD_INT._i_fetch_1_tgr);
        //mon(newstate.KD_INT.temp_i_fetch_1_tgr);
        //mon(!newstate.KD_INT._i_fetch_1_lth);
        //mon(newstate.KD_INT.i_fetch_1_lth);
        //D_fprintf(lf, "IF2: ");
        //mon(!newstate.KD_INT._temp_i_fetch_2_tgr);
        //mon(newstate.KD_INT.temp_i_fetch_2_tgr);
        //mon(!newstate.KD_INT._i_fetch_2_lth);
        //mon(newstate.KD.i_fetch_2_lth);
        //D_fprintf(lf, "IF3: ");
        //mon(!newstate.KD_INT._temp_i_fetch_3_tgr);
        //mon(newstate.KD_INT.i_fetch_3_tgr);
        //mon(!newstate.KD_INT._i_fetch_3_lth);
        //mon(newstate.KD_INT.i_fetch_3_lth);
        //mon(newstate.KD._if_1_lthOif_3_lth_not_flt);
        //mon(newstate.KD_INT.i_fetch_storage_req_tgr);

        //D_fprintf(lf, "3B: ");
        //mon(newstate.KD_INT.allow_3_cycle_ic_req);
        //mon(newstate.KD_INT._temp_kd101_2c_ag_nor);
        //mon(newstate.KD_INT.ros_reg_decode_branch_eop);
        //mon(newstate.RR._pre_decode_branch_instns);
        //D_fprintf(lf, "3N: ");
        //mon(newstate.KD_INT.temp_kd101_3b_an_andnot);
        //mon(newstate.KD_INT._temp_kd101_2c_ag_nor);
        //mon(newstate.KD_INT.ros_reg_decode_normal_eop);
        //mon(newstate.KD_INT._inhibit_neop_if_req);
        //D_fprintf(lf, "3/3/4: ");
        //mon(!newstate.KD._storage_req_from_ic_3_cycles);
        //mon(!newstate.KD._ic_storage_req_3_cycles);
        //mon(!newstate.KD._storage_req_ic_4_cycles);
        //mon(!newstate.DR._ic_request_storage);
        //mon(!newstate.DR._ic_req_storage);
        //D_fprintf(lf, "D/I/S: ");
        //mon(!newstate.MC_INT._d_sync_lth);
        //mon(!newstate.MC_INT._ic_sync_lth);
        //mon(!newstate.MC_INT._scan_sync_lth);
        //D_fprintf(lf, "CS: ");
        //mon(newstate.MC_INT.cpu_request_sync_lth);
        //mon(newstate.MC._insert_key_tgr);
        //mon(newstate.MC_INT.clock_p2);
        //mon(newstate.MC_INT._clock_p1);
        //D_fprintf(lf, "2T: ");
        //mon(!newstate.MC_INT._cpu_2_tgr);
        //mon(newstate.MC_INT.cpu_2_tgr);
        //mon(newstate.MC_INT._clock_p1);
        //mon(newstate.MC_INT.clock_p0);
        //D_fprintf(lf, "2L: ");
        //mon(!newstate.MC_INT._temp894);
        //mon(newstate.MC_INT.cpu_2_lth);
        //mon(newstate.MC_INT.clock_p0);
        //D_fprintf(lf, "A: ");
        //mon(newstate.MC_INT._stop_clock_tgr_ar4);
        //mon(newstate.MC_INT._temp922);
        //D_fprintf(lf, "B: ");
        //mon(newstate.MC_INT.bcu_clean_up_sel_successful);
        //mon(newstate.MC._insert_key_tgr);
        //D_fprintf(lf, "C: ");
        //mon(newstate.MC_INT.temp923);

//        mon(!newstate.MC_INT._reset_cpu_request_tgrs);
        //mon(!newstate.MC_INT._stop_clock_tgr_ar4);
        //mon(newstate.MC_INT.stop_clock_tgr);
        //mon(!newstate.MC._stop_clock_tgr_aw4);
        //mon(newstate.KW._pass_pulse_tgr);
        //mon(newstate.KC._clock_gate);

//        mon(newstate.MC._pulse_invalid_address);
//        mon(newstate.KC.clock_b0);
//        mon(newstate.KM_INT._temp506);
//        mon(newstate.KM_INT.invalid_cpu_address_lth);
//
//        mon(newstate.KD._ifsr);
//        mon(newstate.KM_INT._prog_int_trggr_lth);
//        mon(newstate.KM_INT.invalid_cpu_address_lth);
//        mon(newstate.KM_INT._pit_code_xx0100);
//        mon(newstate.KM_INT._temp_topit);
//        mon(newstate.KC.clock_b0);
//        mon(newstate.KU._inhibit_free_running_osc);
//        mon(newstate.KM_INT.hold_for_set_reset_gate);
//        mon(newstate.KM_INT.set_pit_code_04_P_05_P_80);
//        mon(newstate.KM_INT.invalid_operand_address);
//        mon(!newstate.KM_INT._program_interrupt);
//        mon(!newstate.KM_INT._program_interrupt_code_1);
//        D_fprintf(lf, "%06x  ",newstate.MA.sab.F);
//        mon(newstate.MC_INT.se_1_decoded);
//        mon(newstate.MC_INT.select_se_1);
//        mon(newstate.MC_INT._inhibit_select_se_1);
//        mon(newstate.MC_INT.any_select_lth_not_inhibited);
//        mon(newstate.MC_INT.temp_test_for_inv_adr);
//        mon(newstate.MC_INT._any_sel_lth_not_inhibited);
//        mon(!newstate.MC_INT._temp_sel_unsucc);
//        mon(oldstate.MC_INT.invalid_address_bus);
//        mon(newstate.MC_INT.temp_inv_adr_cancel);
//        mon(newstate.MC_INT._intrpt_priority_gated);
//        mon(newstate.MC_INT.temp_cpu_cancel);
//        mon(!newstate.MC_INT._cancel);
//        mon(newstate.MC.cancel);

//        mon(newstate.CA.ic.B21);
//        mon(newstate.AP.paddl.B61);
//        mon(newstate.DR.gt_pal_40M63_to_ic);
//        mon(newstate.DR.set_ic.B21);
//        mon(newstate.DR.set_ic_21M22);
//        mon(newstate.CA._ic_to_branches.B21);
//        mon(newstate.CA._ic_to_branches.B22);
//        mon(newstate.DS_INT._normal_k_set_rosar_10_e);
//        mon(newstate.DS.temp_k.B12);
//        mon(newstate.DS_INT.ic21Oic22_eq_0Onot_rr);
//        
//        mon(newstate.KD_INT.i_fetch_storage_req_tgr);
//        mon(newstate.KD_INT._i_fetch_storage_req);
//        D_fprintf(lf, "  ");
//        mon(newstate.KD_INT.if1_tgr_O_not_if3);
//        mon(newstate.DR.temp_dr131_g_r.B22);
//        mon(newstate.KD_INT._inhibit_tnisf);
//        mon(newstate.KD_INT._clock_p1);
//        mon(newstate.KD_INT._temp470);
//        mon(newstate.KD_INT._temp_5j_bn_nand);
//        mon(newstate.KD_INT._i_fetch_storage_req);
//        mon(newstate.KD_INT._clock_p1);
//        mon(newstate.KD_INT.d_reg_store);
//        mon(newstate.KD_INT._ros_reg_decode_beop);
//        mon(newstate.KD_INT._ros_decode_neop);

        //D_fprintf(lf, "->Q: ");
        //mon(newstate.RQ_INT.gt_q);

//        D_fprintf(lf, "IC=%06x ", -1-newstate.CA._ic.F);
//          D_fprintf(lf, "IC=%06x ", newstate.CA.ic.F);

//        mon(newstate.KC_INT._temp_kc011_5n_ck_not);
//        mon(newstate.KC_INT._temp_inhibited_clock);
//        mon(newstate.KC_INT._cpu_clock_to_bcu);
//        mon(newstate.KC._clock_gate);
//        mon(newstate.RX_INT.p0M2_clock);
//        mon(newstate.RX._clock_p0M1);
//        mon(newstate.KK.sense_latch_strobe);
//        mon(newstate.KK._reset_sense_latches);
//        D_fprintf(lf, "CROS=%09llx.%016llx ", -1LL - newstate.EF._cros_bit.F2,-1LL-newstate.EF._cros_bit.F);
//        D_fprintf(lf, "%016llx ", newstate.RY._ros_data_lth_bit.F);
//
//        mon(newstate.DR_INT.temp_c.B13);

//mon(newstate.KM_INT.temp_brPnor_eop);
//mon(!newstate.KM_INT._program_interrupt_tgr_lth);
//        mon(newstate.KD.block_i_fetch_tgr);
//        mon(newstate.KD_INT._reset_stadOp1);
//        mon(newstate.KD_INT._temp_kd501_2b_ca_nand);
//        mon(newstate.KD._master_reset);
//        mon(newstate.DR.gt_pal_40M63_to_d00M23);
//        mon(newstate.KM.rosar_bit_10Od_reg_20_yiaei);
//        mon(newstate.RS_RT.gt_pswMd_17M20Pp);
//        mon(newstate.RS_RT._rst_d);

//        D_fprintf(lf, "D=%06x ", newstate.RD.d_bit.F);
//        mon(newstate.RX_INT.p0M2_clock);
//        mon(newstate.RX_INT._wait_state_block);
//        mon(newstate.RX_INT.clock_p2_rx003);
//        mon(newstate.RX_INT._temp_block_rosar_reset);
//        mon(newstate.RX_INT.p4_gate);
//        mon(newstate.RX_INT._inhibit_next_address);

//        mon(newstate.DS.normal_j_set_rosar_bit_10);
//        mon(newstate.DS.normal_k_set_rosar_bit_10);
//        mon(newstate.DS.temp_k.B14);
//        mon(newstate.DS.set_rosar_10_fast_k14);

//        mon(newstate.CA.ic.B21);
//        mon(newstate.CA_INT._temp226);
//        mon(newstate.CA_INT._temp227);
//        mon(newstate.AP.paddl.B61);
//        mon(newstate.DR.gt_pal_40M63_to_ic);
//        mon(newstate.DR.set_ic.B21);
//        mon(newstate.DR.set_ic_21M22);

//        mon(newstate.DS_INT._roslth_62M68_eq_j.B97);
//        mon(newstate.RE._e_bit.B6);
//        mon(newstate.DS_INT._roslth_62M68_eq_j.B103);
//        mon(newstate.CA._ic_to_branches.B21);

//        mon(newstate.DS.j97Oic21Pj103Oe06_ros_10);
//        mon(newstate.KM.rosar_bit_10Od_reg_20_yiaei);
//        mon(newstate.RX_INT._scanMmc_inh_int_cond);
//        mon(newstate.RX_INT.yiaei_end_op);
//        mon(newstate.RX_INT.temp1193);

//        mon(newstate.DS._eff_rr_format);                    // TRUE
//
//        mon(newstate.DS.normal_set_rosar_bit_9);
//        mon(newstate.DS.j102Pj103Oe05_to_rosar_9);
//        mon(newstate.DS.roslth_62M68_eq_j.B97);             // TRUE
//        mon(newstate.DS.effective_b_field_per_ic);
//        mon(newstate.DS.roslth_62M68_eq_j.B121);
//        mon(newstate.DS.effective_bMfield_per_d);
//        
//        mon(newstate.KM.rosar_bit_9Od_reg_19_yiaei);
//        mon(newstate.RX_INT._scanMmc_inh_int_cond);
//        mon(newstate.RX_INT.yiaei_end_op);
//        mon(newstate.RX_INT.temp1192);

//        D_fprintf(lf, "  LAL=%03x", newstate.LS_INT.mem_read_addr.F);
//        D_fprintf(lf, "  LS=%08x", newstate.LS.ls_busMM.F);

        //mon(!newstate.RS_RT._gt_st_32M63MMpadda_31M62_tMl1);

//        D_fprintf(lf, "PA=%016llx ", newstate.AP_INT.temp_padda.F);
//        D_fprintf(lf, "PB=%016llx ", newstate.AP_INT.temp_paddb.F);
        //D_fprintf(lf, "T=%016llx ", newstate.AP_INT.bit_transmit.F);
        //D_fprintf(lf, "C=%016llx ", newstate.AP_INT.bit_carry.F);
        //D_fprintf(lf, "HS=%016llx ", -1LL - newstate.AP_INT._half_sum.F);
        //D_fprintf(lf, "Cin=%016llx ", -1LL - newstate.AP._carry_into_bit.F);
//        D_fprintf(lf, "FS=%016llx ", newstate.AP_INT.sum.F);
        //D_fprintf(lf, "%016llx ", -1LL - newstate.AP_INT._latched_sum.F);

//        D_fprintf(lf, "PAL=%016llx ", newstate.AP.paddl.F);

//        mon(newstate.AP_INT.clock_p2_extended);
//        mon(newstate.AP_INT._clock_p3_extended);
//
//        mon(newstate.RE_INT.gt_e_8M11_to_lal_01M04);
//        mon(newstate.RE_INT.gt_e_12M15_to_lal_01M04);
//        mon(newstate.RQ_INT.gt_r_08M11_to_lal);
//        mon(newstate.RQ_INT.gt_r_12M15_to_lal);
//        
//        mon(newstate.RS_RT_INT.gt_lsMt);
//*/
//        /*
//        mon(!newstate.KM_INT._temp_flt_rr);
//        mon(!newstate.KM_INT._temp_flt_rx);
//        mon(!newstate.DN._fx_pt_rr_mult_P_divide_P_sco);
//        mon(!newstate.DN._shift_instructions);
//        mon(!newstate.KM_INT._temp_fx_pt_dvd);
//        D_fprintf(lf, "  ");
//        mon(newstate.RE.e_bit.B8);
//        mon(newstate.KM_INT.temp552);
//        D_fprintf(lf, "  ");
//
//        mon(newstate.RS_RT.st_bit_28P29P30P31_is_a_one);
//        mon(newstate.KM_INT.temp553);
//        D_fprintf(lf, "  ");
//
//        mon(newstate.RE.e_bit.B11);
//        mon(newstate.KM_INT.temp554);
//        D_fprintf(lf, "  ");
//
//        mon(newstate.RE.e_bit.B12);
//        mon(newstate.KM_INT.temp_flt_rr);
//        D_fprintf(lf, "  ");
//
//        mon(newstate.RE.e_bit.B15);
//        mon(newstate.KM_INT.temp_flt_rr);
//
//        D_fprintf(lf, "  ");
//        mon(newstate.KM_INT.enable_d_reg_specif_error);
//        D_fprintf(lf, "  ");
//
//        mon(newstate.RD.d_bit.B21);
//        mon(newstate.KM_INT.e_reg_decode_inst_a);
//        D_fprintf(lf, "  ");
//
//        mon(newstate.RD.d_bit.B22);
//        mon(newstate.KM_INT.e_reg_decode_inst_b);
//        D_fprintf(lf, "  ");
//
//        mon(newstate.RD.d_bit.B23);
//        mon(newstate.KM_INT.e_reg_decode_inst_c);
//        D_fprintf(lf, "  ");
//
//
//        mon(!newstate.KM_INT._specif_error_dPstPe_reg);
//        mon(!newstate.KM_INT._execute);
//        mon(!newstate.KM_INT._temp_wrd);
//        mon(!newstate.KM_INT._specif_error_pal);
//        mon(!newstate.KM_INT._temp_3b_ca);
//
//        mon(newstate.KM.block_rosar_O_set_bit_7_yiasi);
//        mon(newstate.RX_INT.yiasi);
//        */
//
//        /*
//        mon(newstate.RW_INT.temp_add_P_sub_logical);
//        mon(newstate.RW_INT.temp_ab_bit_31);
//        mon(newstate.RW_INT.stat_a);
//        mon(newstate.RW_INT.temp_ab_bit_31);
//        mon(newstate.RW_INT.temp_logical_compare_rrPrx);
//        mon(newstate.RW_INT._stat_a);
//        mon(newstate.RW_INT.temp_rw343_1g_ah);
//        mon(newstate.RW.fixed_point_compare);
//        mon(newstate.RW_INT._stat_a);
//        D_fprintf(lf, "  ");
//        mon(newstate.RW_INT._temp1164);
//        mon(newstate.KX._iMo_condition_code_10);
//        mon(newstate.RW_INT._set_cc_10_dec_compPadd_type);
//        mon(newstate.RW_INT._code_10_compare_log_si_P_ss);
//        mon(newstate.RW_INT._code_10_compPcomp_logPadd_log);
//        mon(newstate.RW_INT._temp_rw341_6m_af);
//        D_fprintf(lf, "  ");
//        mon(newstate.RW_INT.temp_rw352_3f_an);
//        mon(newstate.JA._wrd_ioce_logout);
//        mon(newstate.RW_INT.condition_code_bit_10);
//        D_fprintf(lf, "  ");
//mon(newstate.KD_INT.ros_reg_decode_branch_eop);
//mon(newstate.KD_INT.allow_3_cycle_ic_req);
//mon(newstate.KD_INT._temp_kd101_2c_ag_nor);
//mon(newstate.RR._pre_decode_branch_instns);
//
//D_fprintf(lf, "  ");
//
//mon(newstate.KD_INT.ros_reg_decode_normal_eop);
//mon(newstate.KD_INT.temp_kd101_3b_an_andnot);
//mon(newstate.KD_INT._temp_kd101_2c_ag_nor);
//mon(newstate.KD_INT._inhibit_neop_if_req);
//D_fprintf(lf, "  ");
//
//
//        mon(!newstate.KD._storage_req_from_ic_3_cycles);
//        mon(!newstate.KD._ic_storage_req_3_cycles);
//        mon(!newstate.KD._storage_req_ic_4_cycles);
//       
//
//        mon(newstate.DS_INT._normal_k_set_rosar_10_e);
//        mon(newstate.DS.roslth_57M61_eq.B12);
//        mon(newstate.DS_INT.temp398);
//        mon(newstate.DS.roslth_57M61_eq.B12);
//        mon(newstate.DS_INT.ic21Oic22_eq_0Onot_rr);  // 1
//        mon(newstate.DS.roslth_57M61_eq.B27);
//        mon(newstate.DN.move);

//        mon(newstate.KC._clock_gate);
//        mon(newstate.RQ_INT.gt_q);
//
//        mon(newstate.CA._ic_21M22_eq.B0);
//        mon(newstate.CA._ic_21M22_eq.B3);
//        mon(newstate.KD_INT.ic_21M22_eq_00);
//        mon(newstate.KD_INT.ic_21M22_eq_11);
//        mon(newstate.KD_INT.ic_21M22_eq_01P10);
//        mon(newstate.KD_INT.pre_decode_rx_format);
//        mon(newstate.KD_INT.e_12M15_eq_0);
//        mon(newstate.RE._e12M15_eq_0000);
//        mon(newstate.DN.rx_format);
//        mon(newstate.KD_INT.ic_21M22_eq_00P11);
//        mon(newstate.KD_INT.pre_decode_rr_format);
//        mon(newstate.KD_INT.ic_21M22_eq_10);
//        D_fprintf(lf, "  ");
//        mon(newstate.KD_INT.temp451);
//        mon(newstate.KD_INT.temp452);
//        mon(newstate.KD_INT.temp453);
//        mon(newstate.KD_INT.ros_reg_decode_if_reset);
////        mon(newstate.KD_INT.block_neop_ic_req);
////        mon(!newstate.KD_INT._inhibit_neop_if_req);
//
//        D_fprintf(lf, "  ");
//        mon(newstate.KD_INT._clock_p1);
//        mon(newstate.KD_INT.clock_p2);
//        mon(newstate.DR.temp_dr131_g_l.B15);
//        
//        D_fprintf(lf, "  ");
//        mon(newstate.KD._turn_on_i_fetch_1_at_eop);
//        mon(newstate.KM_INT.temp_toif);
        //mon(newstate.KD_INT._temp_kd101_2c_ag_nor);
        //mon(newstate.KD_INT.temp_kd101_3b_an_andnot);
        //mon(newstate.KD_INT.ros_decode_normal_eop);
        //mon(newstate.KD_INT._i_fetch_2_lth);
        //mon(newstate.KD_INT.ros_decode_branch_eop);

//        mon(newstate.KD._i_fetch_storage_req_in_prog);
        //mon(!newstate.KD._turn_on_i_fetch_1_at_eop);
//        mon(newstate.KM_INT._man_cntPtcsPpscPintrpt);
//        mon(newstate.KM_INT._invalid_inst_addr_priority);

        //mon(newstate.KM_INT.temp_i_f_forced_ad);
//        mon(newstate.KM_INT._temp_pre_decode_shift_inst);
//        mon(newstate.KM_INT._rosar_bit_7_for_i_fetch);
        //mon(newstate.KM.block_rosar_on_except_cond);
        //mon(newstate.RX_INT.yiaei_end_op);
        //mon(newstate.RX_INT._inhibit_next_address);
        //mon(newstate.RX_INT.p4_gate);
        //mon(newstate.RX_INT._clock_p3_blocked);
//        mon(newstate.KM._exceptional_cond_to_i_fetch);
//        mon(newstate.DR_INT._i_fetch_1_lth);
//        mon(newstate.DR.gt_pal_40M63_to_ic);
//        mon(newstate.CA_INT._rst_ic_for_pal);
//        mon(newstate.CA_INT.gt_pal_to_ic);
//        mon(newstate.CA_INT._rst_ic_21M22Ppar);
//        mon(newstate.CA_INT.set_ic_21M22Ppar);
//       
//        mon(newstate.RX._clock_p0M1);
//        mon(newstate.KK.sense_latch_strobe);
//        mon(newstate.KK._reset_sense_latches);
//        D_fprintf(lf, "  %09llx.", -1LL - newstate.EF._cros_bit.F2);
//        D_fprintf(lf, "%016llx  ", -1LL-newstate.EF._cros_bit.F);
//        mon(newstate.RX_INT._clock_p3_blocked);
//        mon(newstate.RX_INT.p4_gate);
//        mon(newstate.RX_INT._inhibit_next_address);
//        mon(newstate.DS.normal_set_rosar_bit_7);
//        mon(newstate.DS.j97Pj121Oeff_r01_to_rosar_7);
//        mon(newstate.DS.j103_to_rosar_6_7_10);
//        mon(newstate.RE.e_bit.B3);
//        mon(newstate.KM.rosar_bit_7Od_reg_17_yiaei);
//        mon(newstate.RX_INT._scanMmc_inh_int_cond);
//        mon(newstate.RX_INT.yiaei_end_op);
//        
//        mon(newstate.KD.i_fetch_2_lth);
//        mon(newstate.KD.rx_and_e12M15_eq_0_or_rs);

//        mon(newstate.KD._ic_storage_req_3_cycles);
//        D_fprintf(lf, "  ");
//        mon(newstate.MC.inh_osc_select_outstanding);
//        mon(newstate.MC._stop_clock_tgr_aw4);
//        */
//        /*
//        for (int i = 0; i < 4; i++) D_fprintf(lf, "%d", oldstate.RE.e_bit[8+i]);
//        D_fprintf(lf, " ");
//        for (int i = 0; i < 4; i++) D_fprintf(lf, "%d", oldstate.CE.incr_latch[8 + i]);
//        D_fprintf(lf, " ");
//        for (int i = 0; i < 4; i++) D_fprintf(lf, "%d", oldstate.RE.e_bit[12+i]);
//        D_fprintf(lf, " ");
//        for (int i = 0; i < 4; i++) D_fprintf(lf, "%d", oldstate.CE.incr_latch[12 + i]);
//
//        mon(oldstate.CE_INT.gate_e8M11P1_to_incr8M11);
//        mon(oldstate.RE.clock_p2_incr_reset);
//        mon(oldstate.RE_INT.gate_incr8M11_to_e8M11);
//        */
////        mon(oldstate.RE_INT._reset_e8M11_plus_parity);
//
//        /*
//        mon(newstate.CE_INT.gate_e12M15P1_to_incr12M15);
//        mon(newstate.RE._e_bit[15]);
//        mon(newstate.CE_INT.gate_e12M15M1_to_incr12M15);
//        mon(newstate.RE._e_bit[15]);
//        mon(newstate.CE_INT.gate_e12M15_to_incr12M15);
//        mon(newstate.RE.e_bit[15]);
//        mon(newstate.CE_INT.gate_cone_to_incr12M15);
//        mon(newstate.CE_INT.cone_13_and_15);
//        mon(newstate.CE_INT.gate_d18M21_to_incr12M15);
//        mon(newstate.RD.d_bit[21]);
//        mon(newstate.RE.clock_p2_incr_reset);
//        mon(newstate.CE_INT._incr_latch[15]);
//        mon(newstate.CE.incr_latch[15]);
//        D_fprintf(lf, "  ");
//
//        mon(newstate.RR.r[15]);
//        mon(newstate.RE_INT.gate_r8M15_to_e8M15);
//        mon(newstate.AP.paddl[63]);
//        mon(newstate.RE_INT.gate_pal56M63_to_e8M15);
//        mon(newstate.CE.incr_latch[15]);
//        mon(newstate.RE_INT.gate_incr12M15_to_e12M15);
//        mon(newstate.RE_INT._temp_gt_r8M15);
//        mon(newstate.RE_INT._temp_gt_pal56M63);
//        mon(newstate.RE_INT._temp_gt_incr12M15);
//        mon(newstate.RE._e_bit[15]);
//        mon(newstate.RE.e_bit[15]);
//        */
//
//     
//        /* MEMORY */
//        /*
//        D_fprintf(lf, "  ");
//        mon(newstate.MC_INT.d_sync_tgr);    // FIRES
//        mon(newstate.MC_INT.d_sync_lth);
//        mon(newstate.MC_INT.cpu_request_sync_lth);
//        mon(newstate.MC_INT.d_req_tgr);
//        mon(newstate.MC_INT.cpu_2_tgr);
//        mon(newstate.MC_INT.cpu_2_lth);
//        mon(newstate.MC_INT.cpu_3_tgr); // here
//        mon(newstate.MC_INT.cpu_3_lth);
//        mon(newstate.MC_INT.cpu_4_tgr);
//        mon(newstate.MC_INT.cpu_4_lth);
//        mon(newstate.MC_INT.cpu_5_tgr);
//        mon(newstate.MC.cpu_5_lth);
//        mon(newstate.MC_INT.stg_2_tgr);
//        mon(newstate.MC_INT.issue_a_select);
//        D_fprintf(lf, "  ");
//        mon(newstate.MC_INT._select_outstanding_mc741);
//        mon(newstate.MC_INT._temp_inh_stg_select);
//        D_fprintf(lf, "  ");
//        mon(newstate.MC_INT.select_timing_pulse_mc707);
//        mon(newstate.MC_INT._select_timing_pulse);
//        mon(newstate.MC.select_timing_pulse_mc401);
//
//        mon(newstate.MC_INT.select_pulse_se_1);
//        mon(newstate.MC_INT.delayed_invalid_address_bus);
//        mon(newstate.MC_INT.se_1_decoded);
//        mon(newstate.MC_INT.select_se_1);
//        D_fprintf(lf, "  ");
//
//        mon(newstate.MC.gate_d_to_sab);
//        mon(newstate.MC.gate_ic_to_bus);
//        */
///*
//        mon(newstate.DS_INT._normal_k_set_rosar_bit_10_a);
//        mon(newstate.DS_INT._normal_k_set_rosar_bit_10_b);
//        mon(newstate.DS_INT._normal_k_set_rosar_bit_10);
//        mon(newstate.DS_INT._normal_k_set_rosar_bit_10_d);
//        mon(newstate.DS_INT._normal_k_set_rosar_10_e);
//        mon(newstate.DS_INT._normal_k_set_rosar_10_f);
//        D_fprintf(lf, "  ");
//
//        mon(newstate.RX_INT.p4_gate);
//        mon(newstate.RX_INT._inhibit_next_address);
//        mon(newstate.DS.normal_j_set_rosar_bit_10);
//        mon(newstate.DS.normal_k_set_rosar_bit_10);
//        mon(newstate.DS.temp_k.B14);
//        mon(newstate.DS.set_rosar_10_fast_k14);
//        mon(newstate.DS.j97Oic21Pj103Oe06_ros_10);
//        mon(newstate.KM.rosar_bit_10Od_reg_20_yiaei);
//        mon(newstate.RX_INT._scanMmc_inh_int_cond);
//        mon(newstate.RX_INT.yiaei_end_op);
//        mon(newstate.RX_INT.temp1192);
//        mon(newstate.RX_INT._clock_p3_blocked);
//        D_fprintf(lf, "  %03x", -1-oldstate.RX._rosar.F);
//        */
        D_fprintf(lf, "ROSAR=%03x\n", oldstate.RX.rosar.F);
    }

    extern double runtime;

    void single_cycle() {
        static int startup = 0;
        runtime += 0.00000001; // 10 ns

        if (startup++ == 100)
            newstate.EXTERNAL_.power_on_reset = false;

        memcpy(&oldstate, &newstate, sizeof(oldstate));
        static int sixty = 0;

        sixty++;
        if (sixty == 833333) {
            sixty = 0;
            newstate.EXTERNAL_.P60_cycles_from_transformer = !oldstate.EXTERNAL_.P60_cycles_from_transformer;
        }
        else {
            newstate.EXTERNAL_.P60_cycles_from_transformer = oldstate.EXTERNAL_.P60_cycles_from_transformer;
        }
        process_ald();
        process_ald_clock();

        cycle_mon();

        memcpy(&oldstate, &newstate, sizeof(oldstate));
        process_ald();

        cycle_mon();
    }

#if defined(COMPARE_M65)
    std::map<int, unsigned char> w65, wh, sk65, skh;

    int io_herc = 0;
    int io_65 = 0;

    void record_65_io(int i) {
        io_65 = ((i & 7) << 24) | (i & 0xf00) | ((i >> 24) & 0xff);
    }
    void record_herc_io(int i) {
        // Compare IOCE transactions, rather than every I/O instruction.
        // KX decodes only IOCE 1 (channels 0..3). TCH of another IOCE
        // completes locally with CC=3, which cpu.c checks independently.
        // Do not suppress CC=3 within IOCE 1: an absent channel there
        // still generates an IOCE transaction that must be compared.
        if ((i & 0xff000000) == 0x07000000 && (i & 0x00000c00))
            return;
        io_herc = i;
    }

    void io_compare() {
        if (io_65 != io_herc) {
            D_fprintf(lf, "Execution gone astray: I/O operations do not match\n");
            D_fprintf(lf, "M65:\n");
            D_fprintf(lf, "  %08x\n", io_65);
            D_fprintf(lf, "Herc:\n");
            D_fprintf(lf, "  %08x\n", io_herc);
        }
        io_65 = 0;
        io_herc = 0;
    }

    void record_65_set_key(int sec, int sea) {
        sk65[sea & 0x7ffff0] = (sec >> 25) & 0x1f;
    }

    void record_65_write(int sea, int wh, int wl) {
        for (int i = 0; i < 4; i++) {
            if (sea & (1 << (31 - i)))
                w65[(sea & 0x7ffff8) + i] = (wh >> ((3 - i) * 8)) & 0xff;
            if (sea & (1 << (27 - i)))
                w65[(sea & 0x7ffff8) + 4 + i] = (wl >> ((3 - i) * 8)) & 0xff;           
        }
    }

    void record_herc_write(int addr, int len, char* data) {
        if (addr>=0)
            for (int i = 0; i < len; i++)
                wh[addr + i] = data[i];
    }
    void record_herc_write_char(int addr, char data) {
        if (addr >=0)
            wh[addr] = data;
    }
    void record_65_write_char(int addr, char data) {
        w65[addr] = data;
    }

    void record_herc_set_key(int addr, char data) {
        skh[addr] = data;
    }

    void write_compare() {
        if (w65 != wh) {
            D_fprintf(lf, "Execution gone astray: memory writes do not match\n");
            D_fprintf(lf, "M65:\n");
            for (auto& a : w65)
                D_fprintf(lf, "  %06x: %02x\n", a.first, a.second);
            D_fprintf(lf, "Herc:\n");
            for (auto& a : wh)
                D_fprintf(lf, "  %06x: %02x\n", a.first, a.second);
        }
        if (sk65 != skh) {
            D_fprintf(lf, "Execution gone astray: storage key writes do not match\n");
            D_fprintf(lf, "M65:\n");
            for (auto& a : sk65)
                D_fprintf(lf, "  %06x: %02x\n", a.first, a.second);
            D_fprintf(lf, "Herc:\n");
            for (auto& a : skh)
                D_fprintf(lf, "  %06x: %02x\n", a.first, a.second);
        }
        w65.clear();
        wh.clear();
        sk65.clear();
        skh.clear();
    }

#endif
}
