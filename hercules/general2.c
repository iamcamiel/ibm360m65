/* GENERAL2.C   (c) Copyright Roger Bowler, 1994-2009                */
/*              ESA/390 CPU Emulator                                 */
/*              Instructions N-Z                                     */

// $Id: general2.c 5222 2009-03-01 12:28:49Z jj $

/*              (c) Copyright Peter Kuschnerus, 1999-2009 (UPT & CFC)*/

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2009      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2009      */

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
// Revision 1.127  2009/01/23 11:56:02  bernard
// copyright notice
//
// Revision 1.126  2008/03/28 13:36:25  rbowler
// Fix incorrect registers when cc=1 for TRTE,TRTRE
//
// Revision 1.125  2008/03/27 16:45:17  rbowler
// TRTE and TRTRE incorrectly return cc=3
//
// Revision 1.124  2008/03/06 16:10:35  rbowler
// Remove extraneous trailing blanks (cosmetic change only)
//
// Revision 1.123  2008/03/01 15:15:41  bernard
// Removed the silly control-M characters
//
// Revision 1.122  2008/03/01 12:19:04  rbowler
// Rename new features to include the word facility
//
// Revision 1.121  2008/03/01 07:51:00  bernard
// TRTE, TRTRE: Checking for a_bit and ch>255 is double. Removed the a_bit.
//
// Revision 1.120  2008/02/29 16:25:13  bernard
// Added Parsing Enhancement Facility
//
// Revision 1.119  2008/02/15 21:21:18  ptl00
// Fix STCKE so that byte 0 is cleared
//
// Revision 1.118  2008/01/24 00:59:03  gsmith
// Fix and optimize TR instruction
//
// Revision 1.117  2007/12/30 17:48:21  bernard
// Anoter UTF xlate error
//
// Revision 1.116  2007/12/30 09:09:51  bernard
// Some errors in UTF translation
//
// Revision 1.115  2007/11/30 15:14:14  rbowler
// Permit String-Instruction facility to be activated in S/370 mode
//
// Revision 1.114  2007/06/23 00:04:10  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.113  2007/01/13 07:21:48  bernard
// backout ccmask
//
// Revision 1.112  2007/01/12 15:23:51  bernard
// ccmask phase 1
//
// Revision 1.111  2007/01/09 05:10:19  gsmith
// Tweaks to lm/stm
//
// Revision 1.110  2007/01/04 23:12:04  gsmith
// remove thunk calls for program_interrupt
//
// Revision 1.109  2006/12/31 21:16:32  gsmith
// 2006 Dec 31 really back out mainlockx.pat
//
// Revision 1.108  2006/12/20 09:09:40  jj
// Fix bogus log entries
//
// Revision 1.107  2006/12/20 04:26:20  gsmith
// 19 Dec 2006 ip_all.pat - performance patch - Greg Smith
//
// Revision 1.106  2006/12/20 04:22:00  gsmith
// 2006 Dec 19 Backout mainlockx.pat - possible SMP problems - Greg Smith
//
// Revision 1.105  2006/12/08 09:43:21  jj
// Add CVS message log
//

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_GENERAL2_C_)
#define _GENERAL2_C_
#endif

#if !defined(SOFTWARE_M65) && !defined(HARDWARE_M65)

#include "hercules.h"
#include "opcode.h"
#include "inline.h"
#include "clock.h"


/*-------------------------------------------------------------------*/
/* 16   OR    - Or Register                                     [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(or_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR0(inst, regs, r1, r2);

    /* OR second operand with first and set condition code */
    regs->psw.cc = ( regs->GR_L(r1) |= regs->GR_L(r2) ) ? 1 : 0;
}


/*-------------------------------------------------------------------*/
/* 56   O     - Or                                              [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(or)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* OR second operand with first and set condition code */
    regs->psw.cc = ( regs->GR_L(r1) |= n ) ? 1 : 0;
}


