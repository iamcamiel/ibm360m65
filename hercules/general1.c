/* GENERAL1.C   (c) Copyright Roger Bowler, 1994-2009                */
/*              ESA/390 CPU Emulator                                 */
/*              Instructions A-M                                     */

/*              (c) Copyright Peter Kuschnerus, 1999-2009 (UPT & CFC)*/

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2009      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2009      */

// $Id: general1.c 5220 2009-03-01 09:46:32Z jj $

/*-------------------------------------------------------------------*/
/* This module implements all general instructions of the            */
/* S/370 and ESA/390 architectures, as described in the manuals      */
/* GA22-7000-03 System/370 Principles of Operation                   */
/* SA22-7201-06 ESA/390 Principles of Operation                      */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      Corrections to shift instructions by Jay Maynard, Jan Jaeger */
/*      Branch tracing by Jan Jaeger                                 */
/*      Instruction decode by macros - Jan Jaeger                    */
/*      Prevent TOD from going backwards in time - Jan Jaeger        */
/*      Fix STCKE instruction - Bernard van der Helm                 */
/*      Instruction decode rework - Jan Jaeger                       */
/*      Make STCK update the TOD clock - Jay Maynard                 */
/*      Fix address wraparound in MVO - Jan Jaeger                   */
/*      PLO instruction - Jan Jaeger                                 */
/*      Modifications for Interpretive Execution (SIE) by Jan Jaeger */
/*      Clear TEA on data exception - Peter Kuschnerus           v209*/
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.178  2009/02/07 22:54:06  ivan
// Define MIN() macro for MVCLE use in case it isn't defined
//
// Revision 1.177  2009/02/07 22:49:38  ivan
// Use concpy in MVCL to maintain Concurrent Block Update Consistency
// Use concpy in MVCLE to maintain Concurrent Block Update Consistency
// Enhance performances for MVCLE
//
// Revision 1.176  2009/02/04 18:09:39  ivan
// Temporary fixes for 'potentially uninitialized variables' by GCC.
// To me, it's bogus. Look for MVCL in general1.c & zmoncode in cpu.c
// and read FIXME.
//
// Revision 1.175  2009/02/04 12:22:37  ivan
// Fix issue with unaligned LM/STM when running on a host architecture that
// enforces alignment
//
// Revision 1.174  2009/01/31 21:37:50  rbowler
// Adopt general coding style - No functional changes
//
// Revision 1.173  2009/01/31 17:49:17  ivan
// Ensure proper nullification of MVCL if an access exception occurs during the 1st unit of operation
//
// Revision 1.172  2009/01/23 11:55:54  bernard
// copyright notice
//
// Revision 1.171  2009/01/11 20:28:33  ivan
// Remove dead code
//
// Revision 1.170  2008/12/24 07:50:16  ivan
// CSST Fix - Parm 2 alignement check error
//
// Revision 1.169  2008/12/23 13:44:57  ivan
// CSST Facility 2 fix
//
// Revision 1.168  2008/12/22 00:29:10  ivan
// Implement February 2008 z/Arch Compare And Swap And Store Facility 2
// Update FAQ to reflect change
// update FAQ to also indicate z/Arch DAT Enhancement is implemented
//
// Revision 1.167  2008/10/07 22:24:35  gsmith
// Fix zero ilc problem after branch trace
//
// Revision 1.166  2008/05/06 22:15:42  rbowler
// Fix warning: operation on `p1' may be undefined
//
// Revision 1.165  2008/04/11 14:28:44  bernard
// Integrate regs->exrl into base Hercules code.
//
// Revision 1.164  2008/04/09 12:01:35  rbowler
// Correct target instruction address for EXRL
//
// Revision 1.163  2008/04/09 07:38:09  bernard
// Allign to Rogers terminal ;-)
//
// Revision 1.162  2008/04/09 07:15:14  bernard
// Comment Roger
//
// Revision 1.161  2008/04/09 07:05:19  bernard
// EXRL has format C6x0, not C6. Thanks Roger!
//
// Revision 1.160  2008/04/09 05:38:57  bernard
// Instruction fetching error, now fetched with PSW_IA macro.
//
// Revision 1.159  2008/04/08 18:33:41  bernard
// Error in EXRL
//
// Revision 1.158  2008/04/08 17:13:26  bernard
// Added execute relative long instruction
//
// Revision 1.157  2008/03/05 00:34:44  ptl00
// Fix CFC operand size fetch
//
// Revision 1.156  2008/03/04 00:52:32  ptl00
// Fix BSM/BASSM mode switch trace
//
// Revision 1.155  2008/02/28 22:07:09  ptl00
// Fix mode switch trace
//
// Revision 1.154  2008/02/15 21:20:00  ptl00
// Fix EX to SS ops so that ILC is 4 on PER rupts
//
// Revision 1.153  2007/11/30 15:14:14  rbowler
// Permit String-Instruction facility to be activated in S/370 mode
//
// Revision 1.152  2007/08/07 19:47:59  ivan
// Fix a couple of gcc-4.2 warnings
//
// Revision 1.151  2007/06/23 00:04:10  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.150  2007/05/26 21:23:19  rbowler
// Eliminate uninitialised variable warnings in CSST
//
// Revision 1.149  2007/05/26 14:23:55  rbowler
// CSST instruction
//
// Revision 1.148  2007/01/13 07:21:11  bernard
// backout ccmask
//
// Revision 1.147  2007/01/12 15:23:41  bernard
// ccmaks phase 1
//
// Revision 1.146  2007/01/09 05:10:19  gsmith
// Tweaks to lm/stm
//
// Revision 1.145  2007/01/04 23:12:04  gsmith
// remove thunk calls for program_interrupt
//
// Revision 1.144  2006/12/31 21:16:32  gsmith
// 2006 Dec 31 really back out mainlockx.pat
//
// Revision 1.143  2006/12/20 09:09:40  jj
// Fix bogus log entries
//
// Revision 1.142  2006/12/20 04:26:19  gsmith
// 19 Dec 2006 ip_all.pat - performance patch - Greg Smith
//
// Revision 1.141  2006/12/20 04:22:00  gsmith
// 2006 Dec 19 Backout mainlockx.pat - possible SMP problems - Greg Smith
//
// Revision 1.140  2006/12/08 09:43:21  jj
// Add CVS message log
//

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_GENERAL1_C_)
#define _GENERAL1_C_
#endif

#if !defined(SOFTWARE_M65) && !defined(HARDWARE_M65)

#include "hercules.h"
#include "opcode.h"
#include "inline.h"
#include "clock.h"


