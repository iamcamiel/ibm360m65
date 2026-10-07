/* CONTROL.C    (c) Copyright Roger Bowler, 1994-2010                */
/*              ESA/390 CPU Emulator                                 */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2009      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2009      */

// $Id: control.c 5636 2010-02-16 14:30:20Z ivan $

/*-------------------------------------------------------------------*/
/* This module implements all control instructions of the            */
/* S/370 and ESA/390 architectures, as described in the manuals      */
/* GA22-7000-03 System/370 Principles of Operation                   */
/* SA22-7201-06 ESA/390 Principles of Operation                      */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      Bad frame support by Jan Jaeger                              */
/*      Branch tracing by Jan Jaeger                                 */
/*      CSP instructions by Jan Jaeger                               */
/*      Instruction decode by macros - Jan Jaeger                    */
/*      Prevent TOD from going backwards in time - Jan Jaeger        */
/*      Instruction decode rework - Jan Jaeger                       */
/*      PR may lose pending interrupts - Jan Jaeger                  */
/*      Modifications for Interpretive Execution (SIE) by Jan Jaeger */
/*      ESAME low-address protection - Roger Bowler                  */
/*      ESAME linkage stack operations - Roger Bowler                */
/*      ESAME BSA instruction - Roger Bowler                    v209c*/
/*      ASN-and-LX-reuse facility - Roger Bowler            June 2004*/
/*      SIGP orders 11,12.2,13,15 - Fish                     Oct 2005*/
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_CONTROL_C_)
#define _CONTROL_C_
#endif

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#if defined(OPTION_FISHIO)
#include "w32chan.h"
#endif // defined(OPTION_FISHIO)

#if !defined(SOFTWARE_M65) && !defined(HARDWARE_M65)

/* Temporary debug */
extern  int     ipending_cmd(int,void *,void *);

/*-------------------------------------------------------------------*/
/* 83   DIAG  - Diagnose                                        [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(diagnose)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    RS(inst, regs, r1, r3, b2, effective_addr2);

    PRIV_CHECK(regs);

    PTT(PTT_CL_INF,"DIAG",regs->GR_L(r1),regs->GR_L(r3),(U32)(effective_addr2 & 0xffffff));

    /* Process diagnose instruction */
    ARCH_DEP(diagnose_call) (effective_addr2, b2, r1, r3, regs);

    /* Perform serialization and checkpoint-synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

} /* end DEF_INST(diagnose) */


/*-------------------------------------------------------------------*/
/* 09   ISK   - Insert Storage Key                              [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(insert_storage_key)
{
int     r1, r2;                         /* Values of R fields        */
RADR    n;                              /* Absolute storage addr     */
    RR_(inst, regs, r1, r2);

    PRIV_CHECK(regs);

    /* Program check if R2 bits 28-31 are not zeroes */
    if ( regs->GR_L(r2) & 0x0000000F )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Load 2K block address from R2 register */
    n = regs->GR_L(r2) & 0x00FFF800;

    /* Addressing exception if block is outside main storage */
    if ( n > regs->mainlim )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

        /* Insert the storage key into R1 register bits 24-31 */
        regs->GR_LHLCL(r1) = STORAGE_KEY(n, regs) & 0xFE;

    /* In BC mode, clear bits 29-31 of R1 register */
        regs->GR_LHLCL(r1) &= 0xF8;
}


/*-------------------------------------------------------------------*/
/* 82   LPSW  - Load Program Status Word                         [S] */
/*-------------------------------------------------------------------*/
DEF_INST(load_program_status_word)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
DBLWRD  dword;
int     rc;

    S(inst, regs, b2, effective_addr2);
    PRIV_CHECK(regs);

    DW_CHECK(effective_addr2, regs);

    /* Perform serialization and checkpoint synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

    /* Fetch new PSW from operand address */
    STORE_DW ( dword, ARCH_DEP(vfetch8) ( effective_addr2, b2, regs ) );

    /* Set the breaking event address register */
    SET_BEAR_REG(regs, regs->ip - 4);

    /* Load updated PSW (ESA/390 Format in ESAME mode) */
    if ((rc = ARCH_DEP(load_psw) ( regs, dword )))
        ARCH_DEP(program_interrupt) (regs, rc);
    /* Perform serialization and checkpoint synchronization */
    PERFORM_SERIALIZATION (regs);
    PERFORM_CHKPT_SYNC (regs);

