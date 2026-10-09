/* CPU.C        (c) Copyright Roger Bowler, 1994-2009                */
/*              ESA/390 CPU Emulator                                 */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2009      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2009      */

// $Id: cpu.c 5553 2009-12-23 17:34:16Z ivan $

/*-------------------------------------------------------------------*/
/* This module implements the CPU instruction execution function of  */
/* the S/370 and ESA/390 architectures, as described in the manuals  */
/* GA22-7000-03 System/370 Principles of Operation                   */
/* SA22-7201-06 ESA/390 Principles of Operation                      */
/* SA22-7832-00 z/Architecture Principles of Operation               */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      Nullification corrections by Jan Jaeger                      */
/*      Set priority by Reed H. Petty from an idea by Steve Gay      */
/*      Corrections to program check by Jan Jaeger                   */
/*      Light optimization on critical path by Valery Pogonchenko    */
/*      OSTAILOR parameter by Jay Maynard                            */
/*      CPU timer and clock comparator interrupt improvements by     */
/*          Jan Jaeger, after a suggestion by Willem Konynenberg     */
/*      Instruction decode rework - Jan Jaeger                       */
/*      Modifications for Interpretive Execution (SIE) by Jan Jaeger */
/*      Basic FP extensions support - Peter Kuschnerus           v209*/
/*      ASN-and-LX-reuse facility - Roger Bowler, June 2004      @ALR*/
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.209  2009/01/23 11:46:20  bernard
// copyright notice
//
// Revision 1.208  2009/01/09 23:25:59  ivan
// Fix monitor code location when the monitor call occurs during the interception of a z/Arch SIE guest
//
// Revision 1.207  2008/12/21 02:51:58  ivan
// Place the configuration in system check-stop state when a READ SCP INFO
// is issued from a CPU that is not a CP Engine.
//
// Revision 1.206  2008/12/05 11:26:00  jj
// Fix SIE psw update when host interrupt occurs durng guest processing
//
// Revision 1.205  2008/11/04 05:56:31  fish
// Put ensure consistent create_thread ATTR usage change back in
//
// Revision 1.204  2008/11/03 15:31:57  rbowler
// Back out consistent create_thread ATTR modification
//
// Revision 1.203  2008/10/18 09:32:20  fish
// Ensure consistent create_thread ATTR usage
//
// Revision 1.202  2008/10/07 22:24:35  gsmith
// Fix zero ilc problem after branch trace
//
// Revision 1.201  2008/08/13 21:21:55  gsmith
// Fix bad guest IA after host interrupt during branch trace
//
// Revision 1.200  2008/04/11 14:28:15  bernard
// Integrate regs->exrl into base Hercules code.
//
// Revision 1.199  2008/04/09 07:35:32  bernard
// allign to Rogers terminal ;-)
//
// Revision 1.198  2008/04/08 17:12:29  bernard
// Added execute relative long instruction
//
// Revision 1.197  2008/02/19 11:49:19  ivan
// - Move setting of CPU priority after spwaning timer thread
// - Added support for Posix 1003.1e capabilities
//
// Revision 1.196  2008/02/12 18:23:39  jj
// 1. SPKA was missing protection check (PIC04) because
//    AIA regs were not purged.
//
// 2. BASR with branch trace and PIC16, the pgm old was pointing
//    2 bytes before the BASR.
//
// 3. TBEDR , TBDR using R1 as source, should be R2.
//
// 4. PR with page crossing stack (1st page invalid) and PSW real
//    in stack, missed the PIC 11. Fixed by invoking abs_stck_addr
//    for previous stack entry descriptor before doing the load_psw.
//
// Revision 1.195  2007/12/10 23:12:02  gsmith
// Tweaks to OPTION_MIPS_COUNTING processing
//
// Revision 1.194  2007/12/07 12:08:51  rbowler
// Enable B9xx,EBxx opcodes in S/370 mode for ETF2 (correction)
//
// Revision 1.193  2007/12/02 16:22:09  rbowler
// Enable B9xx,EBxx opcodes in S/370 mode for ETF2
//
// Revision 1.192  2007/11/22 03:49:01  ivan
// Store Monitor code DOUBLEWORD when MC invoked under z/Architecture
// (previously only a fullword was stored)
//
// Revision 1.190  2007/11/02 20:19:20  ivan
// Remove longjmp in process_interrupt() when cpu is STOPPING and a store status
// was requested - leads to a CPU in the STOPPED state that is still executing
// instructions.
//
// Revision 1.189  2007/08/06 22:14:53  gsmith
// process_interrupt returns if nothing happens
//
// Revision 1.188  2007/08/06 22:14:07  gsmith
// rework CPU execution loop
//
// Revision 1.187  2007/08/06 22:12:49  gsmith
// cpu thread exitjmp
//
// Revision 1.186  2007/08/06 22:10:47  gsmith
// reposition process_trace for readability
//
// Revision 1.185  2007/06/23 00:04:05  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.184  2007/06/22 02:22:50  gsmith
// revert config_cpu.pat due to problems in testing
//
// Revision 1.183  2007/06/20 03:52:19  gsmith
// configure_cpu now returns when the CPU is fully configured
//
// Revision 1.182  2007/06/06 22:14:57  gsmith
// Fix SYNCHRONIZE_CPUS when numcpu > number of host processors - Greg
//
// Revision 1.181  2007/04/09 23:07:42  gsmith
// call cpu_uninit() on run_cpu exit
//
// Revision 1.180  2007/03/25 04:20:36  gsmith
// Ensure started_mask CPU bit is off for terminating cpu thread - Fish by Greg
//
// Revision 1.179  2007/03/13 01:43:37  gsmith
// Synchronize started cpu
//
// Revision 1.178  2007/01/16 01:45:33  gsmith
// Tweaks to instruction stepping/tracing
//
// Revision 1.177  2007/01/09 23:18:21  gsmith
// Tweaks to cpuloop
//
// Revision 1.176  2007/01/04 23:12:03  gsmith
// remove thunk calls for program_interrupt
//
// Revision 1.175  2007/01/04 01:08:41  gsmith
// 03 Jan 2007 single_cpu_dw fetch/store patch for ia32
//
// Revision 1.174  2007/01/03 14:21:41  rbowler
// Reinstate semantics of 'g' command changed by hsccmd rev 1.197
//
// Revision 1.173  2006/12/30 16:15:57  gsmith
// 2006 Dec 30 Fix cpu_init to call set_jump_pointers for all arches
//
// Revision 1.172  2006/12/21 22:39:38  gsmith
// 21 Dec 2006 Range for s+, t+ - Greg Smith
//
// Revision 1.171  2006/12/21 01:45:01  gsmith
// 20 Dec 2006 Fix instruction display in program interrupt - Greg Smithh
//
// Revision 1.170  2006/12/20 23:37:29  rbowler
// ip_pat cpu.c rev 1.168 duplicated 2 lines from rev 1.167
//
// Revision 1.169  2006/12/20 10:52:08  rbowler
// cpu.c(294) : warning C4101: 'ip' : unreferenced local variable
//
// Revision 1.168  2006/12/20 04:26:19  gsmith
// 19 Dec 2006 ip_all.pat - performance patch - Greg Smith
//
// Revision 1.167  2006/12/17 21:54:24  rbowler
// Display DXC in msg HHCCP014I for PIC7
//
// Revision 1.166  2006/12/08 09:43:19  jj
// Add CVS message log
//

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_CPU_C_)
#define _CPU_C_
#endif

#include "hercules.h"
#include "opcode.h"
#include "inline.h"


#if defined(COMPARE_M65) || defined(SOFTWARE_M65) || defined(HARDWARE_M65)
int current_io_num = 0;
int current_se_num = 0;
#endif


double runtime = 0.0;

/*-------------------------------------------------------------------*/
/* Put a CPU in check-stop state                                     */
/* Must hold the system intlock                                      */
/*-------------------------------------------------------------------*/
void ARCH_DEP(checkstop_cpu)(REGS *regs)
{
    regs->cpustate=CPUSTATE_STOPPING;
    regs->checkstop=1;
    ON_IC_INTERRUPT(regs);
}
/*-------------------------------------------------------------------*/
/* Put all the CPUs in the configuration in check-stop state         */
/*-------------------------------------------------------------------*/
void ARCH_DEP(checkstop_config)(void)
{
    int i;
    for(i=0;i<MAX_CPU;i++)
    {
        if(IS_CPU_ONLINE(i))
        {
            ARCH_DEP(checkstop_cpu)(sysblk.regs[i]);
        }
    }
    WAKEUP_CPUS_MASK(sysblk.waiting_mask);
}