/*-------------------------------------------------------------------*/
/* 1A   AR    - Add Register                                    [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(add_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR_(inst, regs, r1, r2);

    /* Add signed operands and set condition code */
    regs->psw.cc =
            add_signed (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    regs->GR_L(r2));

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && FOMASK(&regs->psw) )
        regs->program_interrupt (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);
}


/*-------------------------------------------------------------------*/
/* 5A   A     - Add                                             [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(add)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Add signed operands and set condition code */
    regs->psw.cc =
            add_signed (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    n);

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && FOMASK(&regs->psw) )
        regs->program_interrupt (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

}


/*-------------------------------------------------------------------*/
/* 4A   AH    - Add Halfword                                    [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(add_halfword)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
S32     n;                              /* 32-bit operand values     */

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Load 2 bytes from operand address */
    n = (S16)ARCH_DEP(vfetch2) ( effective_addr2, b2, regs );

    /* Add signed operands and set condition code */
    regs->psw.cc =
            add_signed (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    (U32)n);

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && FOMASK(&regs->psw) )
        regs->program_interrupt (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);

}


/*-------------------------------------------------------------------*/
/* 1E   ALR   - Add Logical Register                            [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(add_logical_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR0(inst, regs, r1, r2);

    /* Add signed operands and set condition code */
    regs->psw.cc =
            add_logical (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    regs->GR_L(r2));
}


/*-------------------------------------------------------------------*/
/* 5E   AL    - Add Logical                                     [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(add_logical)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Add signed operands and set condition code */
    regs->psw.cc =
            add_logical (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    n);
}


/*-------------------------------------------------------------------*/
/* 14   NR    - And Register                                    [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(and_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR0(inst, regs, r1, r2);

    /* AND second operand with first and set condition code */
    regs->psw.cc = ( regs->GR_L(r1) &= regs->GR_L(r2) ) ? 1 : 0;
}


/*-------------------------------------------------------------------*/
/* 54   N     - And                                             [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(and)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* AND second operand with first and set condition code */
    regs->psw.cc = ( regs->GR_L(r1) &= n ) ? 1 : 0;
}


/*-------------------------------------------------------------------*/
/* 94   NI    - And Immediate                                   [SI] */
/*-------------------------------------------------------------------*/
DEF_INST(and_immediate)
{
BYTE    i2;                             /* Immediate byte of opcode  */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
BYTE   *dest;                           /* Pointer to target byte    */

    SI(inst, regs, i2, b1, effective_addr1);

    /* Get byte mainstor address */
    dest = MADDR (effective_addr1, b1, regs, ACCTYPE_WRITE, regs->psw.pkey );

    /* AND byte with immediate operand, setting condition code */
    regs->psw.cc = ((*dest &= i2) != 0);

#if defined(COMPARE_M65)
    record_herc_write_char(effective_addr1, *dest);
#endif

    /* Update interval timer if necessary */
    ITIMER_UPDATE(effective_addr1,4-1,regs);
}


/*-------------------------------------------------------------------*/
/* D4   NC    - And Character                                   [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(and_character)
{
int     len, len2, len3;                /* Lengths to copy           */
int     b1, b2;                         /* Base register numbers     */
VADR    addr1, addr2;                   /* Virtual addresses         */
BYTE   *dest1, *dest2;                  /* Destination addresses     */
BYTE   *source1, *source2;              /* Source addresses          */
BYTE   *sk1, *sk2;                      /* Storage key addresses     */
int     i;                              /* Loop counter              */
int     cc = 0;                         /* Condition code            */

    SS_L(inst, regs, len, b1, addr1, b2, addr2);

    ITIMER_SYNC(addr2,len,regs);
    ITIMER_SYNC(addr1,len,regs);

    /* Quick out for 1 byte (no boundary crossed) */
    if (unlikely(len == 0))
    {
        source1 = MADDR (addr2, b2, regs, ACCTYPE_READ, regs->psw.pkey);
        dest1 = MADDR (addr1, b1, regs, ACCTYPE_WRITE, regs->psw.pkey);
        *dest1 &= *source1;
        regs->psw.cc = (*dest1 != 0);
        ITIMER_UPDATE(addr1,0,regs);
#if defined(COMPARE_M65)
        record_herc_write_char(addr1, *dest1);
#endif
        return;
    }

    /* There are several scenarios (in optimal order):
     * (1) dest boundary and source boundary not crossed
     * (2) dest boundary not crossed and source boundary crossed
     * (3) dest boundary crossed and source boundary not crossed
     * (4) dest boundary and source boundary are crossed
     *     (a) dest and source boundary cross at the same time
     *     (b) dest boundary crossed first
     *     (c) source boundary crossed first
     */

    /* Translate addresses of leftmost operand bytes */
    dest1 = MADDR (addr1, b1, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);
    sk1 = regs->dat.storkey;
    source1 = MADDR (addr2, b2, regs, ACCTYPE_READ, regs->psw.pkey);

    for (i = 0; i <= len; i++)
        if (*dest1++ &= *source1++) cc = 1;

    regs->psw.cc = cc;

#if defined(COMPARE_M65)
    dest1 = MADDR(addr1, b1, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);
    record_herc_write(addr1, len + 1, dest1);
#endif
}


/*-------------------------------------------------------------------*/
/* 05   BALR  - Branch and Link Register                        [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_and_link_register)
{
int     r1, r2;                         /* Values of R fields        */
VADR    newia;                          /* New instruction address   */

    RR_B(inst, regs, r1, r2);

    /* Compute the branch address from the R2 operand */
    newia = regs->GR(r2);

    /* Save the link information in the R1 operand */
    regs->GR_L(r1) =
        ( regs->psw.amode )
        ? (0x80000000                 | PSW_IA31(regs, 2))
        : (((!regs->execflag ? 2 : regs->exrl ? 6 : 4) << 29)
        |  (regs->psw.cc << 28)       | (regs->psw.progmask << 24)
        |  PSW_IA24(regs, 2));

    /* Execute the branch unless R2 specifies register 0 */
    if ( r2 != 0 )
        SUCCESSFUL_BRANCH(regs, newia, 2);
    else
        INST_UPDATE_PSW(regs, 2, 0);

} /* end DEF_INST(branch_and_link_register) */


/*-------------------------------------------------------------------*/
/* 45   BAL   - Branch and Link                                 [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_and_link)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX_B(inst, regs, r1, b2, effective_addr2);

    /* Save the link information in the R1 operand */
    regs->GR_L(r1) =
        ( regs->psw.amode )
          ? (0x80000000                 | PSW_IA31(regs, 4))
          : ((4 << 29)                  | (regs->psw.cc << 28)
          |  (regs->psw.progmask << 24) | PSW_IA24(regs, 4));

    SUCCESSFUL_BRANCH(regs, effective_addr2, 4);

} /* end DEF_INST(branch_and_link) */


