/* VSTORE.H     (c) Copyright Roger Bowler, 1999-2009                */
/*              ESA/390 Virtual Storage Functions                    */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2009      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2009      */

// $Id: vstore.h 5401 2009-06-08 03:54:27Z fish $

#if defined(COMPARE_M65)
#include "360_cstruc.h"
#endif
/*-------------------------------------------------------------------*/
/* This module contains various functions which store, fetch, and    */
/* copy values to, from, or between virtual storage locations.       */
/*                                                                   */
/* Functions provided in this module are:                            */
/* vstorec      Store 1 to 256 characters into virtual storage       */
/* vstoreb      Store a single byte into virtual storage             */
/* vstore2      Store a two-byte integer into virtual storage        */
/* vstore4      Store a four-byte integer into virtual storage       */
/* vstore8      Store an eight-byte integer into virtual storage     */
/* vfetchc      Fetch 1 to 256 characters from virtual storage       */
/* vfetchb      Fetch a single byte from virtual storage             */
/* vfetch2      Fetch a two-byte integer from virtual storage        */
/* vfetch4      Fetch a four-byte integer from virtual storage       */
/* vfetch8      Fetch an eight-byte integer from virtual storage     */
/* instfetch    Fetch instruction from virtual storage               */
/* move_chars   Move characters using specified keys and addrspaces  */
/* move_charx   Move characters with optional specifications         */
/* validate_operand   Validate addressing, protection, translation   */
/*-------------------------------------------------------------------*/
/* And provided by means of macro's address wrapping versions of     */
/* the above:                                                        */
/* wstoreX                                                           */
/* wfetchX                                                           */
/* wmove_chars                                                       */
/* wvalidate_operand                                                 */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.85  2009/01/23 13:13:46  bernard
// copyright notice
//
// Revision 1.84  2009/01/17 17:02:15  jj
// Fix change recording on page crossing first op of MVC - Reported by Greg Price
//
// Revision 1.83  2008/04/02 21:52:19  rbowler
// Fix PIC4 when MVCOS operand finishes on page boundary
//
// Revision 1.82  2008/03/23 06:13:07  rbowler
// Add MVCOS instruction (part 3)
//
// Revision 1.81  2008/03/23 03:03:45  gsmith
// 22 Mar 2008 fix unbalanced comment - Peter J Farley III - by Greg
//
// Revision 1.80  2008/03/16 00:09:57  rbowler
// Add MVCOS instruction (part 2)
//
// Revision 1.79  2008/02/20 23:47:22  ptl00
// Fix branch to odd address so pgm old is bumped
//
// Revision 1.78  2007/06/23 00:04:19  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.77  2007/03/13 00:11:14  gsmith
// Updates to concpy for 64-bit hosts
//
// Revision 1.76  2007/03/09 00:54:31  gsmith
// concpy rework
//
// Revision 1.75  2007/03/08 01:27:02  gsmith
// Remove inline attr from vfetchx/vstorex _full functions
//
// Revision 1.74  2007/02/18 16:27:36  gsmith
// Fix instfetch when instruction crosses 0x800
//
// Revision 1.73  2007/01/11 02:44:25  gsmith
// Temp patch to help messed up gcc v4 optimizations
//
// Revision 1.72  2007/01/09 23:19:35  gsmith
// Tweaks to sloppy fetch
//
// Revision 1.71  2007/01/04 23:12:04  gsmith
// remove thunk calls for program_interrupt
//
// Revision 1.70  2007/01/04 01:08:41  gsmith
// 03 Jan 2007 single_cpu_dw fetch/store patch for ia32
//
// Revision 1.69  2007/01/04 00:29:17  gsmith
// 03 Jan 2007 vstorex patch to vstore2, vstore4, vstore8
//
// Revision 1.68  2007/01/03 05:53:34  gsmith
// 03 Jan 2007 Sloppy fetch - Greg Smith
//
// Revision 1.67  2006/12/20 04:26:20  gsmith
// 19 Dec 2006 ip_all.pat - performance patch - Greg Smith
//
// Revision 1.66  2006/12/08 09:43:31  jj
// Add CVS message log
//