/*-------------------------------------------------------------------*/
/* 96   OI    - Or Immediate                                    [SI] */
/*-------------------------------------------------------------------*/
DEF_INST(or_immediate)
{
BYTE    i2;                             /* Immediate operand byte    */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
BYTE   *dest;                           /* Pointer to target byte    */

    SI(inst, regs, i2, b1, effective_addr1);

    ITIMER_SYNC(effective_addr1,1,regs);
    /* Get byte mainstor address */
    dest = MADDR (effective_addr1, b1, regs, ACCTYPE_WRITE, regs->psw.pkey );

    /* OR byte with immediate operand, setting condition code */
    regs->psw.cc = ((*dest |= i2) != 0);
#if defined(COMPARE_M65)
    record_herc_write_char(effective_addr1, *dest);
#endif
    ITIMER_UPDATE(effective_addr1,1,regs);
}


/*-------------------------------------------------------------------*/
/* D6   OC    - Or Characters                                   [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(or_character)
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

    /* Quick out for 1 byte (no boundary crossed) */
    if (unlikely(len == 0))
    {
        source1 = MADDR (addr2, b2, regs, ACCTYPE_READ, regs->psw.pkey);
        dest1 = MADDR (addr1, b1, regs, ACCTYPE_WRITE, regs->psw.pkey);
        *dest1 |= *source1;
        regs->psw.cc = (*dest1 != 0);
        ITIMER_UPDATE(addr1,len,regs);
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
        if (*dest1++ |= *source1++) cc = 1;
    regs->psw.cc = cc;

    ITIMER_UPDATE(addr1,len,regs);
#if defined(COMPARE_M65)
    dest1 = MADDR(addr1, b1, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);
    record_herc_write(addr1, len + 1, dest1);
#endif

}


/*-------------------------------------------------------------------*/
/* F2   PACK  - Pack                                            [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(pack)
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

    /* If operand 1 crosses a page, make sure both pages are accessable */
    if((effective_addr1 & PAGEFRAME_PAGEMASK) !=
        ((effective_addr1 + l1) & PAGEFRAME_PAGEMASK))
        ARCH_DEP(validate_operand) (effective_addr1, b1, l1, ACCTYPE_WRITE_SKP, regs);

    /* If operand 2 crosses a page, make sure both pages are accessable */
    if((effective_addr2 & PAGEFRAME_PAGEMASK) !=
        ((effective_addr2 + l2) & PAGEFRAME_PAGEMASK))
        ARCH_DEP(validate_operand) (effective_addr2, b2, l2, ACCTYPE_READ, regs);

    /* Exchange the digits in the rightmost byte */
    effective_addr1 += l1;
    effective_addr2 += l2;
    sbyte = ARCH_DEP(vfetchb) ( effective_addr2, b2, regs );
    dbyte = (sbyte << 4) | (sbyte >> 4);
    ARCH_DEP(vstoreb) ( dbyte, effective_addr1, b1, regs );

    /* Process remaining bytes from right to left */
    for (i = l1, j = l2; i > 0; i--)
    {
        /* Fetch source bytes from second operand */
        if (j-- > 0)
        {
            sbyte = ARCH_DEP(vfetchb) ( --effective_addr2, b2, regs );
            dbyte = sbyte & 0x0F;

            if (j-- > 0)
            {
                effective_addr2 &= ADDRESS_MAXWRAP(regs);
                sbyte = ARCH_DEP(vfetchb) ( --effective_addr2, b2, regs );
                dbyte |= sbyte << 4;
            }
        }
        else
        {
            dbyte = 0;
        }

        /* Store packed digits at first operand address */
        ARCH_DEP(vstoreb) ( dbyte, --effective_addr1, b1, regs );

        /* Wraparound according to addressing mode */
        effective_addr1 &= ADDRESS_MAXWRAP(regs);
        effective_addr2 &= ADDRESS_MAXWRAP(regs);

    } /* end for(i) */

}


/*-------------------------------------------------------------------*/
/* 04   SPM   - Set Program Mask                                [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(set_program_mask)
{
int     r1, r2;                         /* Values of R fields        */

    RR0(inst, regs, r1, r2);

    /* Set condition code from bits 2-3 of R1 register */
    regs->psw.cc = ( regs->GR_L(r1) & 0x30000000 ) >> 28;

    /* Set program mask from bits 4-7 of R1 register */
    regs->psw.progmask = ( regs->GR_L(r1) >> 24) & PSW_PROGMASK;
}