/*-------------------------------------------------------------------*/
/* 07   BCR   - Branch on Condition Register                    [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_on_condition_register)
{
//int   r1, r2;                         /* Values of R fields        */

//  RR_(inst, regs, r1, r2);

    /* Branch if R1 mask bit is set and R2 is not register 0 */
    if ((inst[1] & 0x0F) != 0 && (inst[1] & (0x80 >> regs->psw.cc)))
        SUCCESSFUL_BRANCH(regs, regs->GR(inst[1] & 0x0F), 2);
    else
    {
        INST_UPDATE_PSW(regs, 2, 0);
        /* Perform serialization and checkpoint synchronization if
           the mask is all ones and the register is all zeroes */
        if ( inst[1] == 0xF0 )
        {
            PERFORM_SERIALIZATION (regs);
            PERFORM_CHKPT_SYNC (regs);
        }
    }

} /* end DEF_INST(branch_on_condition_register) */


/*-------------------------------------------------------------------*/
/* 47   BC    - Branch on Condition                             [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_on_condition)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    /* Branch to operand address if r1 mask bit is set */
    if ((0x80 >> regs->psw.cc) & inst[1])
    {
        RX_BC(inst, regs, b2, effective_addr2);
        SUCCESSFUL_BRANCH(regs, effective_addr2, 4);
    }
    else
        INST_UPDATE_PSW(regs, 4, 0);

} /* end DEF_INST(branch_on_condition) */


/*-------------------------------------------------------------------*/
/* 06   BCTR  - Branch on Count Register                        [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_on_count_register)
{
int     r1, r2;                         /* Values of R fields        */
VADR    newia;                          /* New instruction address   */

    RR_B(inst, regs, r1, r2);

    /* Compute the branch address from the R2 operand */
    newia = regs->GR(r2);

    /* Subtract 1 from the R1 operand and branch if result
           is non-zero and R2 operand is not register zero */
    if ( --(regs->GR_L(r1)) && r2 != 0 )
        SUCCESSFUL_BRANCH(regs, newia, 2);
    else
        INST_UPDATE_PSW(regs, 2, 0);

} /* end DEF_INST(branch_on_count_register) */


/*-------------------------------------------------------------------*/
/* 46   BCT   - Branch on Count                                 [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_on_count)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX_B(inst, regs, r1, b2, effective_addr2);

    /* Subtract 1 from the R1 operand and branch if non-zero */
    if ( --(regs->GR_L(r1)) )
        SUCCESSFUL_BRANCH(regs, effective_addr2, 4);
    else
        INST_UPDATE_PSW(regs, 4, 0);

} /* end DEF_INST(branch_on_count) */


/*-------------------------------------------------------------------*/
/* 86   BXH   - Branch on Index High                            [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_on_index_high)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
S32     i, j;                           /* Integer work areas        */

    RS_B(inst, regs, r1, r3, b2, effective_addr2);

    /* Load the increment value from the R3 register */
    i = (S32)regs->GR_L(r3);

    /* Load compare value from R3 (if R3 odd), or R3+1 (if even) */
    j = (r3 & 1) ? (S32)regs->GR_L(r3) : (S32)regs->GR_L(r3+1);

    /* Add the increment value to the R1 register */
    regs->GR_L(r1) = (S32)regs->GR_L(r1) + i;

    /* Branch if result compares high */
    if ( (S32)regs->GR_L(r1) > j )
        SUCCESSFUL_BRANCH(regs, effective_addr2, 4);
    else
        INST_UPDATE_PSW(regs, 4, 0);

} /* end DEF_INST(branch_on_index_high) */


/*-------------------------------------------------------------------*/
/* 87   BXLE  - Branch on Index Low or Equal                    [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(branch_on_index_low_or_equal)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
S32     i, j;                           /* Integer work areas        */

    RS_B(inst, regs, r1, r3, b2, effective_addr2);

    /* Load the increment value from the R3 register */
    i = regs->GR_L(r3);

    /* Load compare value from R3 (if R3 odd), or R3+1 (if even) */
    j = (r3 & 1) ? (S32)regs->GR_L(r3) : (S32)regs->GR_L(r3+1);

    /* Add the increment value to the R1 register */
    regs->GR_L(r1) = (S32)regs->GR_L(r1) + i;

    /* Branch if result compares low or equal */
    if ( (S32)regs->GR_L(r1) <= j )
        SUCCESSFUL_BRANCH(regs, effective_addr2, 4);
    else
        INST_UPDATE_PSW(regs, 4, 0);

} /* end DEF_INST(branch_on_index_low_or_equal) */


/*-------------------------------------------------------------------*/
/* 19   CR    - Compare Register                                [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR0(inst, regs, r1, r2);

    /* Compare signed operands and set condition code */
    regs->psw.cc =
                (S32)regs->GR_L(r1) < (S32)regs->GR_L(r2) ? 1 :
                (S32)regs->GR_L(r1) > (S32)regs->GR_L(r2) ? 2 : 0;
}


/*-------------------------------------------------------------------*/
/* 59   C     - Compare                                         [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(compare)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Compare signed operands and set condition code */
    regs->psw.cc =
            (S32)regs->GR_L(r1) < (S32)n ? 1 :
            (S32)regs->GR_L(r1) > (S32)n ? 2 : 0;
}


/*-------------------------------------------------------------------*/
/* BA   CS    - Compare and Swap                                [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_and_swap)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    addr2;                          /* effective address         */
BYTE   *main2;                          /* mainstor address          */
U32     old;                            /* old value                 */

    RS(inst, regs, r1, r3, b2, addr2);

    FW_CHECK(addr2, regs);

    ITIMER_SYNC(addr2,4-1,regs);

    /* Perform serialization before starting operation */
    PERFORM_SERIALIZATION (regs);

    /* Get mainstor address */
    main2 = MADDR (addr2, b2, regs, ACCTYPE_WRITE, regs->psw.pkey);

    old = CSWAP32(regs->GR_L(r1));

    /* Obtain main-storage access lock */
    OBTAIN_MAINLOCK(regs);

    /* Attempt to exchange the values */
    regs->psw.cc = cmpxchg4 (&old, CSWAP32(regs->GR_L(r3)), main2);

    /* Release main-storage access lock */
    RELEASE_MAINLOCK(regs);

    /* Perform serialization after completing operation */
    PERFORM_SERIALIZATION (regs);

    if (regs->psw.cc == 1)
    {
        PTT(PTT_CL_CSF,"*CS",regs->GR_L(r1),regs->GR_L(r3),(U32)(addr2 & 0xffffffff));
        regs->GR_L(r1) = CSWAP32(old);
            if (sysblk.cpus > 1)
                sched_yield();
    }
    else
    {
        ITIMER_UPDATE(addr2,4-1,regs);
#if defined(COMPARE_M65)
        record_herc_write(addr2, 4, (char*)main2);
#endif
    }
}