/*-------------------------------------------------------------------*/
/* Store current PSW at a specified address in main storage          */
/*-------------------------------------------------------------------*/
void ARCH_DEP(store_psw) (REGS *regs, BYTE *addr)
{

    /* Ensure psw.IA is set */
    if (!regs->psw.zeroilc)
        SET_PSW_IA(regs);

        STORE_FW ( addr,
                   ( (regs->psw.sysmask << 24)
                   | ((regs->psw.pkey | regs->psw.states) << 16)
                   | (regs->psw.intcode)
                   )
                 );
        if(unlikely(regs->psw.zeroilc))
            STORE_FW ( addr + 4,
                   ( ( (REAL_ILC(regs) << 5)
                     | (regs->psw.cc << 4)
                     | regs->psw.progmask
                     ) << 24
                   ) | regs->psw.IA
                 );
        else
            STORE_FW ( addr + 4,
                   ( ( (REAL_ILC(regs) << 5)
                     | (regs->psw.cc << 4)
                     | regs->psw.progmask
                     ) << 24
                   ) | (regs->psw.IA & ADDRESS_MAXWRAP(regs))
                 );
#if defined(COMPARE_M65)
        if (addr < regs->mainstor + regs->mainlim)
            record_herc_write(addr - regs->mainstor, 8, addr);
#endif
} /* end function ARCH_DEP(store_psw) */

/*-------------------------------------------------------------------*/
/* Load current PSW from a specified address in main storage         */
/* Returns 0 if valid, 0x0006 if specification exception             */
/*-------------------------------------------------------------------*/
int ARCH_DEP(load_psw) (REGS *regs, BYTE *addr)
{
    INVALIDATE_AIA(regs);

    regs->psw.zeroilc = 1;

    regs->psw.sysmask = addr[0];
    regs->psw.pkey    = (addr[1] & 0xF0);
    regs->psw.states  = (addr[1] & 0x0F);

        SET_IC_BCMODE_MASK(regs);

        /* Processing for S/370 BC mode PSW */
        regs->psw.intcode = fetch_hw (addr + 2);
        regs->psw.cc = (addr[4] & 0x30) >> 4;
        regs->psw.progmask = (addr[4] & 0x0F);

        FETCH_FW(regs->psw.IA, addr + 4);
        regs->psw.IA &= 0x00FFFFFF;
        regs->psw.AMASK = AMASK24;

        regs->psw.zerobyte = 0;
        regs->psw.asc = 0;
        regs->psw.amode64 = regs->psw.amode = 0;

    regs->psw.zeroilc = 0;

    /* Check for wait state PSW */
    if (WAITSTATE(&regs->psw) && CPU_STEPPING_OR_TRACING_ALL)
    {
        logmsg (_("HHCCP043I Wait state PSW loaded: "));
        display_psw (regs);
    }

    return 0;
} /* end function ARCH_DEP(load_psw) */

#if !defined(SOFTWARE_M65) && !defined(HARDWARE_M65)
/*-------------------------------------------------------------------*/
/* Load program interrupt new PSW                                    */
/*-------------------------------------------------------------------*/
void (ATTR_REGPARM(2) ARCH_DEP(program_interrupt)) (REGS *regs, int pcode)
{
PSA    *psa;                            /* -> Prefixed storage area  */
REGS   *realregs;                       /* True regs structure       */
int     code;                           /* pcode without PER ind.    */
int     ilc;                            /* instruction length        */
#if defined(OPTION_FOOTPRINT_BUFFER)
U32     n;
#endif /*defined(OPTION_FOOTPRINT_BUFFER)*/
char    dxcstr[8]={0};                  /* " DXC=xx" if data excptn  */

static char *pgmintname[] = {
        /* 01 */        "Operation exception",
        /* 02 */        "Privileged-operation exception",
        /* 03 */        "Execute exception",
        /* 04 */        "Protection exception",
        /* 05 */        "Addressing exception",
        /* 06 */        "Specification exception",
        /* 07 */        "Data exception",
        /* 08 */        "Fixed-point-overflow exception",
        /* 09 */        "Fixed-point-divide exception",
        /* 0A */        "Decimal-overflow exception",
        /* 0B */        "Decimal-divide exception",
        /* 0C */        "HFP-exponent-overflow exception",
        /* 0D */        "HFP-exponent-underflow exception",
        /* 0E */        "HFP-significance exception",
        /* 0F */        "HFP-floating-point-divide exception",
        /* 10 */        "Segment-translation exception",
        /* 11 */        "Page-translation exception",
        /* 12 */        "Translation-specification exception",
        /* 13 */        "Special-operation exception",
        /* 14 */        "Pseudo-page-fault exception",
        /* 15 */        "Operand exception",
        /* 16 */        "Trace-table exception",
        /* 17 */        "ASN-translation exception",
        /* 18 */        "Page access exception",
        /* 19 */        "Vector/Crypto operation exception",
        /* 1A */        "Page state exception",
        /* 1B */        "Page transition exception",
        /* 1C */        "Space-switch event",
        /* 1D */        "Square-root exception",
        /* 1E */        "Unnormalized-operand exception",
        /* 1F */        "PC-translation specification exception",
        /* 20 */        "AFX-translation exception",
        /* 21 */        "ASX-translation exception",
        /* 22 */        "LX-translation exception",
        /* 23 */        "EX-translation exception",
        /* 24 */        "Primary-authority exception",
        /* 25 */        "Secondary-authority exception",
        /* 26 */        "LFX-translation exception",            /*@ALR*/
        /* 27 */        "LSX-translation exception",            /*@ALR*/
        /* 28 */        "ALET-specification exception",
        /* 29 */        "ALEN-translation exception",
        /* 2A */        "ALE-sequence exception",
        /* 2B */        "ASTE-validity exception",
        /* 2C */        "ASTE-sequence exception",
        /* 2D */        "Extended-authority exception",
        /* 2E */        "LSTE-sequence exception",              /*@ALR*/
        /* 2F */        "ASTE-instance exception",              /*@ALR*/
        /* 30 */        "Stack-full exception",
        /* 31 */        "Stack-empty exception",
        /* 32 */        "Stack-specification exception",
        /* 33 */        "Stack-type exception",
        /* 34 */        "Stack-operation exception",
        /* 35 */        "Unassigned exception",
        /* 36 */        "Unassigned exception",
        /* 37 */        "Unassigned exception",
        /* 38 */        "ASCE-type exception",
        /* 39 */        "Region-first-translation exception",
        /* 3A */        "Region-second-translation exception",
        /* 3B */        "Region-third-translation exception",
        /* 3C */        "Unassigned exception",
        /* 3D */        "Unassigned exception",
        /* 3E */        "Unassigned exception",
        /* 3F */        "Unassigned exception",
        /* 40 */        "Monitor event" };

        /* 26 */ /* was "Page-fault-assist exception", */
        /* 27 */ /* was "Control-switch exception", */

    /* If called with ghost registers (ie from hercules command
       then ignore all interrupt handling and report the error
       to the caller */
    if(regs->ghostregs)
        longjmp(regs->progjmp, pcode);

    PTT(PTT_CL_PGM,"*PROG",pcode,(U32)(regs->TEA & 0xffffffff),regs->psw.IA_L);

    /* program_interrupt() may be called with a shadow copy of the
       regs structure, realregs is the pointer to the real structure
       which must be used when loading/storing the psw, or backing up
       the instruction address in case of nullification */
    realregs = sysblk.regs[regs->cpuad];

    /* Prevent machine check when in (almost) interrupt loop */
    realregs->instcount++;

    /* Release any locks */
    if (sysblk.intowner == realregs->cpuad)
        RELEASE_INTLOCK(realregs);
    if (sysblk.mainowner == realregs->cpuad)
        RELEASE_MAINLOCK(realregs);

    /* Ensure psw.IA is set and aia invalidated */
    INVALIDATE_AIA(realregs);

    /* Set instruction length (ilc) */
    ilc = realregs->psw.zeroilc ? 0 : REAL_ILC(realregs);
    if (realregs->psw.ilc == 0 && !realregs->psw.zeroilc)
    {
        /* This can happen if BALR, BASR, BASSM or BSM
           program checks during trace */
        ilc = realregs->execflag ? realregs->exrl ? 6 : 4 : 2;
        realregs->ip += ilc;
        realregs->psw.IA += ilc;
        realregs->psw.ilc = ilc;
    }
    /* Set `execflag' to 0 in case EXecuted instruction program-checked */
    realregs->execflag = 0;

    /* Unlock the main storage lock if held */
    if (realregs->cpuad == sysblk.mainowner)
        RELEASE_MAINLOCK(realregs);

    /* Remove PER indication from program interrupt code
       such that interrupt code specific tests may be done.
       The PER indication will be stored in the PER handling
       code */
    code = pcode & ~PGM_PER_EVENT;

    /* If this is a concurrent PER event then we must add the PER
       bit to the interrupts code */
    if( OPEN_IC_PER(realregs) )
        pcode |= PGM_PER_EVENT;

    /* Perform serialization and checkpoint synchronization */
    PERFORM_SERIALIZATION (realregs);
    PERFORM_CHKPT_SYNC (realregs);

    /* Back up the PSW for exceptions which cause nullification,
       unless the exception occurred during instruction fetch */
    if ((code == PGM_PAGE_TRANSLATION_EXCEPTION
      || code == PGM_SEGMENT_TRANSLATION_EXCEPTION
      || code == PGM_TRACE_TABLE_EXCEPTION
      || code == PGM_AFX_TRANSLATION_EXCEPTION
      || code == PGM_ASX_TRANSLATION_EXCEPTION
      || code == PGM_LX_TRANSLATION_EXCEPTION
      || code == PGM_LFX_TRANSLATION_EXCEPTION                 /*@ALR*/
      || code == PGM_LSX_TRANSLATION_EXCEPTION                 /*@ALR*/
      || code == PGM_LSTE_SEQUENCE_EXCEPTION                   /*@ALR*/
      || code == PGM_EX_TRANSLATION_EXCEPTION
      || code == PGM_PRIMARY_AUTHORITY_EXCEPTION
      || code == PGM_SECONDARY_AUTHORITY_EXCEPTION
      || code == PGM_ALEN_TRANSLATION_EXCEPTION
      || code == PGM_ALE_SEQUENCE_EXCEPTION
      || code == PGM_ASTE_VALIDITY_EXCEPTION
      || code == PGM_ASTE_SEQUENCE_EXCEPTION
      || code == PGM_ASTE_INSTANCE_EXCEPTION                   /*@ALR*/
      || code == PGM_EXTENDED_AUTHORITY_EXCEPTION
      || code == PGM_STACK_FULL_EXCEPTION
      || code == PGM_STACK_EMPTY_EXCEPTION
      || code == PGM_STACK_SPECIFICATION_EXCEPTION
      || code == PGM_STACK_TYPE_EXCEPTION
      || code == PGM_STACK_OPERATION_EXCEPTION
      || code == PGM_VECTOR_OPERATION_EXCEPTION)
      && !realregs->instinvalid)
    {
        realregs->psw.IA -= ilc;
        realregs->psw.IA &= ADDRESS_MAXWRAP(realregs);
    }

    /* The OLD PSW must be incremented on the following
       exceptions during instfetch */
    if(realregs->instinvalid &&
      (  code == PGM_PROTECTION_EXCEPTION
      || code == PGM_ADDRESSING_EXCEPTION
      || code == PGM_SPECIFICATION_EXCEPTION
      || code == PGM_TRANSLATION_SPECIFICATION_EXCEPTION ))
    {
        realregs->psw.IA += ilc;
        realregs->psw.IA &= ADDRESS_MAXWRAP(realregs);
    }

    /* Store the interrupt code in the PSW */
    realregs->psw.intcode = pcode;

    /* Call debugger if active */
    HDC2(debug_program_interrupt, regs, pcode);

    /* Trace program checks other then PER event */
    if(code && (CPU_STEPPING_OR_TRACING(realregs, ilc)
        || sysblk.pgminttr & ((U64)1 << ((code - 1) & 0x3F))))
    {
     BYTE *ip;
#if defined(OPTION_FOOTPRINT_BUFFER)
        if(!(sysblk.insttrace || sysblk.inststep))
            for(n = sysblk.footprptr[realregs->cpuad] + 1 ;
                n != sysblk.footprptr[realregs->cpuad];
                n++, n &= OPTION_FOOTPRINT_BUFFER - 1)
                ARCH_DEP(display_inst)
                        (&sysblk.footprregs[realregs->cpuad][n],
                        sysblk.footprregs[realregs->cpuad][n].inst);
#endif /*defined(OPTION_FOOTPRINT_BUFFER)*/
        logmsg(_("HHCCP014I "));
#if defined(SIE_DEBUG)
        logmsg (MSTRING(_GEN_ARCH) " ");
#endif /*defined(SIE_DEBUG)*/
        if (code == PGM_DATA_EXCEPTION)
            sprintf(dxcstr, " DXC=%2.2X", regs->dxc);
        logmsg (_("CPU%4.4X: %s CODE=%4.4X ILC=%d%s\n"), realregs->cpuad,
                pgmintname[ (code - 1) & 0x3F], pcode, ilc, dxcstr);

        /* Calculate instruction pointer */
        ip = realregs->instinvalid ? NULL
           : (realregs->ip - ilc < realregs->aip)
             ? realregs->inst : realregs->ip - ilc;

        ARCH_DEP(display_inst) (realregs, ip);
    }

    realregs->instinvalid = 0;

        /* Set the main storage reference and change bits */
        STORAGE_KEY(0, regs) |= (STORKEY_REF | STORKEY_CHANGE);

        /* Point to PSA in main storage */
        psa = (void*)(regs->mainstor);

//FIXME: Why are we getting intlock here??
//      OBTAIN_INTLOCK(realregs);

        /* Store current PSW at PSA+X'28' or PSA+X'150' for ESAME */
        ARCH_DEP(store_psw) (realregs, psa->pgmold);

        /* Load new PSW from PSA+X'68' or PSA+X'1D0' for ESAME */
        if ( (code = ARCH_DEP(load_psw) (realregs, psa->pgmnew)) )
        {
            {
                logmsg (_("HHCCP016I CPU%4.4X: Program interrupt loop: "),
                          realregs->cpuad);
                display_psw (realregs);
                OBTAIN_INTLOCK(realregs);
                realregs->cpustate = CPUSTATE_STOPPING;
                ON_IC_INTERRUPT(realregs);
                RELEASE_INTLOCK(realregs);
            }
        }

//      RELEASE_INTLOCK(realregs);

        longjmp(realregs->progjmp, SIE_NO_INTERCEPT);

} /* end function ARCH_DEP(program_interrupt) */