/*-------------------------------------------------------------------*/
/* 8F   SLDA  - Shift Left Double                               [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_left_double)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n;                              /* 32-bit operand values     */
U64     dreg;                           /* Double register work area */
U32     h, i, j, m;                     /* Integer work areas        */

    RS(inst, regs, r1, r3, b2, effective_addr2);

    ODD_CHECK(r1, regs);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Load the signed value from the R1 and R1+1 registers */
    dreg = (U64)regs->GR_L(r1) << 32 | regs->GR_L(r1+1);
    m = ((S64)dreg < 0) ? 1 : 0;

    /* Shift the numeric portion of the value */
    for (i = 0, j = 0; i < n; i++)
    {
        /* Shift bits 1-63 left one bit position */
        dreg <<= 1;

        /* Overflow if bit shifted out is unlike the sign bit */
        h = ((S64)dreg < 0) ? 1 : 0;
        if (h != m)
            j = 1;
    }

    /* Load updated value into the R1 and R1+1 registers */
    regs->GR_L(r1) = (dreg >> 32) & 0x7FFFFFFF;
    if (m)
        regs->GR_L(r1) |= 0x80000000;
    regs->GR_L(r1+1) = dreg & 0xFFFFFFFF;

    /* Condition code 3 and program check if overflow occurred */
    if (j)
    {
        regs->psw.cc = 3;
        if ( FOMASK(&regs->psw) )
            regs->program_interrupt (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);
        return;
    }

    /* Set the condition code */
    regs->psw.cc = (S64)dreg > 0 ? 2 : (S64)dreg < 0 ? 1 : 0;

}


/*-------------------------------------------------------------------*/
/* 8D   SLDL  - Shift Left Double Logical                       [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_left_double_logical)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n;                              /* 32-bit operand values     */
U64     dreg;                           /* Double register work area */

    RS(inst, regs, r1, r3, b2, effective_addr2);

    ODD_CHECK(r1, regs);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Shift the R1 and R1+1 registers */
    dreg = (U64)regs->GR_L(r1) << 32 | regs->GR_L(r1+1);
    dreg <<= n;
    regs->GR_L(r1) = dreg >> 32;
    regs->GR_L(r1+1) = dreg & 0xFFFFFFFF;

}


/*-------------------------------------------------------------------*/
/* 8B   SLA   - Shift Left Single                               [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_left_single)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n, n1, n2;                      /* 32-bit operand values     */
U32     i, j;                           /* Integer work areas        */

    RS(inst, regs, r1, r3, b2, effective_addr2);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Fast path if no possible overflow */
    if (regs->GR_L(r1) < 0x10000 && n < 16)
    {
        regs->GR_L(r1) <<= n;
        regs->psw.cc = regs->GR_L(r1) ? 2 : 0;
        return;
    }

    /* Load the numeric and sign portions from the R1 register */
    n1 = regs->GR_L(r1) & 0x7FFFFFFF;
    n2 = regs->GR_L(r1) & 0x80000000;

    /* Shift the numeric portion left n positions */
    for (i = 0, j = 0; i < n; i++)
    {
        /* Shift bits 1-31 left one bit position */
        n1 <<= 1;

        /* Overflow if bit shifted out is unlike the sign bit */
        if ((n1 & 0x80000000) != n2)
            j = 1;
    }

    /* Load the updated value into the R1 register */
    regs->GR_L(r1) = (n1 & 0x7FFFFFFF) | n2;

    /* Condition code 3 and program check if overflow occurred */
    if (j)
    {
        regs->psw.cc = 3;
        if ( FOMASK(&regs->psw) )
            regs->program_interrupt (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);
        return;
    }

    /* Set the condition code */
    regs->psw.cc = (S32)regs->GR_L(r1) > 0 ? 2 :
                   (S32)regs->GR_L(r1) < 0 ? 1 : 0;

}


/*-------------------------------------------------------------------*/
/* 89   SLL   - Shift Left Single Logical                       [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_left_single_logical)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n;                              /* Integer work areas        */

    RS0(inst, regs, r1, r3, b2, effective_addr2);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Shift the R1 register */
    regs->GR_L(r1) = n > 31 ? 0 : regs->GR_L(r1) << n;
}