/*-------------------------------------------------------------------*/
/* 49   CH    - Compare Halfword                                [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_halfword)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
S32     n;                              /* 32-bit operand values     */

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Load rightmost 2 bytes of comparand from operand address */
    n = (S16)ARCH_DEP(vfetch2) ( effective_addr2, b2, regs );

    /* Compare signed operands and set condition code */
    regs->psw.cc =
            (S32)regs->GR_L(r1) < n ? 1 :
            (S32)regs->GR_L(r1) > n ? 2 : 0;
}


/*-------------------------------------------------------------------*/
/* 15   CLR   - Compare Logical Register                        [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_logical_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR0(inst, regs, r1, r2);

    /* Compare unsigned operands and set condition code */
    regs->psw.cc = regs->GR_L(r1) < regs->GR_L(r2) ? 1 :
                   regs->GR_L(r1) > regs->GR_L(r2) ? 2 : 0;
}


/*-------------------------------------------------------------------*/
/* 55   CL    - Compare Logical                                 [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_logical)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Compare unsigned operands and set condition code */
    regs->psw.cc = regs->GR_L(r1) < n ? 1 :
                   regs->GR_L(r1) > n ? 2 : 0;
}


/*-------------------------------------------------------------------*/
/* 95   CLI   - Compare Logical Immediate                       [SI] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_logical_immediate)
{
BYTE    i2;                             /* Immediate byte            */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
BYTE    cbyte;                          /* Compare byte              */

    SI(inst, regs, i2, b1, effective_addr1);

    /* Fetch byte from operand address */
    cbyte = ARCH_DEP(vfetchb) ( effective_addr1, b1, regs );

    /* Compare with immediate operand and set condition code */
    regs->psw.cc = (cbyte < i2) ? 1 :
                   (cbyte > i2) ? 2 : 0;
}


/*-------------------------------------------------------------------*/
/* D5   CLC   - Compare Logical Character                       [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(compare_logical_character)
{
unsigned int len, len1, len2;           /* Lengths                   */
int      rc;                            /* memcmp() return code      */
int      b1, b2;                        /* Base registers            */
VADR     ea1, ea2;                      /* Effective addresses       */
BYTE    *m1, *m2;                       /* Mainstor addresses        */

    SS_L(inst, regs, len, b1, ea1, b2, ea2);

    ITIMER_SYNC(ea1,len,regs);
    ITIMER_SYNC(ea2,len,regs);

    /* Translate addresses of leftmost operand bytes */
    m1 = MADDR (ea1, b1, regs, ACCTYPE_READ, regs->psw.pkey);
    m2 = MADDR (ea2, b2, regs, ACCTYPE_READ, regs->psw.pkey);

    /* Quick out if comparing just 1 byte */
    if (unlikely(len == 0))
    {
        rc = *m1 - *m2;
        regs->psw.cc = ( rc == 0 ? 0 : ( rc < 0 ? 1 : 2 ) );
        return;
    }

    /* There are several scenarios (in optimal order):
     * (1) dest boundary and source boundary not crossed
     *     (a) halfword compare
     *     (b) fullword compare
     *     (c) doubleword compare (64-bit machines)
     *     (d) other
     * (2) dest boundary not crossed and source boundary crossed
     * (3) dest boundary crossed and source boundary not crossed
     * (4) dest boundary and source boundary are crossed
     *     (a) dest and source boundary cross at the same time
     *     (b) dest boundary crossed first
     *     (c) source boundary crossed first
     */

    if ( (ea1 & 0x7FF) <= 0x7FF - len )
    {
        if ( (ea2 & 0x7FF) <= 0x7FF - len )
        {
            /* (1) - No boundaries are crossed */
            switch(len) {

            case 1:
                /* (1a) - halfword compare */
                {
                    U16 v1, v2;
                    v1 = fetch_hw(m1);
                    v2 = fetch_hw(m2);
                    regs->psw.cc = ( v1 == v2 ? 0 : ( v1 < v2 ? 1 : 2 ) );
                    return;
                }
                break;

            case 3:
                /* (1b) - fullword compare */
                {
                    U32 v1, v2;
                    v1 = fetch_fw(m1);
                    v2 = fetch_fw(m2);
                    regs->psw.cc = ( v1 == v2 ? 0 : ( v1 < v2 ? 1 : 2 ) );
                    return;
                }
                break;

            case 7:
                /* (1c) - doubleword compare (64-bit machines) */
                if (sizeof(unsigned int) >= 8)
                {
                    U64 v1, v2;
                    v1 = fetch_dw(m1);
                    v2 = fetch_dw(m2);
                    regs->psw.cc = ( v1 == v2 ? 0 : ( v1 < v2 ? 1 : 2 ) );
                    return;
                }

            default:
                /* (1d) - other compare */
                rc = memcmp(m1, m2, len + 1);
                break;
            }
        }
        else
        {
            /* (2) - Second operand crosses a boundary */
            len2 = 0x800 - (ea2 & 0x7FF);
            rc = memcmp(m1, m2, len2);
            if (rc == 0)
            {
                m2 = MADDR ((ea2 + len2) & ADDRESS_MAXWRAP(regs),
                            b2, regs, ACCTYPE_READ, regs->psw.pkey);
                rc = memcmp(m1 + len2, m2, len - len2 + 1);
             }
        }
    }
    else
    {
        /* First operand crosses a boundary */
        len1 = 0x800 - (ea1 & 0x7FF);
        if ( (ea2 & 0x7FF) <= 0x7FF - len )
        {
            /* (3) - First operand crosses a boundary */
            rc = memcmp(m1, m2, len1);
            if (rc == 0)
            {
                m1 = MADDR ((ea1 + len1) & ADDRESS_MAXWRAP(regs),
                            b1, regs, ACCTYPE_READ, regs->psw.pkey);
                rc = memcmp(m1, m2 + len1, len - len1 + 1);
             }
        }
        else
        {
            /* (4) - Both operands cross a boundary */
            len2 = 0x800 - (ea2 & 0x7FF);
            if (len1 == len2)
            {
                /* (4a) - Both operands cross at the same time */
                rc = memcmp(m1, m2, len1);
                if (rc == 0)
                {
                    m1 = MADDR ((ea1 + len1) & ADDRESS_MAXWRAP(regs),
                                b1, regs, ACCTYPE_READ, regs->psw.pkey);
                    m2 = MADDR ((ea2 + len1) & ADDRESS_MAXWRAP(regs),
                                b2, regs, ACCTYPE_READ, regs->psw.pkey);
                    rc = memcmp(m1, m2, len - len1 +1);
                }
            }
            else if (len1 < len2)
            {
                /* (4b) - First operand crosses first */
                rc = memcmp(m1, m2, len1);
                if (rc == 0)
                {
                    m1 = MADDR ((ea1 + len1) & ADDRESS_MAXWRAP(regs),
                                b1, regs, ACCTYPE_READ, regs->psw.pkey);
                    rc = memcmp (m1, m2 + len1, len2 - len1);
                }
                if (rc == 0)
                {
                    m2 = MADDR ((ea2 + len2) & ADDRESS_MAXWRAP(regs),
                                b2, regs, ACCTYPE_READ, regs->psw.pkey);
                    rc = memcmp (m1 + len2 - len1, m2, len - len2 + 1);
                }
            }
            else
            {
                /* (4c) - Second operand crosses first */
                rc = memcmp(m1, m2, len2);
                if (rc == 0)
                {
                    m2 = MADDR ((ea2 + len2) & ADDRESS_MAXWRAP(regs),
                                b2, regs, ACCTYPE_READ, regs->psw.pkey);
                    rc = memcmp (m1 + len2, m2, len1 - len2);
                }
                if (rc == 0)
                {
                    m1 = MADDR ((ea1 + len1) & ADDRESS_MAXWRAP(regs),
                                b1, regs, ACCTYPE_READ, regs->psw.pkey);
                    rc = memcmp (m1, m2 + len1 - len2, len - len1 + 1);
                }
            }
        }
    }
    regs->psw.cc = ( rc == 0 ? 0 : ( rc < 0 ? 1 : 2 ) );
}