/*-------------------------------------------------------------------*/
/* Load restart new PSW                                              */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(restart_interrupt) (REGS *regs)
{
int     rc;                             /* Return code               */
PSA    *psa;                            /* -> Prefixed storage area  */

    PTT(PTT_CL_INF,"*RESTART",regs->cpuad,regs->cpustate,regs->psw.IA_L);

    /* Set the main storage reference and change bits */
    STORAGE_KEY(0, regs) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Zeroize the interrupt code in the PSW */
    regs->psw.intcode = 0;

    /* Point to PSA in main storage */
    psa = (PSA*)(regs->mainstor);

    /* Store current PSW at PSA+X'8' or PSA+X'120' for ESAME  */
    ARCH_DEP(store_psw) (regs, psa->RSTOLD);

    /* Load new PSW from PSA+X'0' or PSA+X'1A0' for ESAME */
    rc = ARCH_DEP(load_psw) (regs, psa->RSTNEW);

    if ( rc == 0)
    {
        regs->opinterv = 0;
        regs->cpustate = CPUSTATE_STARTED;
    }

    RELEASE_INTLOCK(regs);

    if ( rc )
        regs->program_interrupt(regs, rc);

    longjmp (regs->progjmp, SIE_INTERCEPT_RESTART);
} /* end function restart_interrupt */
#endif

#if defined(COMPARE_M65) || defined(SOFTWARE_M65) || defined(HARDWARE_M65)
int     io_icode;                          /* Intercept code            */
U32     io_ioid;                           /* I/O interruption address  */
DBLWRD  io_csw;                            /* CSW for S/370 channels    */
#endif