/*-------------------------------------------------------------------*/
/* 8E   SRDA  - Shift Right Double                              [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_right_double)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n;                              /* 32-bit operand values     */
U64     dreg;                           /* Double register work area */

    RS(inst, regs, r1, r3, b2, effective_addr2);

    ODD_CHECK(r1, regs);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Shift the R1 and R1+1 registers */
    dreg = (U64)regs->GR_L(r1) << 32 | regs->GR_L(r1+1);
    dreg = (U64)((S64)dreg >> n);
    regs->GR_L(r1) = dreg >> 32;
    regs->GR_L(r1+1) = dreg & 0xFFFFFFFF;

    /* Set the condition code */
    regs->psw.cc = (S64)dreg > 0 ? 2 : (S64)dreg < 0 ? 1 : 0;

}


/*-------------------------------------------------------------------*/
/* 8C   SRDL  - Shift Right Double Logical                      [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_right_double_logical)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n;                              /* 32-bit operand values     */
U64     dreg;                           /* Double register work area */

    RS(inst, regs, r1, r3, b2, effective_addr2);

    ODD_CHECK(r1, regs);

        /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Shift the R1 and R1+1 registers */
    dreg = (U64)regs->GR_L(r1) << 32 | regs->GR_L(r1+1);
    dreg >>= n;
    regs->GR_L(r1) = dreg >> 32;
    regs->GR_L(r1+1) = dreg & 0xFFFFFFFF;

}


/*-------------------------------------------------------------------*/
/* 8A   SRA   - Shift Right Single                              [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_right_single)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n;                              /* Integer work areas        */

    RS0(inst, regs, r1, r3, b2, effective_addr2);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Shift the signed value of the R1 register */
    regs->GR_L(r1) = n > 30 ?
                    ((S32)regs->GR_L(r1) < 0 ? -1 : 0) :
                    (S32)regs->GR_L(r1) >> n;

    /* Set the condition code */
    regs->psw.cc = ((S32)regs->GR_L(r1) > 0) ? 2 :
                   (((S32)regs->GR_L(r1) < 0) ? 1 : 0);
}


/*-------------------------------------------------------------------*/
/* 88   SRL   - Shift Right Single Logical                      [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(shift_right_single_logical)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
U32     n;                              /* Integer work areas        */

    RS0(inst, regs, r1, r3, b2, effective_addr2);

    /* Use rightmost six bits of operand address as shift count */
    n = effective_addr2 & 0x3F;

    /* Shift the R1 register */
    regs->GR_L(r1) = n > 31 ? 0 : regs->GR_L(r1) >> n;
}


/*-------------------------------------------------------------------*/
/* 50   ST    - Store                                           [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(store)
{
int     r1;                             /* Values of R fields        */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
#if 0
U32    *p;                              /* Mainstor pointer          */
#endif

    RX_(inst, regs, r1, b2, effective_addr2);

//    if ((effective_addr2 & 3)) {
//        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
//        return;
//    }
    ARCH_DEP(vstore4) ( regs->GR_L(r1), effective_addr2, b2, regs );

} /* end DEF_INST(store) */


/*-------------------------------------------------------------------*/
/* 42   STC   - Store Character                                 [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(store_character)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Store rightmost byte of R1 register at operand address */
    ARCH_DEP(vstoreb) ( regs->GR_LHLCL(r1), effective_addr2, b2, regs );
}


/*-------------------------------------------------------------------*/
/* 40   STH   - Store Halfword                                  [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(store_halfword)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Store rightmost 2 bytes of R1 register at operand address */
    ARCH_DEP(vstore2) ( regs->GR_LHL(r1), effective_addr2, b2, regs );
}


/*-------------------------------------------------------------------*/
/* 90   STM   - Store Multiple                                  [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(store_multiple)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* effective address base    */
VADR    effective_addr2;                /* effective address         */
int     i, n;                        /* Integer work areas        */
U32    *p1, *p2;                        /* Mainstor pointers         */
BYTE   *bp1;                            /* Unaligned mainstor ptr    */

    RS(inst, regs, r1, r3, b2, effective_addr2);

    /* Calculate number of bytes to store */
    n = (((r3 - r1) & 0xF) + 1);

    /* Get address of first page */
    bp1 = (BYTE*)MADDR(effective_addr2, b2, regs, ACCTYPE_WRITE, regs->psw.pkey);
    p1 = (U32*)bp1;