/*-------------------------------------------------------------------*/
/* 4F   CVB   - Convert to Binary                               [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_to_binary)
{
U64     dreg;                           /* 64-bit result accumulator */
int     r1;                             /* Value of R1 field         */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
int     ovf;                            /* 1=overflow                */
int     dxf;                            /* 1=data exception          */
BYTE    dec[8];                         /* Packed decimal operand    */

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Fetch 8-byte packed decimal operand */
    ARCH_DEP(vfetchc) (dec, 8-1, effective_addr2, b2, regs);

    /* Convert 8-byte packed decimal to 64-bit signed binary */
    packed_to_binary (dec, 8-1, &dreg, &ovf, &dxf);

    /* Data exception if invalid digits or sign */
    if (dxf)
    {
        regs->dxc = DXC_DECIMAL;
        regs->program_interrupt (regs, PGM_DATA_EXCEPTION);
    }

    /* Overflow if result exceeds 31 bits plus sign */
    if ((S64)dreg < -2147483648LL || (S64)dreg > 2147483647LL)
       ovf = 1;

    /* Store low-order 32 bits of result into R1 register */
    regs->GR_L(r1) = dreg & 0xFFFFFFFF;

    /* Program check if overflow (R1 contains rightmost 32 bits) */
    if (ovf)
        regs->program_interrupt (regs, PGM_FIXED_POINT_DIVIDE_EXCEPTION);

} /* end DEF_INST(convert_to_binary) */


/*-------------------------------------------------------------------*/
/* 4E   CVD   - Convert to Decimal                              [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(convert_to_decimal)
{
S64     bin;                            /* 64-bit signed binary value*/
int     r1;                             /* Value of R1 field         */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
BYTE    dec[16];                        /* Packed decimal result     */

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Load value of register and sign-extend to 64 bits */
    bin = (S64)((S32)(regs->GR_L(r1)));

    /* Convert to 16-byte packed decimal number */
    binary_to_packed (bin, dec);

    /* Store low 8 bytes of result at operand address */
    ARCH_DEP(vstorec) ( dec+8, 8-1, effective_addr2, b2, regs );

} /* end DEF_INST(convert_to_decimal) */


/*-------------------------------------------------------------------*/
/* 1D   DR    - Divide Register                                 [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(divide_register)
{
int     r1;                             /* Values of R fields        */
int     r2;                             /* Values of R fields        */
int     divide_overflow;                /* 1=divide overflow         */

    RR_(inst, regs, r1, r2);

    ODD_CHECK(r1, regs);

    /* Divide r1::r1+1 by r2, remainder in r1, quotient in r1+1 */
    divide_overflow =
        div_signed (&(regs->GR_L(r1)),&(regs->GR_L(r1+1)),
                    regs->GR_L(r1),
                    regs->GR_L(r1+1),
                    regs->GR_L(r2));

    /* Program check if overflow */
    if ( divide_overflow )
        regs->program_interrupt (regs, PGM_FIXED_POINT_DIVIDE_EXCEPTION);
}


/*-------------------------------------------------------------------*/
/* 5D   D     - Divide                                          [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(divide)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */
int     divide_overflow;                /* 1=divide overflow         */

    RX_(inst, regs, r1, b2, effective_addr2);

    ODD_CHECK(r1, regs);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Divide r1::r1+1 by n, remainder in r1, quotient in r1+1 */
    divide_overflow =
        div_signed (&(regs->GR_L(r1)), &(regs->GR_L(r1+1)),
                    regs->GR_L(r1),
                    regs->GR_L(r1+1),
                    n);

    /* Program check if overflow */
    if ( divide_overflow )
        regs->program_interrupt (regs, PGM_FIXED_POINT_DIVIDE_EXCEPTION);

}


/*-------------------------------------------------------------------*/
/* 17   XR    - Exclusive Or Register                           [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(exclusive_or_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR0(inst, regs, r1, r2);

    /* XOR second operand with first and set condition code */
    regs->psw.cc = ( regs->GR_L(r1) ^= regs->GR_L(r2) ) ? 1 : 0;
}


/*-------------------------------------------------------------------*/
/* 57   X     - Exclusive Or                                    [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(exclusive_or)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* XOR second operand with first and set condition code */
    regs->psw.cc = ( regs->GR_L(r1) ^= n ) ? 1 : 0;
}


/*-------------------------------------------------------------------*/
/* 97   XI    - Exclusive Or Immediate                          [SI] */
/*-------------------------------------------------------------------*/
DEF_INST(exclusive_or_immediate)
{
BYTE    i2;                             /* Immediate operand         */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
BYTE   *dest;                           /* Pointer to target byte    */

    SI(inst, regs, i2, b1, effective_addr1);

    ITIMER_SYNC(effective_addr1,1,regs);

    /* Get byte mainstor address */
    dest = MADDR (effective_addr1, b1, regs, ACCTYPE_WRITE, regs->psw.pkey );

    /* XOR byte with immediate operand, setting condition code */
    regs->psw.cc = ((*dest ^= i2) != 0);

#if defined(COMPARE_M65)
    record_herc_write_char(effective_addr1, *dest);
#endif
    ITIMER_UPDATE(effective_addr1,0,regs);
}