/*-------------------------------------------------------------------*/
/* Perform I/O interrupt if pending                                  */
/* Note: The caller MUST hold the interrupt lock (sysblk.intlock)    */
/*-------------------------------------------------------------------*/
void ARCH_DEP(perform_io_interrupt) (REGS *regs)
{
int     rc;                             /* Return code               */
int     icode;                          /* Intercept code            */
PSA    *psa;                            /* -> Prefixed storage area  */
U32     ioparm;                         /* I/O interruption parameter*/
U32     ioid;                           /* I/O interruption address  */
U32     iointid;                        /* I/O interruption ident    */
RADR    pfx;                            /* Prefix                    */
DBLWRD  csw;                            /* CSW for S/370 channels    */

    /* Test and clear pending I/O interrupt */
#if defined(COMPARE_M65) || defined(SOFTWARE_M65) || defined(HARDWARE_M65)
    D_fprintf(lf, "perform_io_interrupt\n");
    icode = io_icode;
    ioid = io_ioid;
    memcpy(csw,io_csw,8);
    io_icode = 0;
    D_fprintf(lf, "perform_io_interrupt: icode=%d\n", icode);

    /* Exit if no interrupt was presented */
    if (icode == 0) return;

    PTT(PTT_CL_IO, "*IOINT", ioid, icode, 0);
#else
    icode = ARCH_DEP(present_io_interrupt) (regs, &ioid, &ioparm, &iointid, csw);

    /* Exit if no interrupt was presented */
    if (icode == 0) return;

    PTT(PTT_CL_IO, "*IOINT", ioid, ioparm, iointid);
#endif


    {
        psa = (void*)(regs->mainstor);
        STORAGE_KEY(0, regs) |= (STORKEY_REF | STORKEY_CHANGE);
    }

    /* Store the channel status word at PSA+X'40' */
    /* Channel state is supplied by the shared Hercules I/O implementation,
       so this copy is not a separate model CPU storage transaction. */
    memcpy (psa->csw, csw, 8);

    /* Set the interrupt code to the I/O device address */
    regs->psw.intcode = ioid;

#if defined(SOFTWARE_M65) || defined(HARDWARE_M65)
regs->mainstor[0x3a] = ioid >> 8;
regs->mainstor[0x3b] = ioid & 0xff;
#else
#if defined(COMPARE_M65)
    record_65_write_char(0x3a, ioid >> 8);
    record_65_write_char(0x3b, ioid & 0xff);
#endif

    /* Trace the I/O interrupt */
    if (CPU_STEPPING_OR_TRACING(regs, 0))
        logmsg (_("HHCCP044I I/O interrupt code=%4.4X "
                "CSW=%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X\n"),
                regs->psw.intcode,
                csw[0], csw[1], csw[2], csw[3],
                csw[4], csw[5], csw[6], csw[7]);

    {
        /* Store current PSW at PSA+X'38' or PSA+X'170' for ESAME */
        ARCH_DEP(store_psw) ( regs, psa->iopold );

        /* Load new PSW from PSA+X'78' or PSA+X'1F0' for ESAME */
        rc = ARCH_DEP(load_psw) ( regs, psa->iopnew );

        if ( rc )
        {
            RELEASE_INTLOCK(regs);
            regs->program_interrupt (regs, rc);
        }
    }
#endif

    RELEASE_INTLOCK(regs);

#if defined(COMPARE_M65) || defined(SOFTWARE_M65) || defined(HARDWARE_M65)
    unsigned int ioc2 = read_m65_reg(M65_REG_IO_CMD);
//    D_fprintf(lf, "  IOC: %x  IOC2: %x  RESP: %x\n", ioc, ioc2, current_io_num | 2);
    write_m65_reg(M65_REG_IO_RESP, current_io_num | 2);
#endif
#if !defined(SOFTWARE_M65) && !defined(HARDWARE_M65)
    longjmp(regs->progjmp, icode);
#endif
} /* end function perform_io_interrupt */

/*-------------------------------------------------------------------*/
/* Perform Machine Check interrupt if pending                        */
/* Note: The caller MUST hold the interrupt lock (sysblk.intlock)    */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(perform_mck_interrupt) (REGS *regs)
{
int     rc;                             /* Return code               */
PSA    *psa;                            /* -> Prefixed storage area  */
U64     mcic;                           /* Mach.check interrupt code */
U32     xdmg;                           /* External damage code      */
RADR    fsta;                           /* Failing storage address   */

    /* Test and clear pending machine check interrupt */
    rc = ARCH_DEP(present_mck_interrupt) (regs, &mcic, &xdmg, &fsta);

    /* Exit if no machine check was presented */
    if (rc == 0) return;

    /* Set the main storage reference and change bits */
    STORAGE_KEY(0, regs) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Point to the PSA in main storage */
    psa = (void*)(regs->mainstor);

    /* Store registers in machine check save area */
    ARCH_DEP(store_status) (regs, 0);

// ZZ
    /* Set the extended logout area to zeros */
    memset(psa->storepsw, 0, 16);

    /* Store the machine check interrupt code at PSA+232 */
    STORE_DW(psa->mckint, mcic);

    /* Trace the machine check interrupt */
    if (CPU_STEPPING_OR_TRACING(regs, 0))
        logmsg (_("HHCCP022I Machine Check code=%16.16" I64_FMT "u\n"),
                  (long long)mcic);

    /* Store the external damage code at PSA+244 */
    STORE_FW(psa->xdmgcode, xdmg);

    /* Store the failing storage address at PSA+248 */
    STORE_FW(psa->mcstorad, fsta);
#if defined(COMPARE_M65)
    RECORD_HERC_ABSOLUTE_FIELD(regs, psa->storepsw);
    RECORD_HERC_ABSOLUTE_FIELD(regs, psa->mckint);
    RECORD_HERC_ABSOLUTE_FIELD(regs, psa->xdmgcode);
    RECORD_HERC_ABSOLUTE_FIELD(regs, psa->mcstorad);
#endif

    /* Store current PSW at PSA+X'30' */
    ARCH_DEP(store_psw) ( regs, psa->mckold );

    /* Load new PSW from PSA+X'70' */
    rc = ARCH_DEP(load_psw) ( regs, psa->mcknew );

    RELEASE_INTLOCK(regs);

    if ( rc )
        regs->program_interrupt (regs, rc);

    longjmp (regs->progjmp, SIE_INTERCEPT_MCK);
} /* end function perform_mck_interrupt */


#if !defined(_GEN_ARCH)


REGS *s370_run_cpu (int cpu, REGS *oldregs);
static REGS *(* run_cpu[GEN_MAXARCH]) (int cpu, REGS *oldregs) =
                {
                    s370_run_cpu,
                };

/*-------------------------------------------------------------------*/
/* CPU instruction execution thread                                  */
/*-------------------------------------------------------------------*/
void *cpu_thread (int *ptr)
{
REGS *regs = NULL;
int   cpu  = *ptr;

    OBTAIN_INTLOCK(NULL);

    /* Signal cpu has started */
    signal_condition (&sysblk.cpucond);

    /* Increment number of CPUs online */
    sysblk.cpus++;

    /* Set hi CPU */
    if (cpu >= sysblk.hicpu)
        sysblk.hicpu = cpu + 1;

    /* Start the TOD clock and CPU timer thread */
    if (!sysblk.todtid)
    {
        if ( create_thread (&sysblk.todtid, DETACHED,
             timer_update_thread, NULL, "timer_update_thread") )
        {
            logmsg (_("HHCCP006S Cannot create timer thread: %s\n"),
                           strerror(errno));
            RELEASE_INTLOCK(NULL);
            return NULL;
        }
    }
    /* Set root mode in order to set priority */
    SETMODE(ROOT);

    /* Set CPU thread priority */
    if (setpriority(PRIO_PROCESS, 0, sysblk.cpuprio))
        logmsg (_("HHCCP001W CPU%4.4X thread set priority %d failed: %s\n"),
                cpu, sysblk.cpuprio, strerror(errno));

    /* Back to user mode */
    SETMODE(USER);

    /* Display thread started message on control panel */
    logmsg (_("HHCCP002I CPU%4.4X thread started: tid="TIDPAT", pid=%d, "
            "priority=%d\n"),
            cpu, thread_id(), getpid(),
            getpriority(PRIO_PROCESS,0));

    /* Execute the program in specified mode */
    do {
        regs = run_cpu[sysblk.arch_mode] (cpu, regs);
    } while (regs);

    /* Decrement number of CPUs online */
    sysblk.cpus--;

    /* Reset hi cpu */
    if (cpu + 1 >= sysblk.hicpu)
    {
        int i;
        for (i = MAX_CPU_ENGINES - 1; i >= 0; i--)
            if (IS_CPU_ONLINE(i))
                break;
        sysblk.hicpu = i + 1;
    }

    /* Signal cpu has terminated */
    signal_condition (&sysblk.cpucond);

    /* Display thread ended message on control panel */
    logmsg (_("HHCCP008I CPU%4.4X thread ended: tid="TIDPAT", pid=%d\n"),
            cpu, thread_id(), getpid());

    RELEASE_INTLOCK(NULL);

    return NULL;
}


void s370_set_jump_pointers(REGS *regs, int jump);
void s390_set_jump_pointers(REGS *regs, int jump);
void z900_set_jump_pointers(REGS *regs, int jump);

/*-------------------------------------------------------------------*/
/* Initialize a CPU                                                  */
/*-------------------------------------------------------------------*/
int cpu_init (int cpu, REGS *regs, REGS *hostregs)
{
int i;

    obtain_lock (&sysblk.cpulock[cpu]);

    regs->cpuad = cpu;
    regs->cpubit = CPU_BIT(cpu);
    regs->arch_mode = sysblk.arch_mode;
    regs->mainstor = sysblk.mainstor;
    regs->sysblk = &sysblk;
    /* 
     * ISW20060125 : LINE REMOVED : This is the job of 
     *               the INITIAL CPU RESET
     */
#if 0
    regs->psa = (PSA*)regs->mainstor;
#endif
    regs->storkeys = sysblk.storkeys;
    regs->mainlim = sysblk.mainsize - 1;
    regs->tod_epoch = get_tod_epoch();

    initialize_condition (&regs->intcond);
    regs->cpulock = &sysblk.cpulock[cpu];

    initial_cpu_reset(regs);

    if (hostregs == NULL)
    {
        regs->cpustate = CPUSTATE_STOPPING;
        ON_IC_INTERRUPT(regs);
        regs->hostregs = regs;
        regs->host = 1;
        sysblk.regs[cpu] = regs;
        sysblk.config_mask |= regs->cpubit;
        sysblk.started_mask |= regs->cpubit;
    }

#if !defined(SOFTWARE_M65) && !defined(HARDWARE_M65)
    /* Initialize opcode table pointers */
    set_opcode_pointers (regs);

#endif

    /* Set multi-byte jump code pointers */
    s370_set_jump_pointers(regs, 0);

    regs->configured = 1;

    release_lock (&sysblk.cpulock[cpu]);

    return 0;
}