#if defined(OPTION_STRICT_ALIGNMENT)
    if(likely(!(((uintptr_t)effective_addr2)&0x03)))
    {
#endif
        for (i = 0; i < n; i++)
            store_fw (p1++, regs->GR_L((r1 + i) & 0xF));
#if defined(OPTION_STRICT_ALIGNMENT)
    }
    else
    {
        for (i = 0; i < n; i++,bp1+=4)
            store_fw (bp1, regs->GR_L((r1 + i) & 0xF));
    }
#endif

    ITIMER_UPDATE(effective_addr2,(n*4)-1,regs);
#if defined(COMPARE_M65)
    bp1 = (BYTE*)MADDR(effective_addr2, b2, regs, ACCTYPE_WRITE, regs->psw.pkey);
    record_herc_write(effective_addr2, n << 2, bp1);
#endif

} /* end DEF_INST(store_multiple) */


/*-------------------------------------------------------------------*/
/* 1B   SR    - Subtract Register                               [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR_(inst, regs, r1, r2);

    /* Subtract signed operands and set condition code */
    regs->psw.cc =
            sub_signed (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    regs->GR_L(r2));

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && FOMASK(&regs->psw) )
        regs->program_interrupt (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);
}


/*-------------------------------------------------------------------*/
/* 5B   S     - Subtract                                        [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Subtract signed operands and set condition code */
    regs->psw.cc =
            sub_signed (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    n);

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && FOMASK(&regs->psw) )
        regs->program_interrupt (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);
}


/*-------------------------------------------------------------------*/
/* 4B   SH    - Subtract Halfword                               [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_halfword)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Load 2 bytes from operand address */
    n = (S16)ARCH_DEP(vfetch2) ( effective_addr2, b2, regs );

    /* Subtract signed operands and set condition code */
    regs->psw.cc =
            sub_signed (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    n);

    /* Program check if fixed-point overflow */
    if ( regs->psw.cc == 3 && FOMASK(&regs->psw) )
        regs->program_interrupt (regs, PGM_FIXED_POINT_OVERFLOW_EXCEPTION);
}


/*-------------------------------------------------------------------*/
/* 1F   SLR   - Subtract Logical Register                       [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_logical_register)
{
int     r1, r2;                         /* Values of R fields        */

    RR0(inst, regs, r1, r2);

    /* Subtract unsigned operands and set condition code */
    if (likely(r1 == r2))
    {
        regs->psw.cc = 2;
        regs->GR_L(r1) = 0;
    }
    else
        regs->psw.cc =
            sub_logical (&(regs->GR_L(r1)),
                           regs->GR_L(r1),
                           regs->GR_L(r2));
}


/*-------------------------------------------------------------------*/
/* 5F   SL    - Subtract Logical                                [RX] */
/*-------------------------------------------------------------------*/
DEF_INST(subtract_logical)
{
int     r1;                             /* Value of R field          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
U32     n;                              /* 32-bit operand values     */

    RX_(inst, regs, r1, b2, effective_addr2);

    /* Load second operand from operand address */
    n = ARCH_DEP(vfetch4) ( effective_addr2, b2, regs );

    /* Subtract unsigned operands and set condition code */
    regs->psw.cc =
            sub_logical (&(regs->GR_L(r1)),
                    regs->GR_L(r1),
                    n);
}