/*-------------------------------------------------------------------*/
/* D7   XC    - Exclusive Or Character                          [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(exclusive_or_character)
{
int     len, len2, len3;                /* Lengths to copy           */
int     b1, b2;                         /* Base register numbers     */
VADR    addr1, addr2;                   /* Virtual addresses         */
BYTE   *dest1, *dest2;                  /* Destination addresses     */
BYTE   *source1, *source2;              /* Source addresses          */
BYTE   *sk1, *sk2;                      /* Storage key addresses     */
int     i;                              /* Loop counter              */
int     cc = 0;                         /* Condition code            */

    SS_L(inst, regs, len, b1, addr1, b2, addr2);

    ITIMER_SYNC(addr1,len,regs);
    ITIMER_SYNC(addr2,len,regs);

    /* Translate addresses of leftmost operand bytes */
    dest1 = MADDR (addr1, b1, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);
    sk1 = regs->dat.storkey;
    source1 = MADDR (addr2, b2, regs, ACCTYPE_READ, regs->psw.pkey);

    if (dest1 == source1)
    {
        /* (1a) - Dest and source are the same */
        memset(dest1, 0, len + 1);
    }
    else
    {
        /* (1b) - Dest and source are not the sam */
        for (i = 0; i <= len; i++)
            if (*dest1++ ^= *source1++) cc = 1;
    }
    regs->psw.cc = cc;

    ITIMER_UPDATE(addr1,len,regs);

#if defined(COMPARE_M65)
    dest1 = MADDR(addr1, b1, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);
    record_herc_write(addr1, len + 1, dest1);
#endif
}


/*-------------------------------------------------------------------*/
/* 44   EX    - Execute                                         [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(execute)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
BYTE   *ip;                             /* -> executed instruction   */

    RX_(inst, regs, r1, b2, regs->ET);

    ODD_CHECK(regs->ET, regs);

    /* Fetch target instruction from operand address */
    ip = INSTRUCTION_FETCH(regs, 1);
    if (ip != regs->exinst)
        memcpy (regs->exinst, ip, 8);

    /* Program check if recursive execute */
    if ( regs->exinst[0] == 0x44
                                                                   )
        regs->program_interrupt (regs, PGM_EXECUTE_EXCEPTION);

    /* Or 2nd byte of instruction with low-order byte of R1 */
    regs->exinst[1] |= r1 ? regs->GR_LHLCL(r1) : 0;

    /*
     * Turn execflag on indicating this instruction is EXecuted.
     * psw.ip is backed up by the EXecuted instruction length to
     * be incremented back by the instruction decoder.
     */
    regs->execflag = 1;
    regs->exrl = 0;
    regs->ip -= ILC(regs->exinst[0]);

    EXECUTE_INSTRUCTION (regs->exinst, regs);

    /* Leave execflag on if pending PER so ILC will reflect EX */
    if (!OPEN_IC_PER(regs))
        regs->execflag = 0;
}


/*-------------------------------------------------------------------*/
/* 43   IC    - Insert Character                                [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(insert_character)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Insert character in r1 register */
    regs->GR_LHLCL(r1) = ARCH_DEP(vfetchb) ( effective_addr2, b2, regs );

}


/*-------------------------------------------------------------------*/
/* 58   L     - Load                                            [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(load)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
#if 0
U32    *p;                              /* Mainstor pointer          */
#endif

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Load R1 register from second operand */

    regs->GR_L(r1) = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

} /* end DEF_INST(load) */


/*-------------------------------------------------------------------*/
/* 18   LR    - Load Register                                   [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(load_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR0(inst, regs, r1, r2);

    /* Copy second operand to first operand */
    regs->GR_L(r1) = regs->GR_L(r2);
}


/*-------------------------------------------------------------------*/
/* 41   LA    - Load Address                                    [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(load_address)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX0(inst, regs, r1, b2, effective_addr2);

    /* Load operand address into register */
    SET_GR_A(r1, regs, effective_addr2);
}


/*-------------------------------------------------------------------*/
/* 12   LTR   - Load and Test Register                          [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(load_and_test_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR0(inst, regs, r1, r2);

    /* Copy second operand and set condition code */
    regs->GR_L(r1) = regs->GR_L(r2);

    regs->psw.cc = (S32)regs->GR_L(r1) < 0 ? 1 :
                   (S32)regs->GR_L(r1) > 0 ? 2 : 0;
}


/*-------------------------------------------------------------------*/
/* 13   LCR   - Load Complement Register                        [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(load_complement_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR_(inst, regs, r1, r2);

    /* Condition code 3 and program check if overflow */
    if ( regs->GR_L(r2) == 0x80000000 )
    {
        regs->GR_L(r1) = regs->GR_L(r2);
        regs->psw.cc = 3;
        if ( FOMASK(&regs->psw) )
            regs->program_interrupt (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);
        return;
    }

    /* Load complement of second operand and set condition code */
    regs->GR_L(r1) = -((S32)regs->GR_L(r2));

    regs->psw.cc = (S32)regs->GR_L(r1) < 0 ? 1 :
                   (S32)regs->GR_L(r1) > 0 ? 2 : 0;
}


/*-------------------------------------------------------------------*/
/* 48   LH    - Load Halfword                                   [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(load_halfword)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Load rightmost 2 bytes of register from operand address */
    regs->GR_L(r1) = (S16)ARCH_DEP(vfetch2) ( effective_addr2, b2, regs );
}