/*-------------------------------------------------------------------*/
/* Uninitialize a CPU                                                */
/*-------------------------------------------------------------------*/
void *cpu_uninit (int cpu, REGS *regs)
{
    if (regs->host)
    {
        obtain_lock (&sysblk.cpulock[cpu]);
        if (regs->guestregs)
        {
            cpu_uninit (cpu, regs->guestregs);
            free (regs->guestregs);
        }
    }

    destroy_condition(&regs->intcond);

    if (regs->host)
    {
        /* Remove CPU from all CPU bit masks */
        sysblk.config_mask &= ~CPU_BIT(cpu);
        sysblk.started_mask &= ~CPU_BIT(cpu);
        sysblk.waiting_mask &= ~CPU_BIT(cpu);
        sysblk.regs[cpu] = NULL;
        release_lock (&sysblk.cpulock[cpu]);
    }

    return NULL;
}


#endif /*!defined(_GEN_ARCH)*/


#if !defined(SOFTWARE_M65) && !defined(HARDWARE_M65)
/*-------------------------------------------------------------------*/
/* Process interrupt                                                 */
/*-------------------------------------------------------------------*/
void (ATTR_REGPARM(1) ARCH_DEP(process_interrupt))(REGS *regs)
{
    /* Process PER program interrupts */
    if( OPEN_IC_PER(regs) )
        regs->program_interrupt (regs, PGM_PER_EVENT);

    /* Obtain the interrupt lock */
    OBTAIN_INTLOCK(regs);
    OFF_IC_INTERRUPT(regs);
    regs->tracing = (sysblk.inststep || sysblk.insttrace);

    /* Ensure psw.IA is set and invalidate the aia */
    INVALIDATE_AIA(regs);
    
    /* Take interrupts if CPU is not stopped */
    if (likely(regs->cpustate == CPUSTATE_STARTED))
    {
        /* Process machine check interrupt */
        if ( OPEN_IC_MCKPENDING(regs) )
        {
            PERFORM_SERIALIZATION (regs);
            PERFORM_CHKPT_SYNC (regs);
            ARCH_DEP (perform_mck_interrupt) (regs);
        }

        /* Process external interrupt */
        if ( OPEN_IC_EXTPENDING(regs) )
        {
            PERFORM_SERIALIZATION (regs);
            PERFORM_CHKPT_SYNC (regs);
            ARCH_DEP (perform_external_interrupt) (regs);
        }

#if !defined(COMPARE_M65) && !defined(SOFTWARE_M65)
        /* Process I/O interrupt */
        if (IS_IC_IOPENDING)
        {
            if ( OPEN_IC_IOPENDING(regs) )
            {
                PERFORM_SERIALIZATION (regs);
                PERFORM_CHKPT_SYNC (regs);
                ARCH_DEP (perform_io_interrupt) (regs);
            }
            else
                WAKEUP_CPU_MASK(sysblk.waiting_mask);
        }
#endif
    } /*CPU_STARTED*/

    /* If CPU is stopping, change status to stopped */
    if (unlikely(regs->cpustate == CPUSTATE_STOPPING))
    {
        /* Change CPU status to stopped */
        regs->opinterv = 0;
        regs->cpustate = CPUSTATE_STOPPED;

        /* Thread exit (note - intlock still held) */
        if (!regs->configured)
            longjmp(regs->exitjmp, SIE_NO_INTERCEPT);

        /* If initial CPU reset pending then perform reset */
        if (regs->sigpireset)
        {
            PERFORM_SERIALIZATION (regs);
            PERFORM_CHKPT_SYNC (regs);
            ARCH_DEP (initial_cpu_reset) (regs);
            RELEASE_INTLOCK(regs);
            longjmp(regs->progjmp, SIE_NO_INTERCEPT);
        }

        /* If a CPU reset is pending then perform the reset */
        if (regs->sigpreset)
        {
            PERFORM_SERIALIZATION (regs);
            PERFORM_CHKPT_SYNC (regs);
            ARCH_DEP(cpu_reset) (regs);
            RELEASE_INTLOCK(regs);
            longjmp(regs->progjmp, SIE_NO_INTERCEPT);
        }

        /* Store status at absolute location 0 if requested */
        if (IS_IC_STORSTAT(regs))
        {
            OFF_IC_STORSTAT(regs);
            ARCH_DEP(store_status) (regs, 0);
            logmsg (_("HHCCP010I CPU%4.4X store status completed.\n"),
                    regs->cpuad);
            /* ISW 20071102 : Do not return via longjmp here. */
            /*    process_interrupt needs to finish putting the */
            /*    CPU in its manual state                     */
            /*
            RELEASE_INTLOCK(regs);
            longjmp(regs->progjmp, SIE_NO_INTERCEPT);
            */
        }
    } /*CPUSTATE_STOPPING*/

    /* Perform restart interrupt if pending */
    if ( IS_IC_RESTART(regs) )
    {
        PERFORM_SERIALIZATION (regs);
        PERFORM_CHKPT_SYNC (regs);
        OFF_IC_RESTART(regs);
        ARCH_DEP(restart_interrupt) (regs);
    } /* end if(restart) */

    /* This is where a stopped CPU will wait */
    if (unlikely(regs->cpustate == CPUSTATE_STOPPED))
    {
        S64 saved_timer = cpu_timer(regs);
        regs->ints_state = IC_INITIAL_STATE;
        sysblk.started_mask ^= regs->cpubit;
        sysblk.intowner = LOCK_OWNER_NONE;

        /* Wait while we are STOPPED */
#if defined(COMPARE_M65)
        /* A panel pause can arrive during a generated CPU microinstruction.
           Preserve that call until Start; abandoning it leaves the two CPUs
           at different instruction boundaries. Reset/restart still use the
           normal interrupt path. Repeated Stop commands must not resume it. */
        do {
            wait_condition (&regs->intcond, &sysblk.intlock);
        } while (regs->cpustate != CPUSTATE_STARTED && regs->configured
                 && !regs->sigpireset && !regs->sigpreset
                 && !IS_IC_RESTART(regs) && !IS_IC_STORSTAT(regs));
#else
        wait_condition (&regs->intcond, &sysblk.intlock);
#endif

        /* Wait while SYNCHRONIZE_CPUS is in progress */
        while (sysblk.syncing)
            wait_condition (&sysblk.sync_bc_cond, &sysblk.intlock);

        sysblk.intowner = regs->cpuad;
        sysblk.started_mask |= regs->cpubit;
        regs->ints_state |= sysblk.ints_state;
        set_cpu_timer(regs,saved_timer);

        ON_IC_INTERRUPT(regs);

#if defined(COMPARE_M65)
        int resume_m65 = regs->cpustate == CPUSTATE_STARTED && regs->configured
            && !regs->sigpireset && !regs->sigpreset
            && !IS_IC_RESTART(regs) && !IS_IC_STORSTAT(regs);
#endif
        RELEASE_INTLOCK(regs);
#if defined(COMPARE_M65)
        if (resume_m65)
            return;
#endif
        longjmp(regs->progjmp, SIE_NO_INTERCEPT);
    } /*CPUSTATE_STOPPED*/

#if !defined(COMPARE_M65)
    /* Test for wait state */
    if (WAITSTATE(&regs->psw))
    {
#ifdef OPTION_MIPS_COUNTING
        regs->waittod = host_tod();
#endif

        /* Test for disabled wait PSW and issue message */
        if( IS_IC_DISABLED_WAIT_PSW(regs) )
        {
            logmsg (_("HHCCP011I CPU%4.4X: Disabled wait state\n"
                      "          "),
                    regs->cpuad);
            display_psw (regs);
            regs->cpustate = CPUSTATE_STOPPING;
            RELEASE_INTLOCK(regs);
            longjmp(regs->progjmp, SIE_NO_INTERCEPT);
        }

        /* Indicate we are giving up intlock */
        sysblk.intowner = LOCK_OWNER_NONE;
        sysblk.waiting_mask |= regs->cpubit;

        /* Wait for interrupt */
        wait_condition (&regs->intcond, &sysblk.intlock);
        /* Wait while SYNCHRONIZE_CPUS is in progress */
        while (sysblk.syncing)
            wait_condition (&sysblk.sync_bc_cond, &sysblk.intlock);

        /* Indicate we now own intlock */
        sysblk.waiting_mask ^= regs->cpubit;
        sysblk.intowner = regs->cpuad;

#ifdef OPTION_MIPS_COUNTING
        /* Calculate the time we waited */
        regs->waittime += host_tod() - regs->waittod;
        regs->waittod = 0;
#endif
        RELEASE_INTLOCK(regs);
        longjmp(regs->progjmp, SIE_NO_INTERCEPT);
    } /* end if(wait) */
#endif

    /* Release the interrupt lock */
    RELEASE_INTLOCK(regs);
    return;

} /* process_interrupt */
#endif