#define s370_wstorec(_src, _len, _addr, _arn, _regs) \
        s370_vstorec((_src), (_len), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs)) 
#define s370_wstoreb(_value, _addr, _arn, _regs) \
        s370_vstoreb((_value), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s370_wstore2(_value, _addr, _arn, _regs) \
        s370_vstore2((_value), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s370_wstore4(_value, _addr, _arn, _regs) \
        s370_vstore4((_value), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s370_wstore8(_value, _addr, _arn, _regs) \
        s370_vstore8((_value), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s370_wfetchc(_dest, _len, _addr, _arn, _regs) \
        s370_vfetchc((_dest), (_len), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs)) 
#define s370_wfetchb(_addr, _arn, _regs) \
        s370_vfetchb(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s370_wfetch2(_addr, _arn, _regs) \
        s370_vfetch2(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s370_wfetch4(_addr, _arn, _regs) \
        s370_vfetch4(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s370_wfetch8(_addr, _arn, _regs) \
        s370_vfetch8(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s370_wmove_chars(_addr1, _arn1, _key1, _addr2, _arn2, _key2, _len, _regs) \
        s370_move_chars(((_addr1) & ADDRESS_MAXWRAP((_regs))), (_arn1), (_key1), \
                        ((_addr2) & ADDRESS_MAXWRAP((_regs))), (_arn2), (_key2), (_len), (_regs))
#define s370_wvalidate_operand(_addr, _arn, _len, _acctype, _regs) \
        s370_validate_operand(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_len), (_acctype), (_regs))

#if !defined(OPTION_NO_INLINE_VSTORE) || defined(_VSTORE_C)

/*-------------------------------------------------------------------*/
/* Store 1 to 256 characters into virtual storage operand            */
/*                                                                   */
/* Input:                                                            */
/*      src     1 to 256 byte input buffer                           */
/*      len     Size of operand minus 1                              */
/*      addr    Logical address of leftmost character of operand     */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      range causes an addressing, translation, or protection       */
/*      exception, and in this case no real storage locations are    */
/*      updated, and the function does not return.                   */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(vstorec) (void *src, BYTE len,
                                        VADR addr, int arn, REGS *regs)
{
BYTE   *main1, *main2;                  /* Mainstor addresses        */
BYTE   *sk;                             /* Storage key addresses     */
int     len2;                           /* Length to end of page     */

    main1 = MADDR(addr, arn, regs, ACCTYPE_WRITE, regs->psw.pkey);
    memcpy(main1,
            src, len + 1);
    ITIMER_UPDATE(addr,len,regs);
#if defined(COMPARE_M65)
    record_herc_write(addr, len + 1, main1);
#endif

} /* end function ARCH_DEP(vstorec) */

/*-------------------------------------------------------------------*/
/* Store a single byte into virtual storage operand                  */
/*                                                                   */
/* Input:                                                            */
/*      value   Byte value to be stored                              */
/*      addr    Logical address of operand byte                      */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or protection             */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(vstoreb) (BYTE value, VADR addr,
                                         int arn, REGS *regs)
{
BYTE   *main1;                          /* Mainstor address          */

    main1 = MADDR(addr, arn, regs, ACCTYPE_WRITE, regs->psw.pkey);
    *main1 = value;
#if defined(COMPARE_M65)
    record_herc_write_char(addr, value);
#endif
    ITIMER_UPDATE(addr,1-1,regs);

} /* end function ARCH_DEP(vstoreb) */

/* vstore2 accelerator - Simple case only (better inline candidate) */
_VSTORE_C_STATIC void ARCH_DEP(vstore2) (U16 value, VADR addr, int arn,
                                                            REGS *regs)
{
    BYTE *mn;
    mn = MADDR (addr, arn, regs, ACCTYPE_WRITE, regs->psw.pkey);
    STORE_HW(mn, value);
#if defined(COMPARE_M65)
    record_herc_write(addr, 2, (char*)mn);
#endif
    ITIMER_UPDATE(addr,2-1,regs);

} /* end function ARCH_DEP(vstore2) */

/* vstore4 accelerator - Simple case only (better inline candidate) */
_VSTORE_C_STATIC void ARCH_DEP(vstore4) (U32 value, VADR addr, int arn,
                                                            REGS *regs)
{
    /* Most common case : Aligned */
    BYTE *mn;
    mn = MADDR(addr, arn, regs, ACCTYPE_WRITE, regs->psw.pkey);
    STORE_FW(mn, value);
    ITIMER_UPDATE(addr,4-1,regs);
#if defined(COMPARE_M65)
    record_herc_write(addr, 4, mn);
#endif
}

/*-------------------------------------------------------------------*/
/* Store an eight-byte integer into virtual storage operand          */
/*                                                                   */
/* Input:                                                            */
/*      value   64-bit integer value to be stored                    */
/*      addr    Logical address of leftmost operand byte             */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or protection             */
/*      exception, and in this case the function does not return.    */
/*                                                                   */
/*      NOTE that vstore8_full should only be invoked when a page    */
/*           boundary IS going to be crossed.                        */
/*-------------------------------------------------------------------*/
_VSTORE_FULL_C_STATIC void ARCH_DEP(vstore8_full)(U64 value, VADR addr,
                                              int arn, REGS *regs)
{
BYTE   *main1, *main2;                  /* Mainstor addresses        */
BYTE   *sk;                             /* Storage key addresses     */
int     len;                            /* Length to end of page     */
BYTE    temp[8];                        /* Copied value              */ 

    len = 0x800 - (addr & 0x7FF);
    main1 = MADDR(addr, arn, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);
    sk = regs->dat.storkey;
    main2 = MADDR((addr + len) & ADDRESS_MAXWRAP(regs), arn, regs,
                  ACCTYPE_WRITE, regs->psw.pkey);
    *sk |= (STORKEY_REF | STORKEY_CHANGE);
    STORE_DW(temp, value);
    memcpy(main1, temp, len);
    memcpy(main2, temp+len, 8-len);
#if defined(COMPARE_M65)
    record_herc_write(addr, len, (char*)main1);
    record_herc_write((addr + len) & ADDRESS_MAXWRAP(regs), 8-len, (char*)main2);
#endif

} /* end function ARCH_DEP(vstore8) */
_VSTORE_C_STATIC void ARCH_DEP(vstore8) (U64 value, VADR addr, int arn,
                                                            REGS *regs)
{
    if(likely(!((VADR_L)addr & 0x07)))
    {
        /* Most common case : Aligned */
        U64 *mn;
        mn = (U64*)MADDR(addr,arn,regs,ACCTYPE_WRITE,regs->psw.pkey);
#if defined(OPTION_SINGLE_CPU_DW) && defined(ASSIST_STORE_DW)
        if (regs->cpubit == regs->sysblk->started_mask)
            *mn = CSWAP64(value);
        else
#endif
        STORE_DW(mn, value);
#if defined(COMPARE_M65)
        record_herc_write(addr, 8, (char*)mn);
#endif
    }
    else
    {
        BYTE *mn;
        mn = MADDR(addr,arn,regs,ACCTYPE_WRITE,regs->psw.pkey);
        /* invoking STORE_DW ensures endianness correctness */
        STORE_DW(mn,value);
#if defined(COMPARE_M65)
        record_herc_write(addr, 8, (char*)mn);
#endif
    }
    ITIMER_UPDATE(addr,8-1,regs);
}

/*-------------------------------------------------------------------*/
/* Fetch a 1 to 256 character operand from virtual storage           */
/*                                                                   */
/* Input:                                                            */
/*      len     Size of operand minus 1                              */
/*      addr    Logical address of leftmost character of operand     */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/* Output:                                                           */
/*      dest    1 to 256 byte output buffer                          */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or fetch protection       */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(vfetchc) (void *dest, BYTE len,
                                        VADR addr, int arn, REGS *regs)
{
BYTE   *main1, *main2;                  /* Main storage addresses    */
int     len2;                           /* Length to copy on page    */

    main1 = MADDR(addr,arn,regs,ACCTYPE_READ,regs->psw.pkey);

    ITIMER_SYNC(addr,len,regs);
    memcpy (dest, main1, len + 1);
} /* end function ARCH_DEP(vfetchc) */

/*-------------------------------------------------------------------*/
/* Fetch a single byte operand from virtual storage                  */
/*                                                                   */
/* Input:                                                            */
/*      addr    Logical address of operand character                 */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/* Returns:                                                          */
/*      Operand byte                                                 */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or fetch protection       */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC BYTE ARCH_DEP(vfetchb) (VADR addr, int arn,
                                         REGS *regs)
{
BYTE   *mn;                           /* Main storage address      */

    ITIMER_SYNC(addr,1-1,regs);
    mn = MADDR (addr, arn, regs, ACCTYPE_READ, regs->psw.pkey);
    return *mn;
} /* end function ARCH_DEP(vfetchb) */

_VSTORE_C_STATIC U16 ARCH_DEP(vfetch2) (VADR addr, int arn, REGS *regs)
{
    BYTE *mn;
    ITIMER_SYNC(addr,2-1,regs);
    mn = MADDR(addr,arn,regs,ACCTYPE_READ,regs->psw.pkey);
    return fetch_hw(mn);
}

_VSTORE_C_STATIC U32 ARCH_DEP(vfetch4) (VADR addr, int arn, REGS *regs)
{
    BYTE *mn;
    ITIMER_SYNC(addr,4-1,regs);
    mn=MADDR(addr,arn,regs,ACCTYPE_READ,regs->psw.pkey);
    return fetch_fw(mn);
}

/*-------------------------------------------------------------------*/
/* Fetch an eight-byte integer operand from virtual storage          */
/*                                                                   */
/* Input:                                                            */
/*      addr    Logical address of leftmost byte of operand          */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/* Returns:                                                          */
/*      Operand in 64-bit integer format                             */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or fetch protection       */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_FULL_C_STATIC U64 ARCH_DEP(vfetch8_full) (VADR addr, int arn,
                                             REGS *regs)
{
BYTE   *mn;                             /* Main storage addresses    */
int     len;                            /* Length to end of page     */
BYTE    temp[16];                       /* Copy destination          */

    /* Get absolute address of first byte of operand */
    mn = MADDR (addr, arn, regs, ACCTYPE_READ, regs->psw.pkey);
    memcpy(temp, mn, 8);
    len = 0x800 - (addr & 0x7FF);
    mn = MADDR ((addr + len) & ADDRESS_MAXWRAP(regs), arn, regs,
                ACCTYPE_READ, regs->psw.pkey);
    memcpy(temp+len, mn, 8);
    return fetch_dw(temp);

} /* end function ARCH_DEP(vfetch8) */

_VSTORE_C_STATIC U64 ARCH_DEP(vfetch8) (VADR addr, int arn, REGS *regs)
{
    if(likely(!((VADR_L)addr & 0x07)))
    {
        /* doubleword aligned fetch */
        U64 *mn;
        ITIMER_SYNC(addr,8-1,regs);
        mn=(U64*)MADDR (addr, arn, regs, ACCTYPE_READ, regs->psw.pkey);
#if defined(OPTION_SINGLE_CPU_DW) && defined(ASSIST_FETCH_DW)
        if (regs->cpubit == regs->sysblk->started_mask)
            return CSWAP64(*mn);
#endif
        return fetch_dw(mn);
    }
    else
    {
        /* unaligned, non-crossing doubleword fetch */
        BYTE *mn;
        ITIMER_SYNC(addr,8-1,regs);
        mn=MADDR (addr, arn, regs, ACCTYPE_READ, regs->psw.pkey);
        return fetch_dw(mn);
    }
}
#endif

#if !defined(OPTION_NO_INLINE_IFETCH) || defined(_VSTORE_C)
/*-------------------------------------------------------------------*/
/* Fetch instruction from halfword-aligned virtual storage location  */
/*                                                                   */
/* Input:                                                            */
/*      regs    Pointer to the CPU register context                  */
/*      exec    If 1 then called by EXecute otherwise called by      */
/*              INSTRUCTION_FETCH                                    */
/*                                                                   */
/* If called by INSTRUCTION_FETCH then                               */
/*      addr    regs->psw.IA                                         */
/*      dest    regs->inst                                           */
/*                                                                   */
/* If called by EXecute then                                         */
/*      addr    regs->ET                                             */
/*      dest    regs->exinst                                         */
/*                                                                   */
/* Output:                                                           */
/*      If successful, a pointer is returned to the instruction. If  */
/*      the instruction crossed a page boundary then the instruction */
/*      is copied either to regs->inst or regs->exinst (depending on */
/*      the exec flag).  Otherwise the pointer points into mainstor. */
/*                                                                   */
/*      If the exec flag is 0 and tracing or PER is not active then  */
/*      the AIA is updated.  This forces interrupts to be checked    */
/*      instfetch to be call for each instruction.  Note that        */
/*      process_trace() is called from here if tracing is active.    */
/*                                                                   */
/*      A program check may be generated if the instruction address  */
/*      is odd, or causes an addressing or translation exception,    */
/*      and in this case the function does not return.  In the       */
/*      latter case, regs->instinvalid is 1 which indicates to       */
/*      program_interrupt that the exception occurred during         */
/*      instruction fetch.                                           */
/*                                                                   */
/*      Because this function is inlined and `exec' is a constant    */
/*      (either 0 or 1) the references to exec are optimized out by  */
/*      the compiler.                                                */
/*-------------------------------------------------------------------*/
_VFETCH_C_STATIC BYTE * ARCH_DEP(instfetch) (REGS *regs, int exec)
{
VADR    addr;                           /* Instruction address       */
BYTE   *ia;                             /* Instruction pointer       */
BYTE   *dest;                           /* Copied instruction        */
int     pagesz;                         /* Effective page size       */
int     offset;                         /* Address offset into page  */
int     len;                            /* Length for page crossing  */

    SET_BEAR_REG(regs, regs->bear_ip);

    addr = exec ? regs->ET
         : likely(regs->aie == NULL) ? regs->psw.IA : PSW_IA(regs,0);

    offset = (int)(addr & PAGEFRAME_BYTEMASK);

    /* Program check if instruction address is odd */
    if ( unlikely(offset & 0x01) )
    {
        if (!exec) regs->instinvalid = 1;
        regs->program_interrupt(regs, PGM_SPECIFICATION_EXCEPTION);
    }
    pagesz = unlikely(addr < 0x800) ? 0x800 : PAGEFRAME_PAGESIZE;

    if (!exec) regs->instinvalid = 1;

    /* Get instruction address */
    ia = MADDR (addr, USE_INST_SPACE, regs, ACCTYPE_INSTFETCH, regs->psw.pkey);

    /* If boundary is crossed then copy instruction to destination */
    if ( offset + ILC(ia[0]) > pagesz )
    {
        /* Note - dest is 8 bytes */
        dest = exec ? regs->exinst : regs->inst;
        memcpy (dest, ia, 4);
        len = pagesz - offset;
        offset = 0;
        addr = (addr + len) & ADDRESS_MAXWRAP(regs);
        ia = MADDR(addr, USE_INST_SPACE, regs, ACCTYPE_INSTFETCH, regs->psw.pkey);
        if (!exec) regs->ip = ia - len;
        memcpy(dest + len, ia, 4);
    }
    else
    {
        dest = ia;
        if (!exec) regs->ip = ia;
    }

    if (!exec)
    {
        regs->instinvalid = 0;

        /* Update the AIA */
        regs->AIV = addr & PAGEFRAME_PAGEMASK;
        regs->aip = (BYTE *)((uintptr_t)ia & ~PAGEFRAME_BYTEMASK);
        regs->aim = (uintptr_t)regs->aip ^ (uintptr_t)regs->AIV;
        if (likely(!regs->tracing && !regs->permode))
            regs->aie = regs->aip + pagesz - 5;
        else
        {
            regs->aie = (BYTE *)1;
            if (regs->tracing)
                ARCH_DEP(process_trace)(regs);
        }
    }

    return dest;

} /* end function ARCH_DEP(instfetch) */
#endif


/*-------------------------------------------------------------------*/
/* Copy 8 bytes at a time concurrently                               */
/*-------------------------------------------------------------------*/
#ifndef _VSTORE_CONCPY
#define _VSTORE_CONCPY
static __inline__ void concpy (REGS *regs, void *d, void *s, int n)
{
 int   n2;
 BYTE *dest = (BYTE *)d, *src = (BYTE *)s;

    /* Byte for byte copy if short length or possible overlap */
    if (n < 8
     || (dest <= src  && dest + 8 > src)
     || (src  <= dest && src  + 8 > dest))
    {
        /* use memset directly when the copy's effect is to
           propagate a byte over an area - like in MVC 1(255,2),0(2) */
        if(dest==src+1)
        {
            memset(dest,*src,n);
        }
        else
        {
            for ( ; n; n--)
                *(dest++) = *(src++);
        }
        return;
    }

    /* copy to an 8 byte boundary */
    n2 = (intptr_t)dest & 7;
    n -= n2;
    for ( ; n2; n2--)
        *(dest++) = *(src++);

#if !defined(OPTION_STRICT_ALIGNMENT) && \
    ((!defined(_MSVC_) && defined(SIZEOF_LONG)  && SIZEOF_LONG  >= 8) || \
     ( defined(_MSVC_) && defined(SIZEOF_INT_P) && SIZEOF_INT_P >= 8))

    /* Below for 64-bit BUILDS ONLY, since the below C code
       does NOT generate atomic 64-bit load/store assembler
       code sequence compatible with Concurrent Block Update
       except when building for 64-bit systems...
    */
    UNREFERENCED(regs);

    /* copy 8 bytes at a time */
    for ( ; n >= 8; n -= 8, dest += 8, src += 8)
        *(U64 *)dest = *(U64 *)src;

#else /* 32-bit builds... */

 #if !defined(OPTION_STRICT_ALIGNMENT)

    /* copy 4 bytes at a time if only one cpu started */
    if (regs->cpubit == regs->sysblk->started_mask)
        for ( ; n >= 4; n -= 4, dest += 4, src += 4)
            *(U32 *)dest = *(U32 *)src;
    else

 #else /* defined(OPTION_STRICT_ALIGNMENT) */
    UNREFERENCED(regs);
 #endif

    /* else copy 8 bytes at a time concurrently */
        for ( ; n >= 8; n -= 8, dest += 8, src += 8)
            store_dw_noswap(dest,fetch_dw_noswap(src));

#endif /* (64-bit builds test...) */

    /* copy leftovers */
    for ( ; n; n--)
        *(dest++) = *(src++);
}
#endif /* !defined(_VSTORE_CONCPY) */

#if !defined(OPTION_NO_INLINE_VSTORE) || defined(_VSTORE_C)

/*-------------------------------------------------------------------*/
/* Move characters using specified keys and address spaces           */
/*                                                                   */
/* Input:                                                            */
/*      addr1   Effective address of first operand                   */
/*      arn1    Access register number for first operand,            */
/*              or USE_PRIMARY_SPACE or USE_SECONDARY_SPACE          */
/*      key1    Bits 0-3=first operand access key, 4-7=zeroes        */
/*      addr2   Effective address of second operand                  */
/*      arn2    Access register number for second operand,           */
/*              or USE_PRIMARY_SPACE or USE_SECONDARY_SPACE          */
/*      key2    Bits 0-3=second operand access key, 4-7=zeroes       */
/*      len     Operand length minus 1 (range 0-255)                 */
/*      regs    Pointer to the CPU register context                  */
/*                                                                   */
/*      This function implements the MVC, MVCP, MVCS, MVCK, MVCSK,   */
/*      and MVCDK instructions.  These instructions move up to 256   */
/*      characters using the address space and key specified by      */
/*      the caller for each operand.  Operands are moved byte by     */
/*      byte to ensure correct processing of overlapping operands.   */
/*                                                                   */
/*      The arn parameter for each operand may be an access          */
/*      register number, in which case the operand is in the         */
/*      primary, secondary, or home space, or in the space           */
/*      designated by the specified access register, according to    */
/*      the current PSW addressing mode.                             */
/*                                                                   */
/*      Alternatively the arn parameter may be one of the special    */
/*      values USE_PRIMARY_SPACE or USE_SECONDARY_SPACE in which     */
/*      case the operand is in the specified space regardless of     */
/*      the current PSW addressing mode.                             */
/*                                                                   */
/*      A program check may be generated if either logical address   */
/*      causes an addressing, protection, or translation exception,  */
/*      and in this case the function does not return.               */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(move_chars) (VADR addr1, int arn1,
       BYTE key1, VADR addr2, int arn2, BYTE key2, int len, REGS *regs)
{
BYTE   *dest1, *dest2;                  /* Destination addresses     */
BYTE   *source1, *source2;              /* Source addresses          */
BYTE   *sk1, *sk2;                      /* Storage key addresses     */
int     len2, len3;                     /* Lengths to copy           */

    ITIMER_SYNC(addr2,len,regs);

    /* Translate addresses of leftmost operand bytes */
    source1 = MADDR (addr2, arn2, regs, ACCTYPE_READ, key2);
    dest1 = MADDR (addr1, arn1, regs, ACCTYPE_WRITE_SKP, key1);
    sk1 = regs->dat.storkey;

    concpy (regs, dest1, source1, len + 1);
    
    ITIMER_UPDATE(addr1,len,regs);
#if defined(COMPARE_M65)
    record_herc_write(addr1, len + 1, dest1);
#endif

} /* end function ARCH_DEP(move_chars) */



/*-------------------------------------------------------------------*/
/* Validate operand for addressing, protection, translation          */
/*                                                                   */
/* Input:                                                            */
/*      addr    Effective address of operand                         */
/*      arn     Access register number                               */
/*      len     Operand length minus 1 (range 0-255)                 */
/*      acctype Type of access requested: READ or WRITE              */
/*      regs    Pointer to the CPU register context                  */
/*                                                                   */
/*      The purpose of this function is to allow an instruction      */
/*      operand to be validated for addressing, protection, and      */
/*      translation exceptions, thus allowing the instruction to     */
/*      be nullified or suppressed before any updates occur.         */
/*                                                                   */
/*      A program check is generated if the operand causes an        */
/*      addressing, protection, or translation exception, and        */
/*      in this case the function does not return.                   */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(validate_operand) (VADR addr, int arn,
                                      int len, int acctype, REGS *regs)
{
    /* Translate address of leftmost operand byte */
    MADDR (addr, arn, regs, acctype, regs->psw.pkey);

#ifdef FEATURE_INTERVAL_TIMER
    else
        ITIMER_SYNC(addr,len,regs);
#endif /*FEATURE_INTERVAL_TIMER*/
} /* end function ARCH_DEP(validate_operand) */

#endif /*!defined(OPTION_NO_INLINE_VSTORE) || defined(_VSTORE_C)*/