/*-------------------------------------------------------------------*/
/* 0A   SVC   - Supervisor Call                                 [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(supervisor_call)
{
BYTE    i;                              /* Instruction byte 1        */
PSA    *psa;                            /* -> prefixed storage area  */
int     rc;                             /* Return code               */

    RR_SVC(inst, regs, i);

    /* Set the main storage reference and change bits */
    STORAGE_KEY(0, regs) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Use the I-byte to set the SVC interruption code */
    regs->psw.intcode = i;

    /* Point to PSA in main storage */
    psa = (void*)(regs->mainstor);

    /* Store current PSW at PSA+X'20' */
    ARCH_DEP(store_psw) ( regs, psa->svcold );

    /* Load new PSW from PSA+X'60' */
    if ( (rc = ARCH_DEP(load_psw) ( regs, psa->svcnew ) ) )
        regs->program_interrupt (regs, rc);

    /* Perform serialization and checkpoint synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    RETURN_INTCHECK(regs);

}


/*-------------------------------------------------------------------*/
/* 93   TS    - Test and Set                                     [S] */
/*-------------------------------------------------------------------*/
DEF_INST(test_and_set)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
BYTE   *main2;                          /* Mainstor address          */
BYTE    old;                            /* Old value                 */

    S(inst, regs, b2, effective_addr2);

    ITIMER_SYNC(effective_addr2,0,regs);
    /* Perform serialization before starting operation */
    PERFORM_SERIALIZATION (regs);

    /* Get operand absolute address */
    main2 = MADDR (effective_addr2, b2, regs, ACCTYPE_WRITE, regs->psw.pkey);

    /* Obtain main-storage access lock */
    OBTAIN_MAINLOCK(regs);

    /* Get old value */
    old = *main2;

    /* Attempt to exchange the values */
    while (cmpxchg1(&old, 255, main2));
    regs->psw.cc = old >> 7;
#if defined(COMPARE_M65)
    record_herc_write_char(effective_addr2, *main2);
#endif

    /* Release main-storage access lock */
    RELEASE_MAINLOCK(regs);

    /* Perform serialization after completing operation */
    PERFORM_SERIALIZATION (regs);

    if (regs->psw.cc == 1)
    {
            if (sysblk.cpus > 1)
                sched_yield();
    }
    else
    {
        ITIMER_UPDATE(effective_addr2,0,regs);
    }
}


/*-------------------------------------------------------------------*/
/* 91   TM    - Test under Mask                                 [SI] */
/*-------------------------------------------------------------------*/
DEF_INST(test_under_mask)
{
BYTE    i2;                             /* Immediate operand         */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */
BYTE    tbyte;                          /* Work byte                 */

    SI(inst, regs, i2, b1, effective_addr1);

    /* Fetch byte from operand address */
    tbyte = ARCH_DEP(vfetchb) ( effective_addr1, b1, regs );

    /* AND with immediate operand mask */
    tbyte &= i2;

    /* Set condition code according to result */
    regs->psw.cc =
            ( tbyte == 0 ) ? 0 :            /* result all zeroes */
            ( tbyte == i2) ? 3 :            /* result all ones   */
            1 ;                             /* result mixed      */
}


/*-------------------------------------------------------------------*/
/* DC   TR    - Translate                                       [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(translate)
{
int     len, len2 = -1;                 /* Lengths                   */
int     b1, b2;                         /* Values of base field      */
int     i, b, n;                        /* Work variables            */
VADR    addr1, addr2;                   /* Effective addresses       */
BYTE   *dest, *dest2 = NULL, *tab, *tab2; /* Mainstor pointers       */

    SS_L(inst, regs, len, b1, addr1, b2, addr2);

    /* Get destination pointer */
    dest = MADDR (addr1, b1, regs, ACCTYPE_WRITE, regs->psw.pkey);

    tab = MADDR (addr2, b2, regs, ACCTYPE_READ, regs->psw.pkey);
    /* Perform translate function */
    for (i = 0; i <= len; i++)
        dest[i] = tab[dest[i]];
#if defined(COMPARE_M65)
    record_herc_write(addr1, len + 1, dest);
#endif
}


/*-------------------------------------------------------------------*/
/* DD   TRT   - Translate and Test                              [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(translate_and_test)
{
int     l;                              /* Lenght byte               */
int     b1, b2;                         /* Values of base field      */
VADR    effective_addr1,
        effective_addr2;                /* Effective addresses       */