#if defined(COMPARE_M65) || defined(SOFTWARE_M65) || defined(HARDWARE_M65)

int iointnum = 0;

void process_memory(REGS* regs) {
    unsigned int sec = read_m65_reg(M65_REG_SE_CMD);
    if ((current_se_num ^ sec) & 0xc0000000) {
        current_se_num = sec & 0xc0000000;
        unsigned int response = current_se_num;
        unsigned int sea = read_m65_reg(M65_REG_SE_ADDR);

        if (mon_stor)
            D_fprintf(lf, "SEC: %08x  SEA: %08x\n", sec, sea);
        fflush(lf);
        if (!(sec & 8)) {
            // not cancelled
            if (sec & 0x10) {
                // TS returns the original doubleword and sets marked bytes.
                // The storage unit performs this operation, not the CPU ALDs.
                unsigned int address = sea & 0x007ffff8;
                unsigned __int64 data;
                OBTAIN_MAINLOCK(regs);
                memcpy(&data, &sysblk.mainstor[address], sizeof(data));
#if defined(COMPARE_M65)
                // Native Hercules must still see the original byte for CC.
                // Record the model's write; native TS updates shared storage.
                record_65_write(sea, 0xffffffff, 0xffffffff);
#else
                for (int i = 0; i < 8; i++) {
                    if (sea & (0x80000000U >> i))
                        sysblk.mainstor[address + i] = 0xff;
                }
#endif
                write_m65_reg(M65_REG_SE_RDATA_HI, htonl(data & 0xffffffff));
                write_m65_reg(M65_REG_SE_RDATA_LO, htonl(data >> 32));
                RELEASE_MAINLOCK(regs);
            }
            else if (sec & 1) {
                // write
                unsigned int wh = read_m65_reg(M65_REG_SE_WDATA_HI);
                unsigned int wl = read_m65_reg(M65_REG_SE_WDATA_LO);
#if defined(COMPARE_M65)
                record_65_write(sea, wh, wl);
                if (sea == 0xf0000050) {
                    sysblk.mainstor[0x50] = (wh >> 24) & 255;
                    sysblk.mainstor[0x51] = (wh >> 16) & 255;
                    sysblk.mainstor[0x52] = (wh >> 8) & 255;
                    sysblk.mainstor[0x53] = wh & 255;
                    record_herc_write(0x50, 4, &(sysblk.mainstor[0x50]));
                }
#else
                for (int i = 0; i < 4; i++) {
                    if (sea & (1 << (31 - i)))
                        sysblk.mainstor[(sea & 0x7ffff8) + i] = (wh >> ((3 - i) * 8)) & 0xff;
                    if (sea & (1 << (27 - i)))
                        sysblk.mainstor[(sea & 0x7ffff8) + 4 + i] = (wl >> ((3 - i) * 8)) & 0xff;
                }
#endif
            }
            else if (sec & 2) {
                // set key
#if defined(COMPARE_M65)
                record_65_set_key(sec, sea);
#else
                /* Update the storage key from R1 register bits 24-30 */
                STORAGE_KEY(sea & 0x7ffff0, regs) &= STORKEY_BADFRM;
                STORAGE_KEY(sea & 0x7ffff0, regs) |= ((sec >> 22) & 0xf8) & ~(STORKEY_BADFRM);
                STORKEY_INVALIDATE(regs, sea & 0x7ffff0);
#endif
            }
            else if (sec & 4) {
                /* ISK returns the five Model-65 key bits, not a data word.
                   Keep key and valid together with the completed sequence.
                   WA gates key advance only for that matching live request. */
                unsigned int key = STORAGE_KEY(sea & 0x007ff800, regs) & 0xf8;
                response |= (key << 22) | 0x01000000;
            }
            else {
                // read
                unsigned __int64 data = *(unsigned __int64*)&(sysblk.mainstor[sea & 0x007ffff8]);
                write_m65_reg(M65_REG_SE_RDATA_HI, htonl(data & 0xffffffff));
                write_m65_reg(M65_REG_SE_RDATA_LO, htonl(data >> 32));
            }
        }
        write_m65_reg(M65_REG_SE_RESP, response);
    }
}

void twenty_cycle(REGS * regs) {
    for (int i = 0; i < 20; i++) {
        single_cycle();
        if (i >= 17)
            process_memory(regs);
    }
    if (newstate.RX.rosar.F == 0) {
        single_cycle();
    }
    if (newstate.RX.rosar.F == 0) {
        single_cycle();
    }
    if (newstate.RX.rosar.F == 0) {
        single_cycle();
    }
    if (newstate.RX.rosar.F == 0) {
        single_cycle();
    }
    if (newstate.RX.rosar.F == 0) {
        single_cycle();
    }
    if (newstate.RX.rosar.F == 0) {
        single_cycle();
    }

    if (!mon_20) return;
    D_fprintf(lf, "ROSAR: %03x  IC: %06x  D: %06x  AB: %01llx%016llx  ST: %016llx  F: %02x  ABC: %01x  STC: %01x  PAL: %016llx  SAL: %02x  SAB: %06x  SDBI: %016llx  SDBO: %016llx\n"
        , newstate.RX.rosar.F, newstate.CA.ic.F, newstate.RD.d_bit.F, newstate.RA_RB.ab_bit.F2, newstate.RA_RB.ab_bit.F, newstate.RS_RT.st_bit.F, newstate.RF.f_bit.F, newstate.CW_INT.abc_tgr.F, newstate.CS_INT.stc_tgr.F, newstate.AP.paddl.F, newstate.AS.saddl.F, newstate.MA.sab.F, newstate.MB.sdbi.F, newstate.WA.sdbo.F);
    D_fprintf(lf, "Q: %016llx  R: %04x  E: %04x\n"
        , newstate.RQ.q.F, newstate.RR.r.F, newstate.RE.e_bit.F);
    for (int i = 0; i < 25; i++) {
        D_fprintf(lf, " %x", newstate.ls_mem[i].F);
    }
    D_fprintf(lf, "\n");
}