#if !defined(COMPARE_M65)
    RETURN_INTCHECK(regs);
#endif
} /* end DEF_INST(load_program_status_word) */


/*-------------------------------------------------------------------*/
/* 08   SSK   - Set Storage Key                                 [RR] */
/*-------------------------------------------------------------------*/
DEF_INST(set_storage_key)
{
int     r1, r2;                         /* Values of R fields        */
RADR    n;                              /* Absolute storage addr     */

    RR_(inst, regs, r1, r2);

    PRIV_CHECK(regs);

    /* Program check if R2 bits 28-31 are not zeroes */
    if ( regs->GR_L(r2) & 0x0000000F )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Load 2K block address from R2 register */
    n = regs->GR_L(r2) & 0x00FFF800;

    /* Addressing exception if block is outside main storage */
    if ( n > regs->mainlim )
        ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

#if defined(COMPARE_M65)
    record_herc_set_key(n, regs->GR_LHLCL(r1) & ~(STORKEY_BADFRM));
#endif
    {
        /* Update the storage key from R1 register bits 24-30 */
        STORAGE_KEY(n, regs) &= STORKEY_BADFRM;
        STORAGE_KEY(n, regs) |= regs->GR_LHLCL(r1) & ~(STORKEY_BADFRM);
    }

    STORKEY_INVALIDATE(regs, n);

//  /*debug*/logmsg("SSK storage block %8.8X key %2.2X\n",
//  /*debug*/       regs->GR_L(r2), regs->GR_LHLCL(r1) & 0xFE);

} /* end DEF_INST(set_storage_key) */


/*-------------------------------------------------------------------*/
/* 80   SSM   - Set System Mask                                  [S] */
/*-------------------------------------------------------------------*/
DEF_INST(set_system_mask)
{
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */

    S(inst, regs, b2, effective_addr2);
    /*
     * ECPS:VM - Before checking for prob/priv
     * Check CR6 to see if S-ASSIST is requested
     *
     * If we can process it, then do it
    */
    PRIV_CHECK(regs);

    /* Special operation exception if SSM-suppression is active */
    if ( (regs->CR(0) & CR0_SSM_SUPP)
        )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);

    /* Load new system mask value from operand address */
    regs->psw.sysmask = ARCH_DEP(vfetchb) ( effective_addr2, b2, regs );

    SET_IC_MASK(regs);

    RETURN_INTCHECK(regs);

} /* end DEF_INST(set_system_mask) */



/*-------------------------------------------------------------------*/
/* B6   STCTL - Store Control                                   [RS] */
/*-------------------------------------------------------------------*/
DEF_INST(store_control)
{
int     r1, r3;                         /* Register numbers          */
int     b2;                             /* Base of effective addr    */
VADR    effective_addr2;                /* Effective address         */
int     i, m, n;                        /* Integer work areas        */
U32    *p1, *p2 = NULL;                 /* Mainstor pointers         */

    RS(inst, regs, r1, r3, b2, effective_addr2);

    PRIV_CHECK(regs);

    FW_CHECK(effective_addr2, regs);

    /* Calculate number of regs to store */
    n = ((r3 - r1) & 0xF) + 1;

    /* Calculate number of words to next boundary */
    m = (0x800 - (effective_addr2 & 0x7ff)) >> 2;

    /* Address of operand beginning */
    p1 = (U32*)MADDR(effective_addr2, b2, regs, ACCTYPE_WRITE, regs->psw.pkey);

    /* Get address of next page if boundary crossed */
    if (unlikely (m < n))
        p2 = (U32*)MADDR(effective_addr2 + (m*4), b2, regs, ACCTYPE_WRITE, regs->psw.pkey);
    else
        m = n;

#if defined(COMPARE_M65)
    U32* compare_first = p1;
    U32* compare_second = p2;
#endif
    /* Store at operand beginning */
    for (i = 0; i < m; i++)
        store_fw (p1++, regs->CR_L((r1 + i) & 0xF));

    /* Store on next page */
    for ( ; i < n; i++)
        store_fw (p2++, regs->CR_L((r1 + i) & 0xF));

    ITIMER_UPDATE(effective_addr2,(n*4)-1,regs);
#if defined(COMPARE_M65)
    record_herc_write(effective_addr2, m*4, (char*)compare_first);
    if (m < n)
        record_herc_write((effective_addr2 + m*4) & ADDRESS_MAXWRAP(regs),
                          (n-m)*4, (char*)compare_second);
#endif

} /* end DEF_INST(store_control) */