/*-------------------------------------------------------------------*/
/* 98   LM    - Load Multiple                                   [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(load_multiple)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
int     i, m, n;                        /* Integer work areas        */
U32    *p1, *p2;                        /* Mainstor pointers         */
BYTE   *bp1;                            /* Unaligned maintstor ptr   */

    RS(inst, regs, r1, r3, b2, effective_addr2);

    /* Calculate number of bytes to load */
    n = (((r3 - r1) & 0xF) + 1) << 2;

    /* Calculate number of bytes to next boundary */
    m = 0x800 - ((VADR_L)effective_addr2 & 0x7ff);

    /* Address of operand beginning */
    bp1 = (BYTE*)MADDR(effective_addr2, b2, regs, ACCTYPE_READ, regs->psw.pkey);
    p1=(U32*)bp1;

    if (likely(n <= m))
    {
        /* Boundary not crossed */
        n >>= 2;
#if defined(OPTION_STRICT_ALIGNMENT)
        if(likely(!(((uintptr_t)effective_addr2)&0x03)))
        {
#endif
            for (i = 0; i < n; i++, p1++)
                regs->GR_L((r1 + i) & 0xF) = fetch_fw (p1);
#if defined(OPTION_STRICT_ALIGNMENT)
        }
        else
        {
            for (i = 0; i < n; i++, bp1+=4)
                regs->GR_L((r1 + i) & 0xF) = fetch_fw (bp1);
        }
#endif
    }
    else
    {
        /* Boundary crossed, get 2nd page address */
        effective_addr2 += m;
        effective_addr2 &= ADDRESS_MAXWRAP(regs);
        p2 = (U32*)MADDR(effective_addr2, b2, regs, ACCTYPE_READ, regs->psw.pkey);

        if (likely((m & 0x3) == 0))
        {
            /* Addresses are word aligned */
            m >>= 2;
            for (i = 0; i < m; i++, p1++)
                regs->GR_L((r1 + i) & 0xF) = fetch_fw (p1);
            n >>= 2;
            for ( ; i < n; i++, p2++)
                regs->GR_L((r1 + i) & 0xF) = fetch_fw (p2);
        }
        else
        {
            /* Worst case */
            U32 rwork[16];
            BYTE *b1, *b2;

            b1 = (BYTE *)&rwork[0];
            b2 = (BYTE *)p1;
            for (i = 0; i < m; i++)
                *b1++ = *b2++;
            b2 = (BYTE *)p2;
            for ( ; i < n; i++)
                *b1++ = *b2++;

            n >>= 2;
            for (i = 0; i < n; i++)
                regs->GR_L((r1 + i) & 0xF) = CSWAP32(rwork[i]);
        }
    }

} /* end DEF_INST(load_multiple) */


/*-------------------------------------------------------------------*/
/* 11   LNR   - Load Negative Register                          [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(load_negative_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR0(inst, regs, r1, r2);

    /* Load negative value of second operand and set cc */
    regs->GR_L(r1) = (S32)regs->GR_L(r2) > 0 ?
                            -((S32)regs->GR_L(r2)) :
                            (S32)regs->GR_L(r2);

    regs->psw.cc = (S32)regs->GR_L(r1) == 0 ? 0 : 1;
}


/*-------------------------------------------------------------------*/
/* 10   LPR   - Load Positive Register                          [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(load_positive_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR_(inst, regs, r1, r2);

    /* Condition code 3 and program check if overflow */
    if ( regs->GR_L(r2) == 0x80000000 )
    {
        regs->GR_L(r1) = regs->GR_L(r2);
        regs->psw.cc = 3;
        if ( FOMASK(&regs->psw) )
            regs->program_interrupt (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);
        return;
    }

    /* Load positive value of second operand and set cc */
    regs->GR_L(r1) = (S32)regs->GR_L(r2) < 0 ?
                            -((S32)regs->GR_L(r2)) :
                            (S32)regs->GR_L(r2);

    regs->psw.cc = (S32)regs->GR_L(r1) == 0 ? 0 : 2;
}


/*-------------------------------------------------------------------*/
/* AF   MC    - Monitor Call                                    [SI] */
/*-------------------------------------------------------------------*/
DEF_INST(monitor_call)
{
BYTE    i2;                             /* Monitor class             */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
CREG    n;                              /* Work                      */

    SI(inst, regs, i2, b1, effective_addr1);

    /* Program check if monitor class exceeds 15 */
    if ( i2 > 0x0F )
        regs->program_interrupt (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Ignore if monitor mask in control register 8 is zero */
    n = (regs->CR(8) & CR8_MCMASK) << i2;
    if ((n & 0x00008000) == 0)
        return;

    regs->monclass = i2;
    regs->MONCODE = effective_addr1;

    /* Generate a monitor event program interruption */
    regs->program_interrupt (regs, PGM_MONITOR_EVENT);

}


/*-------------------------------------------------------------------*/
/* 92   MVI   - Move Immediate                                  [SI] */
/*-------------------------------------------------------------------*/
DEF_INST(move_immediate)
{
BYTE    i2;                             /* Immediate operand         */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */

    SI(inst, regs, i2, b1, effective_addr1);

    /* Store immediate operand at operand address */
    ARCH_DEP(vstoreb) ( i2, effective_addr1, b1, regs );
}


/*-------------------------------------------------------------------*/
/* D2   MVC   - Move Character                                  [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(move_character)
{
BYTE    l;                              /* Length byte               */
int     b1, b2;                         /* Values of base fields     */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */

    SS_L(inst, regs, l, b1, effective_addr1,
                                  b2, effective_addr2);

    /* Move characters using current addressing mode and key */
    ARCH_DEP(move_chars) (effective_addr1, b1, regs->psw.pkey,
                effective_addr2, b2, regs->psw.pkey, l, regs);
}


/*-------------------------------------------------------------------*/
/* E8   MVCIN - Move Inverse                                    [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(move_inverse)
{
BYTE    l;                              /* Lenght byte               */
int     b1, b2;                         /* Base registers            */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */
VADR    n;                              /* 32-bit operand values     */
BYTE    tbyte;                          /* Byte work areas           */
int     i;                              /* Integer work areas        */

    SS_L(inst, regs, l, b1, effective_addr1,
                                  b2, effective_addr2);

    /* If operand 1 crosses a page, make sure both pages are accessable */
    if((effective_addr1 & PAGEFRAME_PAGEMASK) !=
        ((effective_addr1 + l) & PAGEFRAME_PAGEMASK))
        ARCH_DEP(validate_operand) (effective_addr1, b1, l, ACCTYPE_WRITE_SKP, regs);

    /* If operand 2 crosses a page, make sure both pages are accessable */
    n = (effective_addr2 - l) & ADDRESS_MAXWRAP(regs);
    if((n & PAGEFRAME_PAGEMASK) !=
        ((n + l) & PAGEFRAME_PAGEMASK))
        ARCH_DEP(validate_operand) (n, b2, l, ACCTYPE_READ, regs);

    /* Process the destination operand from left to right,
       and the source operand from right to left */
    for ( i = 0; i <= l; i++ )
    {
        /* Fetch a byte from the source operand */
        tbyte = ARCH_DEP(vfetchb) ( effective_addr2, b2, regs );

        /* Store the byte in the destination operand */
        ARCH_DEP(vstoreb) ( tbyte, effective_addr1, b1, regs );

        /* Increment destination operand address */
        effective_addr1++;
        effective_addr1 &= ADDRESS_MAXWRAP(regs);

        /* Decrement source operand address */
        effective_addr2--;
        effective_addr2 &= ADDRESS_MAXWRAP(regs);

    } /* end for(i) */
}