int run_single_instruction(REGS * regs) {
    static int d;
    static bool writing;
    static bool cancelled;
    static bool reported_wait = false;
    static unsigned int wait_cycles = 0;
    int ic;
    bool ic_set = false;
    bool skip_exit = false;
    bool can_exit = false;
    bool delayed_exit = false;
    ic = newstate.CA.ic.F;
    for(int z=0;;z++) {
#if !defined(SOFTWARE_M65) && !defined(HARDWARE_M65)
        if (z % 100 == 99) {
            if (INTERRUPT_PENDING(regs))
#if defined(COMPARE_M65)
            {
                /* Hercules has fetched this instruction but has not executed
                   it yet. A normal interrupt-handler return must preserve
                   its address cache, so Resume does not fetch it twice. */
                BYTE *saved_aie = regs->aie;
                ARCH_DEP(process_interrupt)(regs);
                regs->aie = saved_aie;
            }
#else
                ARCH_DEP(process_interrupt)(regs);
#endif
        }
#endif        //        if (z == 10000) exit(1);
        twenty_cycle(regs);
        /* Console output can precede WTOR setup on this slow model. Expose
           a settled architectural wait so the operator helper can defer
           automatic replies until all runnable OS work has completed. */
        if (newstate.RW.psw_bit.B14 && newstate.RX.rosar.F == 0x784) {
            if (wait_cycles < 32) wait_cycles++;
            if (wait_cycles == 32 && !reported_wait) {
                D_fprintf(lf, "M65WAIT entered ic=%06x\n", newstate.CA.ic.F);
                fflush(lf);
                reported_wait = true;
            }
        } else {
            wait_cycles = 0;
            if (reported_wait) {
                D_fprintf(lf, "M65WAIT left\n");
                fflush(lf);
                reported_wait = false;
            }
        }
        if (!ic_set) {
            ic_set = true;
            ic = newstate.CA.ic.F;
        }

//        process_memory();

        unsigned int ioc = read_m65_reg(M65_REG_IO_CMD);
        if ((current_io_num ^ ioc)&0xc0000000) {
            // new io
            current_io_num = ioc & 0xc0000000;
            unsigned int ext = read_m65_reg(M65_REG_EXT);
            int devnum = (ext & 0xf00) | ((ext >> 24) & 0xff);
            if (ioc & 1) {
                // I/O operation
                D_fprintf(lf, "IOCE OPERATION: %08x:%08x\n", ioc, ext);

#if defined(COMPARE_M65)
                record_65_io(ext);
                return -ic;
#else
                PSA* psa;                            /* -> prefixed storage area  */
                DEVBLK* dev;                            /* -> device block for SIO   */
                ORB     orb;                            /* Operation request blk @IZW*/
                VADR    ccwaddr;                        /* CCW address for start I/O */
                BYTE    ccwkey;                         /* Bits 0-3=key, 4=7=zeroes  */
                dev = find_device_by_devnum(regs->chanset, devnum);
                switch (ext & 7) {
                case 4:
                    PRIV_CHECK(regs);

                    /* Locate the device block */
                    if (!dev) {
                        regs->psw.cc = 3;
                    } else {
                        /* Fetch key and CCW address from the CAW at PSA+X'48' */
                        psa = (PSA*)(regs->mainstor);
                        ccwkey = psa->caw[0] & 0xF0;
                        ccwaddr = (psa->caw[1] << 16) | (psa->caw[2] << 8)
                            | psa->caw[3];

                        /* Build the I/O operation request block */                /*@IZW*/
                        memset(&orb, 0, sizeof(ORB));                             /*@IZW*/
                        orb.flag4 = ccwkey & ORB4_KEY;                             /*@IZW*/
                        STORE_FW(orb.ccwaddr, ccwaddr);                             /*@IZW*/

                        /* Start the channel program and set the condition code */
                        regs->psw.cc = ARCH_DEP(startio) (regs, dev, &orb);        /*@IZW*/

                        regs->siocount++;
                    }
                    break;
                case 5:
                    /* Locate the device block */
                    if (!dev) {
                        regs->psw.cc = 3;
                    } else {

                        /* Test the device and set the condition code */
                        regs->psw.cc = testio(regs, dev, 0); /* CAVA VERIFY 0 */
                        /* Yield time slice so that device handler may get some time */
                        /* to possibly complete an I/O - to prevent a TIO Busy Loop  */
                        if (regs->psw.cc == 2)
                        {
                            sched_yield();
                        }
                    }
                    break;
                case 7:
                    /* Test for pending interrupt and set condition code */
                    regs->psw.cc = testch(regs, ext & 0x0F00);
                    break;
                default:
                    D_fprintf(lf, "Unknown IO instruction: %08x", ext);
                    exit(-1);
                }
                write_m65_reg(M65_REG_IO_RESP, current_io_num | regs->psw.cc);
#endif
            }
#if !defined(COMPARE_M65)
            else if (ioc & 2) {
                D_fprintf(lf, "IPL OPERATION: %08x:%08x\n", ioc, ext);
                // IPL
                regs->psw.cc = s370_load_ipl(0, devnum, sysblk.pcpu, 0);
            }
#endif
            else if (ioc & 4) {
                // permit i-o interrupt
                D_fprintf(lf, "Permit i-o interrupt\n");
                iointnum++;
                io_icode = ARCH_DEP(present_io_interrupt)(regs, &io_ioid, NULL, NULL, io_csw);
                ARCH_DEP(perform_io_interrupt) (regs);
            }
            else
                D_fprintf(lf, "IOCE CMD: %x\n", ioc);
        }

        if (((newstate.RX.rosar.F == 0x02e) /* store in progress */ || (newstate.RX.rosar.F == 0x004) /* program store compare */ 
            || (newstate.RX.rosar.F == 0x014) /* timer clock step */
            || (newstate.RX.rosar.F == 0x02a) || (newstate.RX.rosar.F == 0x00e)) && !oldstate.KD.execute_in_progress_tgr_be4)
            skip_exit = true;
        if ((newstate.RX.rosar.F >= 0x200 && newstate.RX.rosar.F < 0x208) ||
            (newstate.RX.rosar.F >= 0x210 && newstate.RX.rosar.F < 0x228) ||
            (newstate.RX.rosar.F >= 0x230 && newstate.RX.rosar.F < 0x238) ||
            delayed_exit) {
            if (!skip_exit) {
                if (can_exit) {
                    if (oldstate.KD.execute_in_progress_tgr_be4) {
                        if (mon_20) D_fprintf(lf, " *** EXIT PREVENTED BY EXEC TGR ***\n");
                        delayed_exit = false;
                        skip_exit = false;
                    } else if ((!oldstate.RX_INT._temp_block_rosar_reset) || (!newstate.MC._select_se_1_even && (newstate.WA_INT.store || newstate.WA_INT.set_key))) {
                        delayed_exit = true;
                    } else {
                        if (mon_20) D_fprintf(lf, " ==> IC : %06x    R : %04x    E: %04x\n", newstate.CA.ic.F, newstate.RR.r.F, newstate.RE.e_bit.F);
                        if (mon_20) D_fprintf(lf, " *** EXIT ***\n");
                        return ic;
                    }
                }
                else {
                    if (mon_20) D_fprintf(lf, " *** CANNOT EXIT ***\n");
                }
            }
            else {
                ic_set = false;
                skip_exit = false;
                if (mon_20) D_fprintf(lf, " *** EXIT SKIPPED ***\n");
            }
        }
        if (newstate.RX.rosar.F == 0x180) {
            D_fprintf(lf, "\n\n\n*** *** *** INVALID INSTRUCTION! *** *** ***\n\n\n");
            D_fprintf(lf, "\n\n\n*** *** *** INVALID INSTRUCTION! *** *** ***\n\n\n");
            D_fprintf(lf, "\n\n\n*** *** *** INVALID INSTRUCTION! *** *** ***\n\n\n");
//            exit(1);
        }
        can_exit = true;
    }
}
#endif



int instnum = 0;


/*-------------------------------------------------------------------*/
/* Run CPU                                                           */
/*-------------------------------------------------------------------*/
REGS *ARCH_DEP(run_cpu) (int cpu, REGS *oldregs)
{
BYTE   *ip;
REGS    regs;
int ic;

#if defined(COMPARE_M65) || defined(SOFTWARE_M65)
logging = true;
lf = fopen("m65.log", "w");

    full_init();
    write_m65_reg(M65_REG_CFG, sysblk.mainsize - 1);
    for (int i = 0; i < 50; i++) {
        twenty_cycle(&regs);
    }
    D_fprintf(lf, "M65TIMER disable_key=%d clock_enable=%d\n",
        newstate.PK_PL.disable_timer_key, newstate.KW._disable_time_clock);
    D_fprintf(lf, "\n\nINIT COMPLETE\n\n\n");
#endif
    if (oldregs)
    {
        memcpy (&regs, oldregs, sizeof(REGS));
        free (oldregs);
        regs.hostregs = &regs;
        if (regs.guestregs)
            regs.guestregs->hostregs = &regs;
        sysblk.regs[cpu] = &regs;
        release_lock(&sysblk.cpulock[cpu]);
        logmsg (_("HHCCP007I CPU%4.4X architecture mode set to %s\n"),
                cpu, get_arch_mode_string(&regs));
    }
    else
    {
        memset (&regs, 0, sizeof(REGS));

        if (cpu_init (cpu, &regs, NULL))
            return NULL;

        logmsg (_("HHCCP003I CPU%4.4X architecture mode %s\n"),
                cpu, get_arch_mode_string(&regs));
    }

#if !defined(SOFTWARE_M65) && !defined(HARDWARE_M65)
    regs.program_interrupt = &ARCH_DEP(program_interrupt);
#endif

    regs.tracing = (sysblk.inststep || sysblk.insttrace);
    regs.ints_state |= sysblk.ints_state;

    /* Establish longjmp destination for cpu thread exit */
    if (setjmp(regs.exitjmp))
        return cpu_uninit(cpu, &regs);

    RELEASE_INTLOCK(&regs);

    /* Establish longjmp destination for program check */
    setjmp(regs.progjmp);

    /* Set `execflag' to 0 in case EXecuted instruction did a longjmp() */
    regs.execflag = 0;

    do {
#if !defined(SOFTWARE_M65) && !defined(HARDWARE_M65)
        if (INTERRUPT_PENDING(&regs))
            ARCH_DEP(process_interrupt)(&regs);

        ip = INSTRUCTION_FETCH(&regs, 0);
        int ia = PSW_IA24(&regs, 0);
        regs.instcount++;
#endif

#if defined(COMPARE_M65) || defined(SOFTWARE_M65)
        if (0 && instnum==18000) //0 && /*(ip[0]==0xd2) && (ia==0x47a) && (newstate.ls_mem[12].F == 0x10618) &&*/ (iointnum==74) && ip[0]==0x82)
        {
            mon_cycle = true;
//            mon_ros = true;
//            mon_stor = true;
            mon_20 = true;
        }
        ic = run_single_instruction(&regs);
#endif
#if !defined(SOFTWARE_M65) && !defined(HARDWARE_M65)
        EXECUTE_INSTRUCTION(ip, &regs);
#endif
#if defined(COMPARE_M65)
        if (ic < 0) {
            write_m65_reg(M65_REG_IO_RESP, current_io_num | regs.psw.cc);
            run_single_instruction(&regs);
            ic = -ic;
        }
        else {
            for (int i = 0; i < 5 && /*((ia == 0x13c) || (ia == 0xf961c) || (ia == 0x2ea) || (ia == 0x360) || (ia == 0x244)) &&*/ (ic != ia + 8) && (ic != ia + 16); i++) {
                D_fprintf(lf, "Potential IC mismatch: HERC=%x, M65=%x\n", ia, ic);
                ic = run_single_instruction(&regs);
            }
        }
#endif
#if defined(SOFTWARE_M65)
        instnum++;
        D_fprintf(lf, "%06x %7d(%f)(%3d) ", ic, instnum, runtime, iointnum);
#endif
#if defined(COMPARE_M65)
        instnum++;
        D_fprintf(lf, "%06x(%06x) %7d(%f)(%3d) %02x%02x", ia, ic, instnum, 0.0f/*runtime*/, iointnum, ip[0], ip[1]);
        if (ip[0] & 0xc0) { D_fprintf(lf, ".%02x%02x", ip[2], ip[3]); }
        else { D_fprintf(lf, "     "); }
        if ((ip[0] & 0xc0) == 0xc0) { D_fprintf(lf, ".%02x%02x", ip[4], ip[5]); }
        else { D_fprintf(lf, "     "); }
#endif
#if defined(SOFTWARE_M65) || defined(COMPARE_M65)
        for (int i = 0; i < 16; i++) {
            D_fprintf(lf, " %08x", newstate.ls_mem[i].F);
        }
        D_fprintf(lf, "\n");
#endif
#if defined(COMPARE_M65)
#define RETRY do { if (max_retries) { max_retries--; single_cycle(); goto retry; } } while (0)
        int max_retries = 0;
retry:

        if ((ic != ia + 8) && (ic != ia + 16)) {
            D_fprintf(lf, "Execution gone astray! IC does not match: HERC=%x, M65=%x\n", ia, ic);
            max_retries = 0;
        }
        int sysm = newstate.RW.psw_bit.F >> 32;
        if (sysm != regs.psw.sysmask) {
            D_fprintf(lf, "Execution gone astray! SYSMASK does not match: HERC=%x, M65=%x\n", regs.psw.sysmask, sysm);
            max_retries = 0;
        }
        int cond = newstate.RW.psw_bit.B34 * 2 + newstate.RW.psw_bit.B35;
        if (cond != regs.psw.cc) {
            RETRY;
            D_fprintf(lf, "Execution gone astray! CC does not match: HERC=%x, M65=%x\n", regs.psw.cc, cond);
        }
        for (int i = 0; i < 16; i++) {
            if (newstate.ls_mem[i].F != regs.gr[i].F.L.F) {
                RETRY;
                D_fprintf(lf, "Execution gone astray! GR%d does not match: HERC=%x, M65=%x\n", i, regs.gr[i].F.L.F, newstate.ls_mem[i].F);
            }
        }
        write_compare();
        io_compare();
#endif
#if defined(SOFTWARE_M65) || defined(COMPARE_M65)
        fflush(lf);
#endif
        //if (instnum == 21000)
        //    exit(1);


    } while (1);

    /* Never reached */
    return NULL;

} /* end function cpu_thread */