int     cc = 0;                         /* Condition code            */
BYTE    sbyte;                          /* Byte work areas           */
BYTE    dbyte;                          /* Byte work areas           */
int     i;                              /* Integer work areas        */

    SS_L(inst, regs, l, b1, effective_addr1,
                                  b2, effective_addr2);

    /* Process first operand from left to right */
    for ( i = 0; i <= l; i++ )
    {
        /* Fetch argument byte from first operand */
        dbyte = ARCH_DEP(vfetchb) ( effective_addr1, b1, regs );

        /* Fetch function byte from second operand */
        sbyte = ARCH_DEP(vfetchb) ( (effective_addr2 + dbyte)
                                   & ADDRESS_MAXWRAP(regs), b2, regs );

        /* Test for non-zero function byte */
        if (sbyte != 0) {

            /* Store address of argument byte in register 1 */
            if ( regs->psw.amode )
                regs->GR_L(1) = effective_addr1;
            else
                regs->GR_LA24(1) = effective_addr1;

            /* Store function byte in low-order byte of reg.2 */
            regs->GR_LHLCL(2) = sbyte;

            /* Set condition code 2 if argument byte was last byte
               of first operand, otherwise set condition code 1 */
            cc = (i == l) ? 2 : 1;

            /* Terminate the operation at this point */
            break;

        } /* end if(sbyte) */

        /* Increment first operand address */
        effective_addr1++;
        effective_addr1 &= ADDRESS_MAXWRAP(regs);

    } /* end for(i) */

    /* Update the condition code */
    regs->psw.cc = cc;
}


/*-------------------------------------------------------------------*/
/* F3   UNPK  - Unpack                                          [SS] */
/*-------------------------------------------------------------------*/
DEF_INST(unpack)
{
int     l1, l2;                         /* Register numbers          */
int     b1, b2;                         /* Base registers            */
VADR    effective_addr1,
        effective_addr2;                /* Effective addressES       */
int     i, j;                           /* Loop counters             */
BYTE    sbyte;                          /* Source operand byte       */
BYTE    rbyte;                          /* Right result byte of pair */
BYTE    lbyte;                          /* Left result byte of pair  */

    SS(inst, regs, l1, l2, b1, effective_addr1,
                                     b2, effective_addr2);

    /* If operand 1 crosses a page, make sure both pages are accessable */
    if((effective_addr1 & PAGEFRAME_PAGEMASK) !=
        ((effective_addr1 + l1) & PAGEFRAME_PAGEMASK))
        ARCH_DEP(validate_operand) (effective_addr1, b1, l1, ACCTYPE_WRITE_SKP, regs);

    /* If operand 2 crosses a page, make sure both pages are accessable */
    if((effective_addr2 & PAGEFRAME_PAGEMASK) !=
        ((effective_addr2 + l2) & PAGEFRAME_PAGEMASK))
        ARCH_DEP(validate_operand) (effective_addr2, b2, l2, ACCTYPE_READ, regs);

    /* Exchange the digits in the rightmost byte */
    effective_addr1 += l1;
    effective_addr2 += l2;
    sbyte = ARCH_DEP(vfetchb) ( effective_addr2, b2, regs );
    rbyte = (sbyte << 4) | (sbyte >> 4);
    ARCH_DEP(vstoreb) ( rbyte, effective_addr1, b1, regs );

    /* Process remaining bytes from right to left */
    for (i = l1, j = l2; i > 0; i--)
    {
        /* Fetch source byte from second operand */
        if (j-- > 0)
        {
            sbyte = ARCH_DEP(vfetchb) ( --effective_addr2, b2, regs );
            rbyte = (sbyte & 0x0F) | 0xF0;
            lbyte = (sbyte >> 4) | 0xF0;
        }
        else
        {
            rbyte = 0xF0;
            lbyte = 0xF0;
        }

        /* Store unpacked bytes at first operand address */
        ARCH_DEP(vstoreb) ( rbyte, --effective_addr1, b1, regs );
        if (--i > 0)
        {
            effective_addr1 &= ADDRESS_MAXWRAP(regs);
            ARCH_DEP(vstoreb) ( lbyte, --effective_addr1, b1, regs );
        }

        /* Wraparound according to addressing mode */
        effective_addr1 &= ADDRESS_MAXWRAP(regs);
        effective_addr2 &= ADDRESS_MAXWRAP(regs);

    } /* end for(i) */

}


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "general2.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "general2.c"
#endif

#endif

#endif /*!defined(_GEN_ARCH)*/