#define MOVE_NUMERIC_BUMP(_d,_s) \
    do { \
        *(_d)=( *(_d) & 0xF0) | ( *(_s) & 0x0F); \
        _d++; \
        _s++; \
    } while(0)

/*-------------------------------------------------------------------*/
/* D1   MVN   - Move Numerics                                   [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(move_numerics)
{
VADR    addr1, addr2;                   /* Operand virtual addresses */
int     len, arn1, arn2;                /* Operand values            */
BYTE   *dest1, *dest2;                  /* Destination addresses     */
BYTE   *source1, *source2;              /* Source addresses          */
BYTE   *sk1, *sk2;                      /* Storage key addresses     */
int     len2, len3;                     /* Lengths to copy           */
int     i;                              /* Loop counter              */

    SS_L(inst, regs, len, arn1, addr1, arn2, addr2);

    ITIMER_SYNC(addr2,len,regs);

    /* Translate addresses of leftmost operand bytes */
    dest1 = MADDR (addr1, arn1, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);
    sk1 = regs->dat.storkey;
    source1 = MADDR (addr2, arn2, regs, ACCTYPE_READ, regs->psw.pkey);

    for ( i = 0; i <= len; i++)
        MOVE_NUMERIC_BUMP(dest1,source1);
    ITIMER_UPDATE(addr1,len,regs);

#if defined(COMPARE_M65)
    dest1 = MADDR(addr1, arn1, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);
    record_herc_write(addr1, len + 1, dest1);
#endif
}


/*-------------------------------------------------------------------*/
/* F1   MVO   - Move with Offset                                [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(move_with_offset)
{
int     l1, l2;                         /* Lenght values             */
int     b1, b2;                         /* Values of base registers  */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */
int     i, j;                           /* Loop counters             */
BYTE    sbyte;                          /* Source operand byte       */
BYTE    dbyte;                          /* Destination operand byte  */

    SS(inst, regs, l1, l2, b1, effective_addr1,
                                     b2, effective_addr2);

    /* Fetch the rightmost byte from the source operand */
    effective_addr2 += l2;
    effective_addr2 &= ADDRESS_MAXWRAP(regs);
    sbyte = ARCH_DEP(vfetchb) ( effective_addr2--, b2, regs );

    /* Fetch the rightmost byte from the destination operand */
    effective_addr1 += l1;
    effective_addr1 &= ADDRESS_MAXWRAP(regs);
    dbyte = ARCH_DEP(vfetchb) ( effective_addr1, b1, regs );

    /* Move low digit of source byte to high digit of destination */
    dbyte &= 0x0F;
    dbyte |= sbyte << 4;
    ARCH_DEP(vstoreb) ( dbyte, effective_addr1--, b1, regs );

    /* Process remaining bytes from right to left */
    for (i = l1, j = l2; i > 0; i--)
    {
        /* Move previous high digit to destination low digit */
        dbyte = sbyte >> 4;

        /* Fetch next byte from second operand */
        if ( j-- > 0 ) {
            effective_addr2 &= ADDRESS_MAXWRAP(regs);
            sbyte = ARCH_DEP(vfetchb) ( effective_addr2--, b2, regs );
        }
        else
            sbyte = 0x00;

        /* Move low digit to destination high digit */
        dbyte |= sbyte << 4;
        effective_addr1 &= ADDRESS_MAXWRAP(regs);
        ARCH_DEP(vstoreb) ( dbyte, effective_addr1--, b1, regs );

    } /* end for(i) */

}

#define MOVE_ZONE_BUMP(_d,_s) \
    do { \
        *(_d)=( *(_d) & 0x0F) | ( *(_s) & 0xF0); \
        _d++; \
        _s++; \
    } while(0)

/*-------------------------------------------------------------------*/
/* D3   MVZ   - Move Zones                                      [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(move_zones)
{
VADR    addr1, addr2;                   /* Operand virtual addresses */
int     len, arn1, arn2;                /* Operand values            */
BYTE   *dest1, *dest2;                  /* Destination addresses     */
BYTE   *source1, *source2;              /* Source addresses          */
BYTE   *sk1, *sk2;                      /* Storage key addresses     */
int     len2, len3;                     /* Lengths to copy           */
int     i;                              /* Loop counter              */

    SS_L(inst, regs, len, arn1, addr1, arn2, addr2);

    ITIMER_SYNC(addr2,len,regs);

    /* Translate addresses of leftmost operand bytes */
    dest1 = MADDR (addr1, arn1, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);
    sk1 = regs->dat.storkey;
    source1 = MADDR (addr2, arn2, regs, ACCTYPE_READ, regs->psw.pkey);

#if defined(COMPARE_M65)
    /* MOVE_ZONE_BUMP advances dest1; retain the start for write recording. */
    BYTE* compare_dest = dest1;
#endif
    for ( i = 0; i <= len; i++)
        MOVE_ZONE_BUMP(dest1,source1);
    ITIMER_UPDATE(addr1,len,regs);
#if defined(COMPARE_M65)
    record_herc_write(addr1, len + 1, (char*)compare_dest);
#endif
}


/*-------------------------------------------------------------------*/
/* 1C   MR    - Multiply Register                               [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR_(inst, regs, r1, r2);

    ODD_CHECK(r1, regs);

    /* Multiply r1+1 by r2 and place result in r1 and r1+1 */
    mul_signed (&(regs->GR_L(r1)),&(regs->GR_L(r1+1)),
                    regs->GR_L(r1+1),
                    regs->GR_L(r2));
}


/*-------------------------------------------------------------------*/
/* 5C   M     - Multiply                                        [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX_(inst, regs, r1, b2, effective_addr2);

    ODD_CHECK(r1, regs);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Multiply r1+1 by n and place result in r1 and r1+1 */
    mul_signed (&(regs->GR_L(r1)), &(regs->GR_L(r1+1)),
                    regs->GR_L(r1+1),
                    n);

}


/*-------------------------------------------------------------------*/
/* 4C   MH    - Multiply Halfword                               [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(multiply_halfword)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
S32     n;                              /* 32-bit operand values     */

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Load 2 bytes from operand address */
    n = (S16)ARCH_DEP(vfetch2) ( effective_addr2, b2, regs );

    /* Multiply R1 register by n, ignore leftmost 32 bits of
       result, and place rightmost 32 bits in R1 register */
    mul_signed ((U32 *)&n, &(regs->GR_L(r1)), regs->GR_L(r1), n);

}


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "general1.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "general1.c"
#endif

#endif

#endif /*!defined(_GEN_ARCH)*/