/*-------------------------------------------------------------------*/
/* Process Trace                                                     */
/*-------------------------------------------------------------------*/
void ARCH_DEP(process_trace)(REGS *regs)
{
int     shouldtrace = 0;                /* 1=Trace instruction       */
int     shouldstep = 0;                 /* 1=Wait for start command  */

    /* Test for trace */
    if (CPU_TRACING(regs, 0))
        shouldtrace = 1;

    /* Test for step */
    if (CPU_STEPPING(regs, 0))
        shouldstep = 1;

#if !defined(SOFTWARE_M65) && !defined(HARDWARE_M65)
    /* Display the instruction */
    if (shouldtrace || shouldstep)
    {
        BYTE *ip = regs->ip < regs->aip ? regs->inst : regs->ip;
        ARCH_DEP(display_inst) (regs, ip);
    }
#endif

    /* Stop the CPU */
    if (shouldstep)
    {
        REGS *hostregs = regs->hostregs;
        S64 saved_timer[2];

        OBTAIN_INTLOCK(hostregs);
#ifdef OPTION_MIPS_COUNTING
        hostregs->waittod = host_tod();
#endif
        /* The CPU timer is not decremented for a CPU that is in
           the manual state (e.g. stopped in single step mode) */
        saved_timer[0] = cpu_timer(regs);
        saved_timer[1] = cpu_timer(hostregs);
        hostregs->cpustate = CPUSTATE_STOPPED;
        sysblk.started_mask &= ~hostregs->cpubit;
        hostregs->stepwait = 1;
        sysblk.intowner = LOCK_OWNER_NONE;
        while (hostregs->cpustate == CPUSTATE_STOPPED)
        {
            wait_condition (&hostregs->intcond, &sysblk.intlock);
        }
        sysblk.intowner = hostregs->cpuad;
        hostregs->stepwait = 0;
        sysblk.started_mask |= hostregs->cpubit;
        set_cpu_timer(regs,saved_timer[0]);
        set_cpu_timer(hostregs,saved_timer[1]);
#ifdef OPTION_MIPS_COUNTING
        hostregs->waittime += host_tod() - hostregs->waittod;
        hostregs->waittod = 0;
#endif
        RELEASE_INTLOCK(hostregs);
    }
} /* process_trace */

/*-------------------------------------------------------------------*/
/* Set Jump Pointers                                                 */
/*                                                                   */
/* For supported architectures and certain multi-byte instructions,  */
/* EXECUTE_INSTRUCTION and UNROLLED_EXECUTE call a label in this     */
/* function which does a jump to the real instruction.               */
/*                                                                   */
/* The reason why we use labels instead of individual pointers is    */
/* that if -fomit-frame-pointer is omitted then the backframe        */
/* isn't pushed onto the stack.                                      */
/*                                                                   */
/* The reason why this routine is in cpu.c is an attempt to provide  */
/* locality with the corresponding run_cpu function.                 */
/*                                                                   */
/* This routine is called from cpu_init                              */
/*                                                                   */
/*-------------------------------------------------------------------*/
void ARCH_DEP(set_jump_pointers) (REGS *regs, int jump)
{

#if defined(MULTI_BYTE_ASSIST)

    /* Use `switch' to confuse smart-ass optimizing compilers */
    switch (jump) {

 #if defined(MULTI_BYTE_ASSIST_IA32)
    case 0xa7:
jump_a7xx:
 __asm__ (
        "movzbl 1(%%eax),%%ecx\n\t"
        "jmp    *%c0(%%edx,%%ecx,4)"
        : : "i" (offsetof(REGS,ARCH_DEP(opcode_a7xx)))
        );
        return;
    case 0xb2:
jump_b2xx:
 __asm__ (
        "movzbl 1(%%eax),%%ecx\n\t"
        "jmp    *%c0(%%edx,%%ecx,4)"
        : : "i" (offsetof(REGS,ARCH_DEP(opcode_b2xx)))
        );
        return;
    case 0xb9:
jump_b9xx:
 __asm__ (
        "movzbl 1(%%eax),%%ecx\n\t"
        "jmp    *%c0(%%edx,%%ecx,4)"
        : : "i" (offsetof(REGS,ARCH_DEP(opcode_b9xx)))
        );
        return;
jump_ebxx:
 __asm__ (
        "movzbl 5(%%eax),%%ecx\n\t"
        "jmp    *%c0(%%edx,%%ecx,4)"
        : : "i" (offsetof(REGS,ARCH_DEP(opcode_ebxx)))
        );
        return;
 #endif /* defined(MULTI_BYTE_ASSIST_IA32) */

    } /* switch(jump) */

    regs->ARCH_DEP(opcode_table)[0xa7] = &&jump_a7xx;
    regs->ARCH_DEP(opcode_table)[0xb2] = &&jump_b2xx;
    regs->ARCH_DEP(opcode_table)[0xb9] = &&jump_b9xx;
    regs->ARCH_DEP(opcode_table)[0xeb] = &&jump_ebxx;

#else /* !defined(MULTI_BYTE_ASSIST) */
    UNREFERENCED(regs);
    UNREFERENCED(jump);
#endif /* !defined(MULTI_BYTE_ASSIST) */

}

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "cpu.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "cpu.c"
#endif

/*-------------------------------------------------------------------*/
/* Copy program status word                                          */
/*-------------------------------------------------------------------*/
DLL_EXPORT void copy_psw (REGS *regs, BYTE *addr)
{
REGS cregs;

    memcpy(&cregs, regs, sysblk.regs_copy_len);

            s370_store_psw(&cregs, addr);
} /* end function copy_psw */

/*-------------------------------------------------------------------*/
/* Display program status word                                       */
/*-------------------------------------------------------------------*/
void display_psw (REGS *regs)
{
QWORD   qword;                            /* quadword work area      */

    memset(qword, 0, sizeof(qword));

        copy_psw (regs, qword);
        logmsg (_("PSW=%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X\n"),
                qword[0], qword[1], qword[2], qword[3],
                qword[4], qword[5], qword[6], qword[7]);

} /* end function display_psw */

const char* arch_name[GEN_MAXARCH] =
{
        _ARCH_370_NAME,
};

const char* get_arch_mode_string(REGS* regs)
{
    if (!regs) return arch_name[sysblk.arch_mode];
    else return arch_name[regs->arch_mode];
}

#endif /*!defined(_GEN_ARCH)*/