/*-------------------------------------------------------------------*/
/* Calculate CPU capability indicator for STSI instruction           */
/*                                                                   */
/* The CPU capability indicator is 32-bit value which is calculated  */
/* dynamically. A lower value indicates a faster CPU. The value may  */
/* be either an unsigned binary integer or a floating point number.  */
/* If bits 0-8 are zero, it is an integer in the range 0 to 2**23-1. */
/* If bits 0-8 are nonzero, is is a 32-bit short BFP number.         */
/*-------------------------------------------------------------------*/
#if !defined(_STSI_CAPABILITY)
#define _STSI_CAPABILITY
static inline U32 stsi_capability (REGS *regs)
{
U64               dreg;                /* work doubleword            */
struct rusage     usage;               /* RMF type data              */

#define SUSEC_PER_MIPS 48              /* One MIPS eq 48 SU          */

        getrusage(RUSAGE_SELF,&usage);
        dreg = (U64)(usage.ru_utime.tv_sec + usage.ru_stime.tv_sec);
        dreg = (dreg * 1000000) + (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec);
        dreg = INSTCOUNT(regs) / (dreg ? dreg : 1);
        dreg *= SUSEC_PER_MIPS;
        return 0x800000 / (dreg ? dreg : 1);

} /* end function stsi_capability */
#endif /*!defined(_STSI_CAPABILITY)*/


/*-------------------------------------------------------------------*/
/* AC   STNSM - Store Then And Systen Mask                      [SI] */
/*-------------------------------------------------------------------*/
DEF_INST(store_then_and_system_mask)
{
BYTE    i2;                             /* Immediate byte of opcode  */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */

    SI(inst, regs, i2, b1, effective_addr1);

    PRIV_CHECK(regs);

    /* Store current system mask value into storage operand */
    ARCH_DEP(vstoreb) ( regs->psw.sysmask, effective_addr1, b1, regs );

    /* AND system mask with immediate operand */
    regs->psw.sysmask &= i2;

    SET_IC_MASK(regs);

    RETURN_INTCHECK(regs);

} /* end DEF_INST(store_then_and_system_mask) */


/*-------------------------------------------------------------------*/
/* AD   STOSM - Store Then Or Systen Mask                       [SI] */
/*-------------------------------------------------------------------*/
DEF_INST(store_then_or_system_mask)
{
BYTE    i2;                             /* Immediate byte of opcode  */
int     b1;                             /* Base of effective addr    */
VADR    effective_addr1;                /* Effective address         */

    SI(inst, regs, i2, b1, effective_addr1);

    PRIV_CHECK(regs);

    /* Store current system mask value into storage operand */
    ARCH_DEP(vstoreb) ( regs->psw.sysmask, effective_addr1, b1, regs );

    /* OR system mask with immediate operand */
    regs->psw.sysmask |= i2;

    SET_IC_MASK(regs);

    RETURN_INTCHECK(regs);

} /* end DEF_INST(store_then_or_system_mask) */


/*-------------------------------------------------------------------*/
/* B246 STURA - Store Using Real Address                       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(store_using_real_address)
{
int     r1, r2;                         /* Values of R fields        */
RADR    n;                              /* Unsigned work             */

    RRE(inst, regs, r1, r2);

    PRIV_CHECK(regs);

    /* R2 register contains operand real storage address */
    n = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

    /* Program check if operand not on fullword boundary */
    FW_CHECK(n, regs);

    /* Store R1 register at second operand location */
    ARCH_DEP(vstore4) (regs->GR_L(r1), n, USE_REAL_ADDR, regs );

} /* end DEF_INST(store_using_real_address) */


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "control.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "control.c"
#endif

#endif

#endif /*!defined(_GEN_ARCH)*/
