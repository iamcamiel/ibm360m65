/* ESA390.H     (c) Copyright Roger Bowler, 1994-2010                */
/*              ESA/390 Data Areas                                   */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2009      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2009      */

// $Id: esa390.h 5631 2010-02-14 22:26:40Z rbowler $

#ifndef _ESA390_H
#define _ESA390_H

/* Platform-independent storage operand definitions */

#include "htypes.h"         // (need Hercules fixed-size data types)

#if defined(WORDS_BIGENDIAN)

 typedef union {
                 U16 H;
                 struct { BYTE H; BYTE L; } B;
               } HW;
 typedef union {
                 U32 F;
                 struct { HW H; HW L; } H;
                 struct { BYTE B; U32 A:24; } A;
               } FW;
 typedef union {
                 U64 D;
                 struct { FW H; FW L; } F;
               } DW;
 typedef union {
                 struct { DW H; DW L; } D;
                 struct { FW HH; FW HL; FW LH; FW LL; } F;
               } QW;

#else // !defined(WORDS_BIGENDIAN)

 typedef union {
                 U16 H;
                 struct { BYTE L; BYTE H; } B;
               } HW;
 typedef union {
                 U32 F;
                 struct { HW L; HW H; } H;
                 struct { U32 A:24, B:8; } A;
               } FW;
 typedef union {
                 U64 D;
                 struct { FW L; FW H; } F;
               } DW;
 typedef union {
                 struct { DW L; DW H; } D;
                 struct { FW LL; FW LH; FW HL; FW HH; } F;
               } QW;

#endif // defined(WORDS_BIGENDIAN)

typedef union {
                 HWORD H;
                 struct { BYTE H; BYTE L; } B;
               } HWORD_U;
typedef union {
                 FWORD F;
                 struct { HWORD_U H; HWORD_U L; } H;
               } FWORD_U;
typedef union {
                 DBLWRD D;
                 struct { FWORD_U H; FWORD_U L; } F;
               } DWORD_U;

/* Internal-format PSW structure definition */
typedef struct  _PSW {
        BYTE     sysmask;               /* System mask      (0 -  7) */
        BYTE     pkey;                  /* PSW Key          (8 - 11) */
        BYTE     states;                /* EC,M,W,P bits   (12 - 15) */
        BYTE     asc;                   /* Address space control     */
                                        /*                 (16 - 17) */
        BYTE     cc;                    /* Condition code  (18 - 19) */
        BYTE     progmask;              /* Program mask    (20 - 23) */
        BYTE     zerobyte;              /* Zeroes          (24 - 31) */
                                        /* or (esame)      (24 - 30) */
        u_int                           /* Addressing mode (31 - 32) */
                 amode64:1,             /* 64-bit addressing    (31) */
                 amode:1,               /* 31-bit addressing    (32) */
                 zeroilc:1;             /* 1=Zero ILC                */
        U32      zeroword;              /* esame only      (33 - 63) */
        DW       ia;                    /* Instruction addrress      */
                                        /*                 (33 - 63) */
                                        /* or (esame)      (64 -127) */
        DW       amask;                 /* Address wraparound mask   */
        U16      intcode;               /* Interruption code         */
        BYTE     ilc;                   /* Instruction length count  */
        BYTE     unused;
    } PSW;

#define IA_G     ia.D
#define IA_H     ia.F.H.F
#define IA_L     ia.F.L.F
#define IA_LA24  ia.F.L.A.A

#define AMASK_G  amask.D
#define AMASK_L  amask.F.L.F
#define AMASK_H  amask.F.H.F
#define AMASK24  0x00FFFFFF
#define AMASK31  0x7FFFFFFF
#define AMASK64  0xFFFFFFFFFFFFFFFFULL

/* System mask                 (0 -  7) */
#define PSW_PERMODE     0x40            /* Program event recording   */
#define PSW_DATMODE     0x04            /* Dynamic addr translation  */
#define PSW_IOMASK      0x02            /* I/O interrupt mask        */
#define PSW_EXTMASK     0x01            /* External interrupt mask   */

/* PSW key mask                (8 - 11) */
#define PSW_KEYMASK     0xF0            /* PSW key mask              */

/*                            (12 - 15) */
#define PSW_MACH_BIT       2    /* 0x04    Machine check mask        */
#define PSW_WAIT_BIT       1    /* 0x02    Wait state                */
#define PSW_PROB_BIT       0    /* 0x01    Problem state             */

/* Condition code             (18 - 19) */
#define PSW_CCMASK      0x30            /* Condition code mask       */

/* Program mask               (20 - 23) */
#define PSW_PROGMASK    0x0F            /* Program-mask bits         */
#define PSW_FOBIT          3    /* 0x08    Fixed-point overflow bit  */
#define PSW_DOBIT          2    /* 0x04    Decimal overflow bit      */
#define PSW_EUBIT          1    /* 0x02    Exponent underflow bit    */
#define PSW_SGBIT          0    /* 0x01    Significance bit          */

/* Macros for testing states (EC, M, W, P bits) */
#define MACHMASK(p)  (((p)->states & BIT(PSW_MACH_BIT))     != 0)
#define WAITSTATE(p) (((p)->states & BIT(PSW_WAIT_BIT))     != 0)
#define PROBSTATE(p) (((p)->states & BIT(PSW_PROB_BIT))     != 0)

/* Macros for testing program mask */
#define FOMASK(p)             ( (p)->progmask & BIT(PSW_FOBIT) )
#define DOMASK(p)             ( (p)->progmask & BIT(PSW_DOBIT) )
#define EUMASK(p)             ( (p)->progmask & BIT(PSW_EUBIT) )
#define SGMASK(p)             ( (p)->progmask & BIT(PSW_SGBIT) )

/* Structure definition for translation-lookaside buffer entry */
#define TLBN            1024            /* Number TLB entries        */
#define TLB_MASK        0x3FF           /* Mask for 1024 entries     */
#define TLB_REAL_ASD_L  0xFFFFFFFF      /* ASD values for real mode  */
#define TLB_REAL_ASD_G  0xFFFFFFFFFFFFFFFFULL
#define TLB_HOST_ASD    0x800           /* Host entry for XC guest   */
/* TLB Notes -
 * Fields set by translate_addr() are asd, vaddr, pte, id, common and
 * protect.
 * Fields set by logical_to_main() are main, storkey, skey, read and
 * write and are used for accelerated address lookup (formerly AEA).
 */

/* Structure for Dynamic Address Translation */
typedef struct _DAT {
//        RADR    raddr;                  /* Real address              */
//        RADR    aaddr;                  /* Absolute address          */
//        RADR    rpfra;                  /* Real page frame address   */
//        RADR    asd;                    /* Address space designator: */
                                        /*   STD or ASCE             */
//        int     stid;                   /* Address space indicator   */
        BYTE   *storkey;                /* ->Storage key             */
//        U16     xcode;                  /* Translation exception code*/
//        u_int   private:1,              /* 1=Private address space   */
//                protect:2;              /* 1=Page prot, 2=ALE prot   */
      } DAT;

/* Bit definitions for control register 0 */
#define CR0_BMPX        0x80000000      /* Block multiplex ctl  S/370*/
#define CR0_SSM_SUPP    0x40000000      /* SSM suppression control   */
#define CR0_TOD_SYNC    0x20000000      /* TOD clock sync control    */
#define CR0_LOW_PROT    0x10000000      /* Low-address protection    */
#define CR0_EXT_AUTH    0x08000000      /* Extraction auth control   */
#define CR0_SEC_SPACE   0x04000000      /* Secondary space control   */
#define CR0_FETCH_OVRD  0x02000000      /* Fetch protection override */
#define CR0_STORE_OVRD  0x01000000      /* Store protection override */
#define CR0_STORKEY_4K  0x01000000      /* Storkey exception control */
#define CR0_TRAN_FMT    0x00F80000      /* Translation format bits...*/
#define CR0_TRAN_ESA390 0x00B00000      /* ...1M/4K ESA/390 format   */
#define CR0_PAGE_SIZE   0x00C00000      /* Page size for S/370...    */
#define CR0_PAGE_SZ_2K  0x00400000      /* ...2K pages               */
#define CR0_PAGE_SZ_4K  0x00800000      /* ...4K pages               */
#define CR0_ZPAG_SZ_1M  0x00800000      /* ...1M pages           EDAT*/
#define CR0_SEG_SIZE    0x00380000      /* Segment size for S/370... */
#define CR0_SEG_SZ_64K  0x00000000      /* ...64K segments           */
#define CR0_SEG_SZ_1M   0x00100000      /* ...1M segments            */
#define CR0_ASN_LX_REUS 0x00080000      /* ASN-and-LX-reuse control  */
#define CR0_AFP         0x00040000      /* AFP register control      */
#define CR0_VOP         0x00020000      /* Vector control         390*/
#define CR0_ASF         0x00010000      /* AS function control    390*/
#define CR0_XM_MALFALT  0x00008000      /* Malfunction alert mask    */
#define CR0_XM_EMERSIG  0x00004000      /* Emergency signal mask     */
#define CR0_XM_EXTCALL  0x00002000      /* External call mask        */
#define CR0_XM_TODSYNC  0x00001000      /* TOD clock sync mask       */
#define CR0_XM_CLKC     0x00000800      /* Clock comparator mask     */
#define CR0_XM_PTIMER   0x00000400      /* CPU timer mask            */
#define CR0_XM_SERVSIG  0x00000200      /* Service signal mask       */
#define CR0_XM_ITIMER   0x00000080      /* Interval timer mask  S/370*/
#define CR0_XM_INTKEY   0x00000040      /* Interrupt key mask        */
#define CR0_XM_EXTSIG   0x00000020      /* External signal mask S/370*/
#define CR0_XM_ETR      0x00000010      /* External timer mask       */
#define CR0_PC_FAST     0x00000008      /* PC fast control        390*/
#define CR0_CRYPTO      0x00000004      /* Crypto control       ESAME*/
#define CR0_IUCV        0x00000002      /* IUCV interrupt mask       */

#define SERVSIG_PEND    0x00000001      /* Event buffer pending      */
#define SERVSIG_ADDR    0xFFFFFFF8      /* Parameter address         */

/* Bit definitions for control register 1 */
/* CR1 is the primary segment table descriptor or primary ASCE */

/* Bit definitions for control register 2 */
#define CR2_DUCTO       0x7FFFFFC0      /* DUCT origin               */
/* For S/370, CR2 contains channel masks for channels 0-31 */

/* Bit definitions for control register 3 */
#define CR3_SASTEIN     0xFFFFFFFF00000000ULL /* SASN STE instance#  */
#define CR3_KEYMASK     0xFFFF0000      /* PSW key mask              */
#define CR3_SASN        0x0000FFFF      /* Secondary ASN             */

/* Bit definitions for control register 4 */
#define CR4_PASTEIN     0xFFFFFFFF00000000ULL /* PASN STE instance#  */
#define CR4_AX          0xFFFF0000      /* Authorization index       */
#define CR4_PASN        0x0000FFFF      /* Primary ASN               */

/* Bit definitions for control register 5 */
                                        /* When CR0_ASF=0 (ESA/390): */
#define CR5_SSLINK      0x80000000      /* Subsystem-Linkage control */
#define CR5_LTO         0x7FFFFF80      /* Linkage-Table origin      */
#define CR5_LTL         0x0000007F      /* Linkage-Table length      */
                                        /* When CR0_ASF=1 or ESAME:  */
#define CR5_PASTEO      0x7FFFFFC0      /* Primary-ASTE origin       */

/* Bit definitions for control register 6 */
/* CR6 is the I/O interruption subclass mask */

/* Bit definitions for control register 7 */
/* CR7 is the secondary segment table descriptor or secondary ASCE */

/* Bit definitions for control register 8 */
#define CR8_EAX         0xFFFF0000      /* Extended auth index       */
#define CR8_MCMASK      0x0000FFFF      /* Monitor masks             */

/* Bit definitions for PER */
#define CR9_SB          0x80000000      /* Successful Branching      */
#define CR9_IF          0x40000000      /* Instruction Fetch         */
#define CR9_SA          0x20000000      /* Storage Alteration        */
#define CR9_GRA         0x10000000      /* General Register Alt.     */
#define CR9_STURA       0x08000000      /* Store using real addr     */
#define CR9_IFNUL       0x01000000      /* IF nullification     @PER3*/
#define CR9_GRMASK      0x0000FFFF      /* GR mask bits              */
#define CR9_BAC         0x00800000      /* Br addr control PER2 only */
#define CR9_SAC         0x00200000      /* Stor. alter. c. PER2 only */

/* Bit definitions for control register 12 */
#define S_CR12_BRTRACE  0x80000000      /* Branch trace control      */
#define Z_CR12_BRTRACE  0x8000000000000000ULL /* Branch trace control*/
#define CR12_MTRACE     0x4000000000000000ULL /* Mode trace control  */
#define S_CR12_TRACEEA  0x7FFFFFFC      /* Trace entry address       */
#define Z_CR12_TRACEEA  0x3FFFFFFFFFFFFFFCULL /* Trace entry address */
#define CR12_ASNTRACE   0x00000002      /* ASN trace control         */
#define CR12_EXTRACE    0x00000001      /* Explicit trace control    */

/* Bit definitions for control register 13 */
/* CR13 is the home segment table descriptor or home ASCE */

/* Bit definitions for control register 14 */
#define CR14_CHKSTOP    0x80000000      /* Check-stop control   S/370*/
#define CR14_SYNCMCEL   0x40000000      /* Synchronous MCEL     S/370*/
#define CR14_IOEXTLOG   0x20000000      /* I/O extended logout  S/370*/
#define CR14_CHANRPT    0x10000000      /* Channel report mask       */
#define CR14_RCVYRPT    0x08000000      /* Recovery report mask      */
#define CR14_DGRDRPT    0x04000000      /* Degradation report mask   */
#define CR14_XDMGRPT    0x02000000      /* External damage mask      */
#define CR14_WARNING    0x01000000      /* Warning mask              */
#define CR14_ASYNMCEL   0x00800000      /* Asynchronous MCEL    S/370*/
#define CR14_ASYNFIXL   0x00400000      /* Asynch fixed log     S/370*/
#define CR14_TODCTLOV   0x00200000      /* TOD clock control override*/
#define CR14_ASN_TRAN   0x00080000      /* ASN translation control   */
#define CR14_AFTO       0x0007FFFF      /* ASN first table origin    */

/* Bit definitions for control register 15 */
#define CR15_MCEL       0x00FFFFF8      /* MCEL address         S/370*/

/* Linkage table designation bit definitions */
#define LTD_SSLINK      0x80000000      /* Subsystem-Linkage control */
#define LTD_LTO         0x7FFFFF80      /* Linkage-Table origin      */
#define LTD_LTL         0x0000007F      /* Linkage-Table length      */

/* Linkage first table designation bit definitions (ASN-and-LX-reuse)*/
#define LFTD_SSLINK     0x80000000      /* Subsystem-Linkage control */
#define LFTD_LFTO       0x7FFFFF00      /* Linkage-First-Table origin*/
#define LFTD_LFTL       0x000000FF      /* Linkage-First-Table length*/

/* Values for designation type and table type (ESAME mode) */
#define TT_R1TABL       0xC             /* Region first table        */
#define TT_R2TABL       0x8             /* Region second table       */
#define TT_R3TABL       0x4             /* Region third table        */
#define TT_SEGTAB       0x0             /* Segment table             */

/* Address space control element bit definitions (ESAME mode) */
#define ASCE_TO         0xFFFFFFFFFFFFF000ULL /* Table origin        */
#define ASCE_G          0x200           /* Subspace group indicator  */
#define ASCE_P          0x100           /* Private space indicator   */
#define ASCE_S          0x080           /* Storage alteration event  */
#define ASCE_X          0x040           /* Space switch event        */
#define ASCE_R          0x020           /* Real space                */
#define ASCE_DT         0x00C           /* Designation type          */
#define ASCE_TL         0x003           /* Table length              */
#define ASCE_RESV       0xC10           /* Reserved bits - ignored   */

/* Region table entry bit definitions (ESAME mode) */
#define REGTAB_TO       0xFFFFFFFFFFFFF000ULL /* Table origin        */
#define REGTAB_P        0x200           /* DAT Protection bit    EDAT*/
#define REGTAB_TF       0x0C0           /* Table offset              */
#define REGTAB_I        0x020           /* Region invalid            */
#define REGTAB_TT       0x00C           /* Table type                */
#define REGTAB_TL       0x003           /* Table length              */
#define REGTAB_RESV     0xD10           /* Reserved bits - ignored   */

/* Segment table entry bit definitions (ESAME mode) */
#define ZSEGTAB_PTO     0xFFFFFFFFFFFFF800ULL /* Page table origin   */
#define ZSEGTAB_SFAA    0xFFFFFFFFFFF00000ULL /* Seg Fr Abs Addr EDAT*/
#define ZSEGTAB_AV      0x10000         /* ACCF Validity Control EDAT*/
#define ZSEGTAB_ACC     0x0F000         /* Access Control Bits   EDAT*/
#define ZSEGTAB_F       0x800           /* Fetch Protection      EDAT*/
#define ZSEGTAB_FC      0x400           /* Format control        EDAT*/
#define ZSEGTAB_P       0x200           /* Page protection bit       */
#define ZSEGTAB_CO      0x100           /* Change-rec override   EDAT*/
#define ZSEGTAB_I       0x020           /* Invalid segment           */
#define ZSEGTAB_C       0x010           /* Common segment            */
#define ZSEGTAB_TT      0x00C           /* Table type                */
#define ZSEGTAB_RESV    0x0C3           /* Reserved bits - ignored   */

/* Page table entry bit definitions (ESAME mode) */
#define ZPGETAB_PFRA    0xFFFFFFFFFFFFF000ULL /* Page frame real addr*/
#define ZPGETAB_I       0x400           /* Invalid page              */
#define ZPGETAB_P       0x200           /* Protected page            */
#define ZPGETAB_ESVALID 0x100           /* Valid in expanded storage */
#define ZPGETAB_CO      0x100           /* Change-rec override   EDAT*/
#define ZPGETAB_ESREF   0x080           /* ES Referenced             */
#define ZPGETAB_ESCHA   0x040           /* ES Changed                */
#define ZPGETAB_ESLCK   0x020           /* ES Locked                 */
#define ZPGETAB_RESV    0x800           /* Reserved bits - must be 0 */

/* Segment table designation bit definitions (ESA/390 mode) */
#define STD_SSEVENT     0x80000000      /* Space switch event        */
#define STD_STO         0x7FFFF000      /* Segment table origin      */
#define STD_RESV        0x00000C00      /* Reserved bits - must be 0 */
#define STD_GROUP       0x00000200      /* Subspace group indicator  */
#define STD_PRIVATE     0x00000100      /* Private space indicator   */
#define STD_SAEVENT     0x00000080      /* Storage alteration event  */
#define STD_STL         0x0000007F      /* Segment table length      */

/* Segment table entry bit definitions (ESA/390 mode) */
#define SEGTAB_PTO      0x7FFFFFC0      /* Page table origin         */
#define SEGTAB_INVALID  0x00000020      /* Invalid segment           */
#define SEGTAB_COMMON   0x00000010      /* Common segment            */
#define SEGTAB_PTL      0x0000000F      /* Page table length         */
#define SEGTAB_RESV     0x80000000      /* Reserved bits - must be 0 */

/* Page table entry bit definitions (ESA/390 mode) */
#define PAGETAB_PFRA    0x7FFFF000      /* Page frame real address   */
#define PAGETAB_ESNK    0x00000800      /* ES NK bit                 */
#define PAGETAB_INVALID 0x00000400      /* Invalid page              */
#define PAGETAB_PROT    0x00000200      /* Protected page            */
#define PAGETAB_ESVALID 0x00000100      /* Valid in expanded storage */
#define PAGETAB_ESREF   0x00000004      /* ES Referenced             */
#define PAGETAB_ESCHA   0x00000002      /* ES Changed                */
#define PAGETAB_PGLOCK  0x00000001      /* Page lock (LKPG)          */
#define PAGETAB_RESV    0x80000900      /* Reserved bits - must be 0 */

/* Segment table designation bit definitions (S/370 mode) */
#define STD_370_STL     0xFF000000      /* 370 segment table length  */
#define STD_370_STO     0x00FFFFC0      /* 370 segment table origin  */
#define STD_370_SSEVENT 0x00000001      /* 370 space switch event    */

/* Segment table entry bit definitions (S/370 mode) */
#define SEGTAB_370_PTL  0xF0000000      /* Page table length         */
#define SEGTAB_370_PTO  0x00FFFFF8      /* Page table origin         */
#define SEGTAB_370_PROT 0x00000004      /* Protected segment         */
#define SEGTAB_370_CMN  0x00000002      /* Common segment            */
#define SEGTAB_370_INVL 0x00000001      /* Invalid segment           */
#define SEGTAB_370_RSV  0x0F000000      /* Reserved bits - must be 0 */

/* Page table entry bit definitions (S/370 mode) */
#define PAGETAB_PFRA_4K 0xFFF0          /* Page frame real address   */
#define PAGETAB_INV_4K  0x0008          /* Invalid page              */
#define PAGETAB_EA_4K   0x0006          /* Extended physical address */
#define PAGETAB_PFRA_2K 0xFFF8          /* Page frame real address   */
#define PAGETAB_INV_2K  0x0004          /* Invalid page              */
#define PAGETAB_RSV_2K  0x0002          /* Reserved bit - must be 0  */

/* Access-list entry token special value definitions */
#define ALET_PRIMARY    0               /* Primary address-space     */
#define ALET_SECONDARY  1               /* Secondary address-space   */
#define ALET_HOME       2               /* Home address-space        */

/* Access-list entry token bit definitions */
#define ALET_RESV       0xFE000000      /* Reserved bits - must be 0 */
#define ALET_PRI_LIST   0x01000000      /* Primary space access-list */
#define ALET_ALESN      0x00FF0000      /* ALE sequence number       */
#define ALET_ALEN       0x0000FFFF      /* Access-list entry number  */

#define ALD_ALO         0x7FFFFF80      /* Access-list origin (fmt0) */
#define ALD_ALL         0x0000007F      /* Access-list length (fmt0) */
#define ALD_ALL_SHIFT   3               /* Length units are 2**3     */

/* Access-list entry bit definitions */
#define ALE0_INVALID    0x80000000      /* ALEN invalid              */
#define ALE0_FETCHONLY  0x02000000      /* Fetch only address space  */
#define ALE0_PRIVATE    0x01000000      /* Private address space     */
#define ALE0_ALESN      0x00FF0000      /* ALE sequence number       */
#define ALE0_ALEAX      0x0000FFFF      /* ALE authorization index   */
#define ALE2_ASTE       0x7FFFFFC0      /* ASTE address              */
#define ALE3_ASTESN     0xFFFFFFFF      /* ASTE sequence number      */

/* Address-space number (ASN) bit definitions */
#define ASN_AFX         0xFFC0          /* ASN first table index     */
#define ASN_ASX         0x003F          /* ASN second table index    */

/* ASN first table entry bit definitions */
#define AFTE_INVALID    0x80000000      /* ASN invalid               */
#define AFTE_ASTO_0     0x7FFFFFF0      /* ASTE origin (CR0_ASF=0)   */
#define AFTE_RESV_0     0x0000000F      /* Reserved bits (CR0_ASF=0) */
#define AFTE_ASTO_1     0x7FFFFFC0      /* ASTE origin (CR0_ASF=1)   */
#define AFTE_RESV_1     0x0000003F      /* Reserved bits (CR0_ASF=1) */

/* ASN second table entry bit definitions */
#define ASTE0_INVALID   0x80000000      /* ASX invalid               */
#define ASTE0_ATO       0x7FFFFFFC      /* Authority-table origin    */
#define ASTE0_RESV      0x00000002      /* Must be 0 for ESA/390     */
#define ASTE0_BASE      0x00000001      /* Base space of group       */
#define ASTE1_AX        0xFFFF0000      /* Authorization index       */
#define ASTE1_ATL       0x0000FFF0      /* Authority-table length    */
#define ASTE1_RESV      0x0000000F      /* Must be 0 for ESA/390     */
#define ASTE1_CA        0x00000002      /* Controlled ASN            */
#define ASTE1_RA        0x00000001      /* Reusable ASN              */
/* ASTE word 2 is the segment table designation for ESA/390 */
/* ASTE word 3 is the linkage-table designation for ESA/390 */
/* ASTE words 2 and 3 are the ASCE (RTD, STD, or RSD) for ESAME */
/* ASTE word 4 is the access-list designation */
#define ASTE5_ASTESN    0xFFFFFFFF      /* ASTE sequence number      */
#define ASTE6_RESV      0xFFFFFFFF      /* Must be zero for ESA/390  */
/* ASTE word 6 is the LTD or LFTD for ESAME */
/* ASTE words 7-9 are reserved for control program use */
/* ASTE word 10 is unused */
#define ASTE11_ASTEIN   0xFFFFFFFF      /* ASTE instance number      */
/* ASTE words 12-15 are unused */

/* Authority table entry bit definitions */
#define ATE_PRIMARY     0x80            /* Primary authority bit     */
#define ATE_SECONDARY   0x40            /* Secondary authority bit   */

/* Dispatchable unit control table bit definitions */
#define DUCT0_BASTEO    0x7FFFFFC0      /* Base ASTE origin          */
#define DUCT1_SA        0x80000000      /* Subspace active           */
#define DUCT1_SSASTEO   0x7FFFFFC0      /* Subspace ASTE origin      */
/* DUCT word 2 is unused */
#define DUCT3_SSASTESN  0xFFFFFFFF      /* Subspace ASTE seq number  */
/* DUCT word 4 is the access-list designation */
/* DUCT word 5 is unused for ESA/390 */
/* DUCT word 5 contains PKM/KEY/RA/PROB for ESAME */
/* DUCT word 6 is unused */
/* DUCT word 7 is for control program use */
/* DUCT words 8 and 9 are the return address for ESAME. In 24-bit and
   31-bit mode, word 8 contains zero and word 9 contains AM31/IA31 */
/* DUCT word 8 contains AM31/IA31 for ESA/390 */
/* DUCT word 9 contains PKM/KEY/RA/PROB for ESA/390 */
/* DUCT word 10 is unused */
#define DUCT11_TCBA     0x7FFFFFF8      /* Trap control block address*/
#define DUCT11_TE       0x00000001      /* Trap enabled              */
/* DUCT word 12 is unused */
/* DUCT word 13 is unused */
/* DUCT word 14 is unused */
/* DUCT word 15 is unused */
/* Bit definitions for DUCT word 5 (ESAME) or word 9 (ESA/390) */
#define DUCT_PKM        0xFFFF0000      /* PSW key mask              */
#define DUCT_KEY        0x000000F0      /* PSW key                   */
#define DUCT_RA         0x00000008      /* Reduced authority state   */
#define DUCT_PROB       0x00000001      /* Problem state             */
/* Bit definitions for DUCT word 9 (ESAME) or word 8 (ESA/390) */
#define DUCT_AM31       0x80000000      /* 1=31-bit, 0=24-bit address*/
#define DUCT_IA31       0x7FFFFFFF      /* 24/31 return address      */

#define TCB0_P          0x00040000      /* Bit 13 PSW Control (P)    */
#define TCB0_R          0x00020000      /* Bit 14 GR Control (R)     */

#define TRAP0_EXECUTE   0x80000000      /* TRAP is target of execute */
#define TRAP0_TRAP4     0x40000000      /* TRAP is TRAP4             */

/* Linkage stack entry descriptor structure definition */
typedef struct _LSED {
        BYTE    uet;                    /* U-bit and entry type      */
        BYTE    si;                     /* Section identification    */
        HWORD   rfs;                    /* Remaining free space      */
        HWORD   nes;                    /* Next entry size           */
        HWORD   resv;                   /* Reserved bits - must be 0 */
    } LSED;

/* Stack type definitions */
#define LSED_UET_U      0x80            /* Unstack suppression bit   */
#define LSED_UET_ET     0x7F            /* Entry type...             */

#define S_LSED_UET_HDR  0x01            /* ...header entry           */
#define S_LSED_UET_TLR  0x02            /* ...trailer entry          */
#define S_LSED_UET_BAKR 0x04            /* ...branch state entry     */
#define S_LSED_UET_PC   0x05            /* ...call state entry       */

#define Z_LSED_UET_HDR  0x09            /* ...header entry           */
#define Z_LSED_UET_TLR  0x0A            /* ...trailer entry          */
#define Z_LSED_UET_BAKR 0x0C            /* ...branch state entry     */
#define Z_LSED_UET_PC   0x0D            /* ...call state entry       */

/* Program call number bit definitions */
#define PC_LFX1         0xFFF00000      /* Linkage first index (high)*/
#define PC_BIT44        0x00080000      /* 1=LFX1 is significant     */
#define PC_LFX2         0x0007E000      /* Linkage first index (low) */
#define PC_LSX          0x00001F00      /* Linkage second index      */
#define PC_LX           0x000FFF00      /* Linkage index             */
#define PC_EX           0x000000FF      /* Entry index               */

/* Linkage table entry bit definitions */
#define LTE_INVALID     0x80000000      /* LX invalid                */
#define LTE_ETO         0x7FFFFFC0      /* Entry table origin        */
#define LTE_ETL         0x0000003F      /* Entry table length        */

/* Linkage first table entry bit definitions (ASN-and-LX-reuse) */
#define LFTE_INVALID    0x80000000      /* LFX invalid               */
#define LFTE_LSTO       0x7FFFFF00      /* Linkage second table orig */

/* Linkage second table entry bit definitions (ASN-and-LX-reuse) */
#define LSTE0_INVALID   0x80000000      /* LSX invalid               */
#define LSTE0_ETO       0x7FFFFFC0      /* Entry table origin        */
#define LSTE0_ETL       0x0000003F      /* Entry table length        */
#define LSTE1_LSTESN    0xFFFFFFFF      /* LSTE sequence number      */

/* Entry table bit entry definitions */
/* ETE word 0 is the left half of the EIA for ESAME if ETE4_G is set */
#define ETE0_AKM        0xFFFF0000      /* Authorization key mask 390*/
#define ETE0_ASN        0x0000FFFF      /* Address space number   390*/
#define ETE1_AMODE      0x80000000      /* Addressing mode           */
#define ETE1_EIA        0x7FFFFFFE      /* Instruction address       */
#define ETE1_PROB       0x00000001      /* Problem state bit         */
/* ETE word 2 is the entry parameter for ESA/390 */
#define ETE2_AKM        0xFFFF0000      /* Auth.key mask        ESAME*/
#define ETE2_ASN        0x0000FFFF      /* Address space number ESAME*/
#define ETE3_EKM        0xFFFF0000      /* Entry key mask            */
#define ETE4_T          0x80000000      /* 0=Basic PC, 1=Stacking PC */
#define ETE4_G          0x40000000      /* 1=64-bit EIA/EPARM   ESAME*/
#define ETE4_K          0x10000000      /* 1=Replace PSW key by EK   */
#define ETE4_M          0x08000000      /* 1=Replace PKM by EKM, 0=or*/
#define ETE4_E          0x04000000      /* 1=Replace EAX by EEAX     */
#define ETE4_C          0x02000000      /* 0=Primary mode, 1=AR mode */
#define ETE4_S          0x01000000      /* SASN:0=old PASN,1=new PASN*/
#define ETE4_EK         0x00F00000      /* Entry key                 */
#define ETE4_EEAX       0x0000FFFF      /* Entry extended AX         */
#define ETE5_ASTE       0x7FFFFFC0      /* ASTE address              */
/* ETE words 6 and 7 are unused for ESA/390 */
/* ETE words 6 and 7 are the entry parameter for ESAME */

/* Clock states */
#define CC_CLOCK_SET    0               /* Clock in set state        */
#define CC_CLOCK_NOTSET 1               /* Clock in not-set state    */
#define CC_CLOCK_ERROR  2               /* Clock in error state      */
#define CC_CLOCK_STOP   3               /* Clock in stopped state or
                                           not-operational state     */

/* SIGP order codes */
#define SIGP_SENSE               0x01   /* Sense                     */
#define SIGP_EXTCALL             0x02   /* External call             */
#define SIGP_EMERGENCY           0x03   /* Emergency signal          */
#define SIGP_START               0x04   /* Start                     */
#define SIGP_STOP                0x05   /* Stop                      */
#define SIGP_RESTART             0x06   /* Restart                   */
#define SIGP_IPR                 0x07   /* Initial program reset  370*/
#define SIGP_PR                  0x08   /* Program reset          370*/
#define SIGP_STOPSTORE           0x09   /* Stop and store status     */
#define SIGP_IMPL                0x0A   /* Initial uprogram load  370*/
#define SIGP_INITRESET           0x0B   /* Initial CPU reset         */
#define SIGP_RESET               0x0C   /* CPU reset                 */
#define SIGP_STORE               0x0E   /* Store status at address   */
#define SIGP_STOREX              0x11   /* Store ext stat at addr 390*/
#define SIGP_SETARCH             0x12   /* Set architecture mode     */
#define SIGP_COND_EMERGENCY      0x13   /* Conditional Emergency     */
#define SIGP_SENSE_RUNNING_STATE 0x15   /* Sense Running State       */

#define MAX_SIGPORDER            0x15   /* Maximum SIGP order value  */
#define LOG_SIGPORDER            0x03   /* Log any SIGP > this value */

/* SIGP status codes */
#define SIGP_STATUS_EQUIPMENT_CHECK             0x80000000
#define SIGP_STATUS_NOT_RUNNING                 0x00000400
#define SIGP_STATUS_INCORRECT_STATE             0x00000200
#define SIGP_STATUS_INVALID_PARAMETER           0x00000100
#define SIGP_STATUS_EXTERNAL_CALL_PENDING       0x00000080
#define SIGP_STATUS_STOPPED                     0x00000040
#define SIGP_STATUS_OPERATOR_INTERVENING        0x00000020
#define SIGP_STATUS_CHECK_STOP                  0x00000010
#define SIGP_STATUS_INOPERATIVE                 0x00000004
#define SIGP_STATUS_INVALID_ORDER               0x00000002
#define SIGP_STATUS_RECEIVER_CHECK              0x00000001

/* Storage key bit definitions */
#define STORKEY_KEY     0xF0            /* Storage key               */
#define STORKEY_FETCH   0x08            /* Fetch protect bit         */
#define STORKEY_REF     0x04            /* Reference bit             */
#define STORKEY_CHANGE  0x02            /* Change bit                */
#define STORKEY_BADFRM  0x01            /* Unusable frame            */

/* Prefixed storage area structure definition */
typedef struct _PSA_3XX {               /* Prefixed storage area     */
/*000*/ DBLWRD iplpsw;                  /* IPL PSW, Restart new PSW  */
/*008*/ DBLWRD iplccw1;                 /* IPL CCW1, Restart old PSW */
/*010*/ DBLWRD iplccw2;                 /* IPL CCW2                  */
/*018*/ DBLWRD extold;                  /* External old PSW          */
/*020*/ DBLWRD svcold;                  /* SVC old PSW               */
/*028*/ DBLWRD pgmold;                  /* Program check old PSW     */
/*030*/ DBLWRD mckold;                  /* Machine check old PSW     */
/*038*/ DBLWRD iopold;                  /* I/O old PSW               */
/*040*/ DBLWRD csw;                     /* Channel status word (S370)*/
/*048*/ FWORD  caw;                     /* Channel address word(S370)*/
/*04C*/ FWORD  resv04C;                 /* Reserved                  */
/*050*/ FWORD  inttimer;                /* Interval timer            */
/*054*/ FWORD  resv054;                 /* Reserved                  */
/*058*/ DBLWRD extnew;                  /* External new PSW          */
/*060*/ DBLWRD svcnew;                  /* SVC new PSW               */
/*068*/ DBLWRD pgmnew;                  /* Program check new PSW     */
/*070*/ DBLWRD mcknew;                  /* Machine check new PSW     */
/*078*/ DBLWRD iopnew;                  /* I/O new PSW               */
/*080*/ FWORD  extparm;                 /* External interrupt param  */
/*084*/ HWORD  extcpad;                 /* External interrupt CPU#   */
/*086*/ HWORD  extint;                  /* External interrupt code   */
/*088*/ FWORD  svcint;                  /* SVC interrupt code        */
/*08C*/ FWORD  pgmint;                  /* Program interrupt code    */
/*090*/ FWORD  tea;                     /* Translation exception addr*/
/*094*/ HWORD  monclass;                /* Monitor class             */
/*096*/ HWORD  perint;                  /* PER interrupt code        */
/*098*/ FWORD  peradr;                  /* PER address               */
/*09C*/ FWORD  moncode;                 /* Monitor code              */
/*0A0*/ BYTE   excarid;                 /* Exception access id       */
/*0A1*/ BYTE   perarid;                 /* PER access id             */
/*0A2*/ BYTE   opndrid;                 /* Operand access id         */
/*0A3*/ BYTE   arch;                    /* Architecture mode ID      */
/*0A4*/ FWORD  resv0A4;                 /* Reserved                  */
/*0A8*/ FWORD  chanid;                  /* Channel id (S370)         */
/*0AC*/ FWORD  ioelptr;                 /* I/O extended logout (S370)*/
/*0B0*/ FWORD  lcl;                     /* Limited chan logout (S370)*/
/*0B4*/ FWORD  resv0B0;                 /* Reserved                  */
/*0B8*/ FWORD  ioid;                    /* I/O interrupt device id   */
/*0BC*/ FWORD  ioparm;                  /* I/O interrupt parameter   */
/*0C0*/ FWORD  iointid;                 /* I/O interrupt ID          */
/*0C4*/ FWORD  resv0C4;                 /* Reserved                  */
/*0C8*/ FWORD  stfl;                    /* Facilities list (STFL)    */
/*0CC*/ FWORD  resv0CC;                 /* Reserved                  */
/*0D0*/ DBLWRD resv0D0;                 /* Reserved                  */
/*0D8*/ DBLWRD storeptmr;               /* CPU timer save area       */
/*0E0*/ DBLWRD storeclkc;               /* Clock comparator save area*/
/*0E8*/ DBLWRD mckint;                  /* Machine check int code    */
/*0F0*/ FWORD  resv0F0;                 /* Reserved                  */
/*0F4*/ FWORD  xdmgcode;                /* External damage code      */
/*0F8*/ FWORD  mcstorad;                /* Failing storage address   */
/*0FC*/ FWORD  resv0FC;                 /* Reserved                  */
/*100*/ DBLWRD storepsw;                /* Store status PSW save area*/
/*108*/ FWORD  storepfx;                /* Prefix register save area */
/*10C*/ FWORD  resv10C;                 /* Reserved                  */
/*110*/ DBLWRD resv110;                 /* Reserved                  */
/*118*/ DBLWRD resv118;                 /* Reserved                  */
/*120*/ FWORD  storear[16];             /* Access register save area */
/*160*/ FWORD  storefpr[8];             /* FP register save area     */
/*180*/ FWORD  storegpr[16];            /* General register save area*/
/*1C0*/ FWORD  storecr[16];             /* Control register save area*/
} PSA_3XX;

/* Bit settings for translation exception address */
#define TEA_SECADDR     0x80000000      /* Secondary addr (370,390)  */
#define TEA_PROT_A      0x008           /* Access-list prot (ESAME)  */
#define TEA_PROT_AP     0x004           /* Access-list/page protected*/
#define TEA_MVPG        0x004           /* MVPG exception (ESAME)    */
#define TEA_ST          0x003           /* Address space indication..*/
#define TEA_ST_PRIMARY  0x000           /* ..primary STO/ASCE used   */
#define TEA_ST_ARMODE   0x001           /* ..access register mode    */
#define TEA_ST_SECNDRY  0x002           /* ..secondary STO/ASCE used */
#define TEA_ST_HOME     0x003           /* ..home STO/ASCE used      */
#define TEA_SSEVENT     0x80000000      /* Space switch event bit    */
#define TEA_ASN         0x0000FFFF      /* Address space number      */
#define TEA_PCN         0x000FFFFF      /* Program call number       */

/* Bit settings for machine check interruption code */
#define MCIC_SD  0x8000000000000000ULL  /* System damage             */
#define MCIC_P   0x4000000000000000ULL  /* Instruction proc damage   */
#define MCIC_SR  0x2000000000000000ULL  /* System recovery           */
#define MCIC_CD  0x0800000000000000ULL  /* Timing facility damage    */
#define MCIC_ED  0x0400000000000000ULL  /* External damage           */
#define MCIC_VF  0x0200000000000000ULL  /* Vector facility failure   */
#define MCIC_DG  0x0100000000000000ULL  /* Degradation               */

#define MCIC_W   0x0080000000000000ULL  /* Warning                   */
#define MCIC_CP  0x0040000000000000ULL  /* Channel report pending    */
#define MCIC_SP  0x0020000000000000ULL  /* Service processor damage  */
#define MCIC_CK  0x0010000000000000ULL  /* Channel subsystem damage  */
#define MCIC_VS  0x0004000000000000ULL  /* Vector facility source    */
#define MCIC_B   0x0002000000000000ULL  /* Backed up                 */

#define MCIC_SE  0x0000800000000000ULL  /* Storage error uncorrected */
#define MCIC_SC  0x0000400000000000ULL  /* Storage error corrected   */
#define MCIC_KE  0x0000200000000000ULL  /* Storkey error uncorrected */
#define MCIC_DS  0x0000100000000000ULL  /* Storage degradation       */
#define MCIC_WP  0x0000080000000000ULL  /* PSW-MWP validity          */
#define MCIC_MS  0x0000040000000000ULL  /* PSW mask and key validity */
#define MCIC_PM  0x0000020000000000ULL  /* PSW pm and cc validity    */
#define MCIC_IA  0x0000010000000000ULL  /* PSW ia validity           */

#define MCIC_FA  0x0000008000000000ULL  /* Failing stor addr validity*/
#define MCIC_EC  0x0000002000000000ULL  /* External damage code val. */
#define MCIC_FP  0x0000001000000000ULL  /* Floating point reg val.   */
#define MCIC_GR  0x0000000800000000ULL  /* General register validity */
#define MCIC_CR  0x0000000400000000ULL  /* Control register validity */
#define MCIC_ST  0x0000000100000000ULL  /* Storage logical validity  */

#define MCIC_IE  0x0000000080000000ULL  /* Indirect storage error    */
#define MCIC_AR  0x0000000040000000ULL  /* Access register validity  */
#define MCIC_DA  0x0000000020000000ULL  /* Delayed access exeption   */

#define MCIC_PR  0x0000000000200000ULL  /* TOD prog. reg. validity   */
#define MCIC_XF  0x0000000000100000ULL  /* Extended float reg val.   */
#define MCIC_AP  0x0000000000080000ULL  /* Ancillary report          */
#define MCIC_CT  0x0000000000020000ULL  /* CPU timer validity        */
#define MCIC_CC  0x0000000000010000ULL  /* Clock comparator validity */

/* Channel Report Word definitions */
#define CRW_SOL         0x40000000      /* Solicited CRW             */
#define CRW_OVER        0x20000000      /* Overflow, CRW's lost      */
#define CRW_CHAIN       0x10000000      /* More CRW's describe event */
#define CRW_RSC         0x0F000000      /* Reporting resource mask   */
#define CRW_MONIT       0x02000000      /* Channel monitor is source */
#define CRW_SUBCH       0x03000000      /* Subchannel is source      */
#define CRW_CHPID       0x04000000      /* Channel path is source    */
#define CRW_CAF         0x09000000      /* Configuration alert       */
#define CRW_CSS         0x0B000000      /* Channel subsys is source  */
#define CRW_AR          0x00800000      /* Ancillary report indicator*/
#define CRW_ERC         0x003F0000      /* Error recovery code       */
#define CRW_AVAIL       0x00010000      /* Available                 */
#define CRW_INIT        0x00020000      /* Initialized no parm. chg. */
#define CRW_TEMP        0x00030000      /* Temporary error           */
#define CRW_ALERT       0x00040000      /* Installed, subch changed  */
#define CRW_TERM        0x00050000      /* Terminal                  */
#define CRW_PERM        0x00060000      /* Permanent error / not init*/
#define CRW_PERMI       0x00070000      /* Permanent, initialized    */
#define CRW_IPM         0x00080000      /* PIM / PAM / CHPIDs changed*/
#define CRW_RSID        0x0000FFFF      /* Resource identifier       */

/* Bit settings for channel id */
#define CHANNEL_TYPE    0xF0000000      /* Bits 0-3=Channel type...  */
#define CHANNEL_SEL     0x00000000      /* ...selector channel       */
#define CHANNEL_MPX     0x10000000      /* ...byte multiplexor       */
#define CHANNEL_BMX     0x20000000      /* ...block multiplexor      */
#define CHANNEL_MODEL   0x0FFF0000      /* Bits 4-15=Channel model   */
#define CHANNEL_MAXIOEL 0x0000FFFF      /* Bits 16-31=Max.IOEL length*/

/* Program interruption codes */
#define PGM_OPERATION_EXCEPTION                         0x0001
#define PGM_PRIVILEGED_OPERATION_EXCEPTION              0x0002
#define PGM_EXECUTE_EXCEPTION                           0x0003
#define PGM_PROTECTION_EXCEPTION                        0x0004
#define PGM_ADDRESSING_EXCEPTION                        0x0005
#define PGM_SPECIFICATION_EXCEPTION                     0x0006
#define PGM_DATA_EXCEPTION                              0x0007
#define PGM_FIXED_POINT_OVERFLOW_EXCEPTION              0x0008
#define PGM_FIXED_POINT_DIVIDE_EXCEPTION                0x0009
#define PGM_DECIMAL_OVERFLOW_EXCEPTION                  0x000A
#define PGM_DECIMAL_DIVIDE_EXCEPTION                    0x000B
#define PGM_EXPONENT_OVERFLOW_EXCEPTION                 0x000C
#define PGM_EXPONENT_UNDERFLOW_EXCEPTION                0x000D
#define PGM_SIGNIFICANCE_EXCEPTION                      0x000E
#define PGM_FLOATING_POINT_DIVIDE_EXCEPTION             0x000F
#define PGM_SEGMENT_TRANSLATION_EXCEPTION               0x0010
#define PGM_PAGE_TRANSLATION_EXCEPTION                  0x0011
#define PGM_TRANSLATION_SPECIFICATION_EXCEPTION         0x0012
#define PGM_SPECIAL_OPERATION_EXCEPTION                 0x0013
#define PGM_OPERAND_EXCEPTION                           0x0015
#define PGM_TRACE_TABLE_EXCEPTION                       0x0016
#define PGM_ASN_TRANSLATION_SPECIFICATION_EXCEPTION     0x0017
#define PGM_VECTOR_OPERATION_EXCEPTION                  0x0019
#define PGM_SPACE_SWITCH_EVENT                          0x001C
#define PGM_SQUARE_ROOT_EXCEPTION                       0x001D
#define PGM_UNNORMALIZED_OPERAND_EXCEPTION              0x001E
#define PGM_PC_TRANSLATION_SPECIFICATION_EXCEPTION      0x001F
#define PGM_AFX_TRANSLATION_EXCEPTION                   0x0020
#define PGM_ASX_TRANSLATION_EXCEPTION                   0x0021
#define PGM_LX_TRANSLATION_EXCEPTION                    0x0022
#define PGM_EX_TRANSLATION_EXCEPTION                    0x0023
#define PGM_PRIMARY_AUTHORITY_EXCEPTION                 0x0024
#define PGM_SECONDARY_AUTHORITY_EXCEPTION               0x0025
#define PGM_LFX_TRANSLATION_EXCEPTION                   0x0026
#define PGM_LSX_TRANSLATION_EXCEPTION                   0x0027
#define PGM_ALET_SPECIFICATION_EXCEPTION                0x0028
#define PGM_ALEN_TRANSLATION_EXCEPTION                  0x0029
#define PGM_ALE_SEQUENCE_EXCEPTION                      0x002A
#define PGM_ASTE_VALIDITY_EXCEPTION                     0x002B
#define PGM_ASTE_SEQUENCE_EXCEPTION                     0x002C
#define PGM_EXTENDED_AUTHORITY_EXCEPTION                0x002D
#define PGM_LSTE_SEQUENCE_EXCEPTION                     0x002E
#define PGM_ASTE_INSTANCE_EXCEPTION                     0x002F
#define PGM_STACK_FULL_EXCEPTION                        0x0030
#define PGM_STACK_EMPTY_EXCEPTION                       0x0031
#define PGM_STACK_SPECIFICATION_EXCEPTION               0x0032
#define PGM_STACK_TYPE_EXCEPTION                        0x0033
#define PGM_STACK_OPERATION_EXCEPTION                   0x0034
#define PGM_ASCE_TYPE_EXCEPTION                         0x0038
#define PGM_REGION_FIRST_TRANSLATION_EXCEPTION          0x0039
#define PGM_REGION_SECOND_TRANSLATION_EXCEPTION         0x003A
#define PGM_REGION_THIRD_TRANSLATION_EXCEPTION          0x003B
#define PGM_MONITOR_EVENT                               0x0040
#define PGM_PER_EVENT                                   0x0080
#define PGM_CRYPTO_OPERATION_EXCEPTION                  0x0119

/* External interrupt codes */
#define EXT_INTERRUPT_KEY_INTERRUPT                     0x0040
#define EXT_INTERVAL_TIMER_INTERRUPT                    0x0080
#define EXT_TOD_CLOCK_SYNC_CHECK_INTERRUPT              0x1003
#define EXT_CLOCK_COMPARATOR_INTERRUPT                  0x1004
#define EXT_CPU_TIMER_INTERRUPT                         0x1005
#define EXT_MALFUNCTION_ALERT_INTERRUPT                 0x1200
#define EXT_EMERGENCY_SIGNAL_INTERRUPT                  0x1201
#define EXT_EXTERNAL_CALL_INTERRUPT                     0x1202
#define EXT_ETR_INTERRUPT                               0x1406
#define EXT_SERVICE_SIGNAL_INTERRUPT                    0x2401
#define EXT_IUCV_INTERRUPT                              0x4000

/* Macros for classifying CCW operation codes */
#define IS_CCW_WRITE(c)         (((c)&0x03)==0x01)
#define IS_CCW_READ(c)          (((c)&0x03)==0x02)
#define IS_CCW_CONTROL(c)       (((c)&0x03)==0x03)
#define IS_CCW_NOP(c)           ((c)==0x03)
#define IS_CCW_SET_EXTENDED(c)  ((c)==0xC3)
#define IS_CCW_SENSE(c)         (((c)&0x0F)==0x04)
#define IS_CCW_TIC(c)           (((c)&0x0F)==0x08)
#define IS_CCW_RDBACK(c)        (((c)&0x0F)==0x0C)

/* Operation request block structure definition */
typedef struct _ORB {
        FWORD   intparm;                /* Interruption parameter    */
        BYTE    flag4;                  /* Flag byte 4               */
        BYTE    flag5;                  /* Flag byte 5               */
        BYTE    lpm;                    /* Logical path mask         */
        BYTE    flag7;                  /* Flag byte 7               */
        FWORD   ccwaddr;                /* CCW address               */
    } ORB;

/* Bit definitions for ORB flag byte 4 */
#define ORB4_KEY        0xF0            /* Subchannel protection key */
#define ORB4_S          0x08            /* Suspend control           */
#define ORB4_C          0x04            /* Streaming mode (FICON)    */
#define ORB4_M          0x02            /* Modification (FICON)      */
#define ORB4_Y          0x01            /* Synchronization (FICON)   */

/* Bit definitions for ORB flag byte 5 */
#define ORB5_F          0x80            /* CCW format                */
#define ORB5_P          0x40            /* Prefetch                  */
#define ORB5_I          0x20            /* Initial status interrupt  */
#define ORB5_A          0x10            /* Address limit checking    */
#define ORB5_U          0x08            /* Suppress susp interrupt   */
#define ORB5_RESV       0x04            /* Reserved bit - must be 0  */
#define ORB5_H          0x02            /* Format-2 IDAW control     */
#define ORB5_T          0x01            /* 2K format-2 IDAW control  */

/* Bit definitions for ORB flag byte 7 */
#define ORB7_L          0x80            /* Suppress incorrect length */
#define ORB7_D          0x40            /* MIDAW control          @MW*/
#define ORB7_RESV       0x3E            /* Reserved - must be 0   @MW*/
#define ORB7_X          0x01            /* ORB extension control     */

/* Path management control word structure definition */
typedef struct _PMCW {
        FWORD   intparm;                /* Interruption parameter    */
        BYTE    flag4;                  /* Flag byte 4               */
        BYTE    flag5;                  /* Flag byte 5               */
        HWORD   devnum;                 /* Device number             */
        BYTE    lpm;                    /* Logical path mask         */
        BYTE    pnom;                   /* Path not operational mask */
        BYTE    lpum;                   /* Last path used mask       */
        BYTE    pim;                    /* Path installed mask       */
        HWORD   mbi;                    /* Measurement block index   */
        BYTE    pom;                    /* Path operational mask     */
        BYTE    pam;                    /* Path available mask       */
        BYTE    chpid[8];               /* Channel path identifiers  */
        BYTE    zone;                   /* SIE zone                  */
        BYTE    flag25;                 /* Flag byte 25              */
        BYTE    flag26;                 /* Reserved byte - must be 0 */
        BYTE    flag27;                 /* Flag byte 27              */
    } PMCW;

/* Bit definitions for PMCW flag byte 4 */
#define PMCW4_Q         0x80            /* QDIO available            */
#define PMCW4_ISC       0x38            /* Interruption subclass     */
#define PMCW4_A         0x01            /* Alternate Block Control   */
#define PMCW4_RESV      0x46            /* Reserved bits - must be 0 */

/* Bit definitions for PMCW flag byte 5 */
#define PMCW5_E         0x80            /* Subchannel enabled        */
#define PMCW5_LM        0x60            /* Limit mode...             */
#define PMCW5_LM_NONE   0x00            /* ...no limit checking      */
#define PMCW5_LM_LOW    0x20            /* ...lower limit specified  */
#define PMCW5_LM_HIGH   0x40            /* ...upper limit specified  */
#define PMCW5_LM_RESV   0x60            /* ...reserved value         */
#define PMCW5_MM        0x18            /* Measurement mode enable...*/
#define PMCW5_MM_MBU    0x10            /* ...meas.block.upd enabled */
#define PMCW5_MM_DCTM   0x08            /* Dev.conn.time.meas enabled*/
#define PMCW5_D         0x04            /* Multipath mode enabled    */
#define PMCW5_T         0x02            /* Timing facility available */
#define PMCW5_V         0x01            /* Subchannel valid          */

/* Bit definitions for PMCW flag byte 25 */
#define PMCW25_VISC     0x1F            /* Guest ISC                 */
#define PMCW25_TYPE     0xE0            /* Subchannel Type           */
#define PMCW25_TYPE_0   0x00            /* I/O Subchannel            */
#define PMCW25_TYPE_1   0x20            /* CHSC subchannel           */
#define PMCW25_TYPE_2   0x40            /* Message subchannel        */
#define PMCW25_TYPE_3   0x60            /* ADM subchannel            */


/* Bit definitions for PMCW flag byte 27 */
#define PMCW27_I        0x80            /* Interrupt Interlock Cntl  */
#define PMCW27_S        0x01            /* Concurrent sense mode     */
#define PMCW27_RESV     0x7E            /* Reserved bits - must be 0 */

/* Extended status word structure definition */
typedef struct _ESW {
        BYTE    scl0;                   /* Subchannel logout byte 0  */
        BYTE    lpum;                   /* Last path used mask       */
        BYTE    scl2;                   /* Subchannel logout byte 2  */
        BYTE    scl3;                   /* Subchannel logout byte 3  */
        BYTE    erw0;                   /* Extended report word byte0*/
        BYTE    erw1;                   /* Extended report word byte1*/
        BYTE    erw2;                   /* Extended report word byte2*/
        BYTE    erw3;                   /* Extended report word byte3*/
        FWORD   failaddr;               /* Failing storage address   */
        FWORD   resv2;                  /* Reserved word - must be 0 */
        FWORD   resv3;                  /* Reserved word - must be 0 */
    } ESW;

/* Bit definitions for subchannel logout byte 0 */
#define SCL0_ESF        0x7F            /* Extended status flags...  */
#define SCL0_ESF_KEY    0x40            /* ...key check              */
#define SCL0_ESF_MBPGK  0x20            /* ...meas.block prog.check  */
#define SCL0_ESF_MBDCK  0x10            /* ...meas.block data check  */
#define SCL0_ESF_MBPTK  0x08            /* ...meas.block prot.check  */
#define SCL0_ESF_CCWCK  0x04            /* ...CCW check              */
#define SCL0_ESF_IDACK  0x02            /* ...IDAW check             */

/* Bit definitions for subchannel logout byte 2 */
#define SCL2_R          0x80            /* Ancillary report bit      */
#define SCL2_FVF        0x7C            /* Field validity flags...   */
#define SCL2_FVF_LPUM   0x40            /* ...LPUM valid             */
#define SCL2_FVF_TC     0x20            /* ...termination code valid */
#define SCL2_FVF_SC     0x10            /* ...sequence code valid    */
#define SCL2_FVF_USTAT  0x08            /* ...device status valid    */
#define SCL2_FVF_CCWAD  0x04            /* ...CCW address valid      */
#define SCL2_SA         0x03            /* Storage access code...    */
#define SCL2_SA_UNK     0x00            /* ...access type unknown    */
#define SCL2_SA_RD      0x01            /* ...read                   */
#define SCL2_SA_WRT     0x02            /* ...write                  */
#define SCL2_SA_RDBK    0x03            /* ...read backward          */

/* Bit definitions for subchannel logout byte 3 */
#define SCL3_TC         0xC0            /* Termination code...       */
#define SCL3_TC_HALT    0x00            /* ...halt signal issued     */
#define SCL3_TC_NORM    0x40            /* ...stop, stack, or normal */
#define SCL3_TC_CLEAR   0x80            /* ...clear signal issued    */
#define SCL3_TC_RESV    0xC0            /* ...reserved               */
#define SCL3_D          0x20            /* Device status check       */
#define SCL3_E          0x10            /* Secondary error           */
#define SCL3_A          0x08            /* I/O error alert           */
#define SCL3_SC         0x07            /* Sequence code             */

/* Bit definitions for extended report word byte 0 */
#define ERW0_A          0x10            /* Authorization check       */
#define ERW0_P          0x08            /* Path verification required*/
#define ERW0_T          0x04            /* Channel path timeout      */
#define ERW0_F          0x02            /* Failing storage addr valid*/
#define ERW0_S          0x01            /* Concurrent sense          */

/* Bit definitions for extended report word byte 1 */
#define ERW1_SCNT       0x3F            /* Concurrent sense count    */

/* Subchannel status word structure definition */
typedef struct _SCSW {
        BYTE    flag0;                  /* Flag byte 0               */
        BYTE    flag1;                  /* Flag byte 1               */
        BYTE    flag2;                  /* Flag byte 2               */
        BYTE    flag3;                  /* Flag byte 3               */
        FWORD   ccwaddr;                /* CCW address               */
        BYTE    unitstat;               /* Device status             */
        BYTE    chanstat;               /* Subchannel status         */
        HWORD   count;                  /* Residual byte count       */
    } SCSW;

/* Bit definitions for SCSW flag byte 0 */
#define SCSW0_KEY       0xF0            /* Subchannel protection key */
#define SCSW0_S         0x08            /* Suspend control           */
#define SCSW0_L         0x04            /* ESW format (logout stored)*/
#define SCSW0_CC        0x03            /* Deferred condition code...*/
#define SCSW0_CC_0      0x00            /* ...condition code 0       */
#define SCSW0_CC_1      0x01            /* ...condition code 1       */
#define SCSW0_CC_3      0x03            /* ...condition code 3       */

/* Bit definitions for SCSW flag byte 1 */
#define SCSW1_F         0x80            /* CCW format                */
#define SCSW1_P         0x40            /* Prefetch                  */
#define SCSW1_I         0x20            /* Initial status interrupt  */
#define SCSW1_A         0x10            /* Address limit checking    */
#define SCSW1_U         0x08            /* Suppress susp interrupt   */
#define SCSW1_Z         0x04            /* Zero condition code       */
#define SCSW1_E         0x02            /* Extended control          */
#define SCSW1_N         0x01            /* Path not operational      */

/* Bit definitions for SCSW flag byte 2 */
#define SCSW2_Q         0x80            /* QDIO active               */
#define SCSW2_FC        0x70            /* Function control bits...  */
#define SCSW2_FC_START  0x40            /* ...start function         */
#define SCSW2_FC_HALT   0x20            /* ...halt function          */
#define SCSW2_FC_CLEAR  0x10            /* ...clear function         */
#define SCSW2_AC        0x0F            /* Activity control bits...  */
#define SCSW2_AC_RESUM  0x08            /* ...resume pending         */
#define SCSW2_AC_START  0x04            /* ...start pending          */
#define SCSW2_AC_HALT   0x02            /* ...halt pending           */
#define SCSW2_AC_CLEAR  0x01            /* ...clear pending          */

/* Bit definitions for SCSW flag byte 3 */
#define SCSW3_AC        0xE0            /* Activity control bits...  */
#define SCSW3_AC_SCHAC  0x80            /* ...subchannel active      */
#define SCSW3_AC_DEVAC  0x40            /* ...device active          */
#define SCSW3_AC_SUSP   0x20            /* ...suspended              */
#define SCSW3_SC        0x1F            /* Status control bits...    */
#define SCSW3_SC_ALERT  0x10            /* ...alert status           */
#define SCSW3_SC_INTER  0x08            /* ...intermediate status    */
#define SCSW3_SC_PRI    0x04            /* ...primary status         */
#define SCSW3_SC_SEC    0x02            /* ...secondary status       */
#define SCSW3_SC_PEND   0x01            /* ...status pending         */

/* CSW unit status flags */
#define CSW_ATTN        0x80            /* Attention                 */
#define CSW_SM          0x40            /* Status modifier           */
#define CSW_CUE         0x20            /* Control unit end          */
#define CSW_BUSY        0x10            /* Busy                      */
#define CSW_CE          0x08            /* Channel end               */
#define CSW_DE          0x04            /* Device end                */
#define CSW_UC          0x02            /* Unit check                */
#define CSW_UX          0x01            /* Unit exception            */

/* CSW channel status flags */
#define CSW_PCI         0x80            /* Program control interrupt */
#define CSW_IL          0x40            /* Incorrect length          */
#define CSW_PROGC       0x20            /* Program check             */
#define CSW_PROTC       0x10            /* Protection check          */
#define CSW_CDC         0x08            /* Channel data check        */
#define CSW_CCC         0x04            /* Channel control check     */
#define CSW_ICC         0x02            /* Interface control check   */
#define CSW_CHC         0x01            /* Chaining check            */

/* CCW flags */
#define CCW_FLAGS_CD    0x80            /* Chain data flag           */
#define CCW_FLAGS_CC    0x40            /* Chain command flag        */
#define CCW_FLAGS_SLI   0x20            /* Suppress incorrect length
                                           indication flag           */
#define CCW_FLAGS_SKIP  0x10            /* Skip flag                 */
#define CCW_FLAGS_PCI   0x08            /* Program controlled
                                           interrupt flag            */
#define CCW_FLAGS_IDA   0x04            /* Indirect data address flag*/
#define CCW_FLAGS_SUSP  0x02            /* Suspend flag              */
#define CCW_FLAGS_MIDAW 0x01            /* Modified IDAW flag     @MW*/

/* MIDAW flags (bits 40-47)                                       @MW*/
#define MIDAW_LAST      0x80            /* Last MIDAW flag        @MW*/
#define MIDAW_SKIP      0x40            /* Skip flag              @MW*/
#define MIDAW_DTI       0x20            /* Data transfer interrupt@MW*/
#define MIDAW_RESV      0x1F            /* Reserved bits          @MW*/

/* Device independent bit settings for sense byte 0 */
#define SENSE_CR        0x80            /* Command reject            */
#define SENSE_IR        0x40            /* Intervention required     */
#define SENSE_BOC       0x20            /* Bus-out check             */
#define SENSE_EC        0x10            /* Equipment check           */
#define SENSE_DC        0x08            /* Data check                */
#define SENSE_OR        0x04            /* Overrun                   */
#define SENSE_US        0x04            /* Unit specify              */
#define SENSE_CC        0x02            /* Control check             */
#define SENSE_OC        0x01            /* Operation check           */

/* Device dependent bit settings for sense byte 1 */
#define SENSE1_PER      0x80            /* Permanent Error           */
#define SENSE1_ITF      0x40            /* Invalid Track Format      */
#define SENSE1_EOC      0x20            /* End of Cylinder           */
#define SENSE1_MTO      0x10            /* Message to Operator       */
#define SENSE1_NRF      0x08            /* No Record Found           */
#define SENSE1_FP       0x04            /* File Protected            */
#define SENSE1_WRI      0x02            /* Write Inhibited           */
#define SENSE1_IE       0x01            /* Imprecise Ending          */

/* Subchannel information block structure definition */
typedef struct _SCHIB {
        PMCW    pmcw;                   /* Path management ctl word  */
        SCSW    scsw;                   /* Subchannel status word    */
        BYTE    moddep[12];             /* Model dependent area      */
    } SCHIB;

/* Interruption response block structure definition */
typedef struct _IRB {
        SCSW    scsw;                   /* Subchannel status word    */
        ESW esw;                        /* Extended status word      */
        BYTE    ecw[32];                /* Extended control word     */
    } IRB;

/* Measurement Block */
typedef struct _MBK {
        HWORD   srcount;                /* SSCH + RSCH count         */
        HWORD   samplecnt;              /* Sample count              */
        FWORD   dct;                    /* Device connect time       */
        FWORD   fpt;                    /* Function pending time     */
        FWORD   ddt;                    /* Device disconnect time    */
        FWORD   cuqt;                   /* Control unit queueing time*/
        FWORD   resv[3];                /* Reserved                  */
    } MBK;

/* Bit definitions for SCHM instruction */
#define CHM_GPR1_MBK    0xF0000000      /* Measurement Block Key     */
#define CHM_GPR1_M      0x00000002      /* Measurement mode control  */
#define CHM_GPR1_D      0x00000001      /* Block update Mode         */
#define CHM_GPR1_A      0x01000000      /* Alternate mode            */
#define CHM_GPR1_ZONE   0x00FF0000      /* Zone                      */
#define CHM_GPR1_RESV   0x0E00FFFC      /* Reserved, must be zero    */

/* Measurement Block Origin  */
#define S_CHM_GPR2_RESV 0x8000001F      /* Reserved, must be zero    */
#define Z_CHM_GPR2_RESV 0x0000001F      /* Reserved, must be zero    */

/* Definitions for PLO instruction */
#define PLO_GPR0_FC     0x000000FF      /* Function code mask        */
#define PLO_GPR0_T      0x00000100      /* Function test mask        */
#define PLO_GPR0_RESV   0xFFFFFE00      /* Reserved bits             */
#define PLO_CL                   0      /* Compare and load          */
#define PLO_CLG                  1      /* Compare and load          */
#define PLO_CLGR                 2      /* Compare and load    ESAME */
#define PLO_CLX                  3      /* Compare and load    ESAME */
#define PLO_CS                   4      /* Compare and swap          */
#define PLO_CSG                  5      /* Compare and swap          */
#define PLO_CSGR                 6      /* Compare and swap    ESAME */
#define PLO_CSX                  7      /* Compare and swap    ESAME */
#define PLO_DCS                  8      /* Double compare and swap   */
#define PLO_DCSG                 9      /* Double compare and swap   */
#define PLO_DCSGR               10      /* Double c and s      ESAME */
#define PLO_DCSX                11      /* Double c and s      ESAME */
#define PLO_CSST                12      /* Compare and swap and store*/
#define PLO_CSSTG               13      /* Compare and swap and store*/
#define PLO_CSSTGR              14      /* C/S/S               ESAME */
#define PLO_CSSTX               15      /* C/S/S               ESAME */
#define PLO_CSDST               16      /* C/S and double store      */
#define PLO_CSDSTG              17      /* C/S and double store      */
#define PLO_CSDSTGR             18      /* C/S/DS              ESAME */
#define PLO_CSDSTX              19      /* C/S/DS              ESAME */
#define PLO_CSTST               20      /* C/S and triple store      */
#define PLO_CSTSTG              21      /* C/S and triple store      */
#define PLO_CSTSTGR             22      /* C/S/TS              ESAME */
#define PLO_CSTSTX              23      /* C/S/TS              ESAME */

/* Bit definitions for Store Facilities List instruction */
/* Byte STFL_0: STFL/STFLE bits 0-7 */
#define STFL_0_N3               0x80    /* Instructions marked N3 in
                                           the reference summary are
                                           available in ESA/390 mode */
#define STFL_0_ESAME_INSTALLED  0x40    /* ESAME mode is available on
                                           this processor            */
#define STFL_0_ESAME_ACTIVE     0x20    /* ESAME mode is active on
                                           this processor            */
#define STFL_0_IDTE_INSTALLED   0x10    /* IDTE installed ESAME mode */
#define STFL_0_IDTE_SC_SEGTAB   0x08    /* IDTE selective clearing
                                           when segtab invalidated   */
#define STFL_0_IDTE_SC_REGTAB   0x04    /* IDTE selective clearing
                                           when regtab invalidated   */
#define STFL_0_ASN_LX_REUSE     0x02    /* ASN-and-LX-reuse facility
                                           is installed              */
#define STFL_0_STFL_EXTENDED    0x01    /* Store facility list    @Z9
                                           extended is installed  @Z9*/
/* Byte STFL_1: STFL/STFLE bits 8-15 */
#define STFL_1_ENHANCED_DAT     0x80    /* Enhanced-DAT facility  208
                                           is installed           208*/
#define STFL_1_SENSE_RUN_STATUS 0x40    /* Sense running status   @Z9
                                           facility is installed  @Z9*/
#define STFL_1_CONDITIONAL_SSKE 0x20    /* Conditional SSKE facility
                                           is installed           407*/
#define STFL_1_CONFIG_TOPOLOGY  0x10    /* STSI-enhancement for
                                           configuration topology    */
/* Byte STFL_2: STFL/STFLE bits 16-23 */
#define STFL_2_TRAN_FAC2        0x80    /* Extended translation
                                           facility 2 is installed   */
#define STFL_2_MSG_SECURITY     0x40    /* Message security assist
                                           feature is installed      */
#define STFL_2_LONG_DISPL_INST  0x20    /* Long displacement facility
                                           is installed              */
#define STFL_2_LONG_DISPL_HPERF 0x10    /* Long displacement facility
                                           has high performance      */
#define STFL_2_HFP_MULT_ADD_SUB 0x08    /* HFP multiply-add/subtract
                                           facility is installed     */
#define STFL_2_EXTENDED_IMMED   0x04    /* Extended immediate     @Z9
                                           facility is installed  @Z9*/
#define STFL_2_TRAN_FAC3        0x02    /* Extended translation
                                           facility 3 is installed   */
#define STFL_2_HFP_UNNORM_EXT   0x01    /* HFP unnormalized extension
                                           facility is installed  @Z9*/
/* Byte STFL_3: STFL/STFLE bits 24-31 */
#define STFL_3_ETF2_ENHANCEMENT 0x80    /* Extended translation   @Z9
                                           facility 2 enhancement @Z9*/
#define STFL_3_STORE_CLOCK_FAST 0x40    /* Store clock fast       @Z9
                                           enhancement installed  @Z9*/
#define STFL_3_PARSING_ENHANCE  0x20    /* Parsing-Enhancement    208
                                           facility is installed  208*/
#define STFL_3_MVCOS            0x10    /* MVCOS instruction
                                           is installed           407*/
#define STFL_3_TOD_CLOCK_STEER  0x08    /* TOD clock steering     @Z9
                                           facility is installed  @Z9*/
#define STFL_3_ETF3_ENHANCEMENT 0x02    /* Extended translation   @Z9
                                           facility 3 enhancement @Z9*/
#define STFL_3_EXTRACT_CPU_TIME 0x01    /* Extract CPU time facility
                                           is installed           407*/
/* Byte STFL_4: STFLE bits 32-39 */
#define STFL_4_CSSF             0x80    /* Compare-and-Swap-and-Store
                                           facility is installed     */
#define STFL_4_CSSF2            0x40    /* Compare-and-Swap-and-Store
                                           facility 2 is installed   */
#define STFL_4_GEN_INST_EXTN    0x20    /* General-Instr-Extn     208
                                           facility is installed  208*/
#define STFL_4_EXECUTE_EXTN     0x10    /* Execute-Extensions     208
                                           facility is installed  208*/
/* Byte STFL_5: STFLE bits 40-47 */
#define STFL_5_SET_PROG_PARAM   0x80    /* 40:Set-Program-Parameter
                                           facility installed (ESAME)*/
#define STFL_5_FPS_ENHANCEMENT  0x40    /* Floating point support
                                           enhancements (FPR-GR-loading
                                           FPS-sign-handling, and
                                           DFP-rounding) installed   */
#define STFL_5_DECIMAL_FLOAT    0x20    /* Decimal floating point
                                           (DFP) facility            */
#define STFL_5_DFP_HPERF        0x10    /* DFP has high performance  */
#define STFL_5_PFPO             0x08    /* PFPO instruction installed*/
/* Byte STFL_6: STFLE bits 48-55 */
/* Byte STFL_7: STFLE bits 56-63 */
/* Byte STFL_8: STFLE bits 64-71 */
#define STFL_8_CPU_MEAS_COUNTER 0x10    /* 67:CPU-measurement counter
                                           facility installed (ESAME)*/
#define STFL_8_CPU_MEAS_SAMPLNG 0x08    /* 68:CPU-measurement sampling
                                           facility installed (ESAME)*/

/* Bit definitions for the Vector Facility */
#define VSR_M    0x0001000000000000ULL  /* Vector mask mode bit      */
#define VSR_VCT  0x0000FFFF00000000ULL  /* Vector count              */
#define VSR_VIX  0x00000000FFFF0000ULL  /* Vector interruption index */
#define VSR_VIU  0x000000000000FF00ULL  /* Vector in-use bits        */
#define VSR_VIU0 0x0000000000008000ULL  /* Vector in-use bit vr0     */
#define VSR_VCH  0x00000000000000FFULL  /* Vector change bits        */
#define VSR_VCH0 0x0000000000000080ULL  /* Vector change bit vr0     */
#define VSR_RESV 0xFFFE000000000000ULL  /* Reserved bits             */

#define VAC_MASK 0x00FFFFFFFFFFFFFFULL  /* Vector Activity Count mask*/

/* SIE Format 1 State Descriptor Block */
typedef struct _SIE1BK {                /* SIE State Descriptor      */
/*000*/ BYTE  v;                        /* Intervention requests     */
#define SIE_V           v
#define SIE_V_WAIT      0x10            /* Wait/Run bit              */
#define SIE_V_EXTCALL   0x08            /* External call pending     */
#define SIE_V_STOP      0x04            /* SIE Stop control          */
#define SIE_V_IO        0x02            /* I/O Interrupt pending     */
#define SIE_V_EXT       0x01            /* EXT Interrupt pending     */
/*001*/ BYTE  s;                        /* State controls            */
#define SIE_S           s
#define SIE_S_T         0x80            /* Interval timer irpt pend  */
#define SIE_S_RETENTION 0x40            /* SIE State retained        */
#define SIE_S_EXP_TIMER 0x02            /* Expedite timer enabled    */
#define SIE_S_EXP_RUN   0x01            /* Expedite run enabled      */
/*002*/ BYTE  mx;                       /* Machine mode control      */
#define SIE_MX          mx
#define SIE_MX_RRF      0x80            /* Region Relocate Installed */
#define SIE_MX_XC       0x01            /* XC mode guest             */
/*003*/ BYTE  m;                        /* Mode controls             */
#define SIE_M           m
#define SIE_M_VCC       0x40            /* Vector change control     */
#define SIE_M_XA        0x20            /* XA mode guest             */
#define SIE_M_370       0x10            /* 370 mode guest            */
#define SIE_M_VR        0x08            /* V=R mode guest            */
#define SIE_M_ITMOF     0x04            /* Guest ival timer disabled */
#define SIE_M_GPE       0x01            /* Guest per enhancement     */
/*004*/ FWORD  prefix;                  /* Guest prefix register     */
/*008*/ HWORD  mso;                     /* Main Storage Origin       */
/*00A*/ HWORD  mse;                     /* Main Storage Extent       */
/*00C*/ FWORD  resv0cf;
/*010*/ FWORD  gr14;                    /* Guest gr 14               */
/*014*/ FWORD  gr15;                    /* Guest gr 15               */
/*018*/ DBLWRD psw;                     /* Guest PSW                 */
/*020*/ FWORD  resv20f;
/*024*/ FWORD  residue;                 /* Residue counter           */
/*028*/ DBLWRD cputimer;                /* CPU timer                 */
/*030*/ DBLWRD clockcomp;               /* Clock comparator          */
/*038*/ DBLWRD epoch;                   /* Guest/Host epoch diff.    */
/*040*/ FWORD  svc_ctl;                 /* SVC Controls              */
#define SIE_SVC0        svc_ctl[0]
#define SIE_SVC0_ALL    0x80            /* Intercept all SVCs        */
#define SIE_SVC0_1N     0x40            /* Intercept SVC 1n          */
#define SIE_SVC0_2N     0x20            /* Intercept SVC 2n          */
#define SIE_SVC0_3N     0x10            /* Intercept SVC 3n          */
/*044*/ HWORD lctl_ctl;                 /* LCTL Control              */
#define SIE_LCTL0       lctl_ctl[0]
#define SIE_LCTL0_CR0   0x80            /* Intercept LCTL 0          */
#define SIE_LCTL0_CR1   0x40            /* Intercept LCTL 1          */
#define SIE_LCTL0_CR2   0x20            /* Intercept LCTL 2          */
#define SIE_LCTL0_CR3   0x10            /* Intercept LCTL 3          */
#define SIE_LCTL0_CR4   0x08            /* Intercept LCTL 4          */
#define SIE_LCTL0_CR5   0x04            /* Intercept LCTL 5          */
#define SIE_LCTL0_CR6   0x02            /* Intercept LCTL 6          */
#define SIE_LCTL0_CR7   0x01            /* Intercept LCTL 7          */
#define SIE_LCTL1       lctl_ctl[1]
#define SIE_LCTL1_CR8   0x80            /* Intercept LCTL 8          */
#define SIE_LCTL1_CR9   0x40            /* Intercept LCTL 9          */
#define SIE_LCTL1_CR10  0x20            /* Intercept LCTL 10         */
#define SIE_LCTL1_CR11  0x10            /* Intercept LCTL 11         */
#define SIE_LCTL1_CR12  0x08            /* Intercept LCTL 12         */
#define SIE_LCTL1_CR13  0x04            /* Intercept LCTL 13         */
#define SIE_LCTL1_CR14  0x02            /* Intercept LCTL 14         */
#define SIE_LCTL1_CR15  0x01            /* Intercept LCTL 15         */
/*046*/ HWORD cpuad;                    /* Virtual CPU address       */
/*048*/ FWORD ic;                       /* Interception Controls     */
#define SIE_IC0         ic[0]
#define SIE_IC0_OPEREX  0x80            /* Intercept operation exc.  */
#define SIE_IC0_PRIVOP  0x40            /* Intercept priv. op. exc.  */
#define SIE_IC0_PGMALL  0x20            /* Intercept program ints    */
#define SIE_IC0_TS1     0x08            /* Intercept TS cc1          */
#define SIE_IC0_CS1     0x04            /* Intercept CS cc1          */
#define SIE_IC0_CDS1    0x02            /* Intercept CDS cc1         */
#define SIE_IC0_IPTECSP 0x01            /* Intercept IPTE or CSP     */
#define SIE_IC1         ic[1]
#define SIE_IC1_LPSW    0x40            /* Intercept LPSW            */
#define SIE_IC1_PXLB    0x20            /* Intercept PTLB or PALB    */
#define SIE_IC1_SSM     0x10            /* Intercept SSM             */
#define SIE_IC1_BSA     0x08            /* Intercept BSA             */
#define SIE_IC1_STCTL   0x04            /* Intercept STCTL           */
#define SIE_IC1_STNSM   0x02            /* Intercept STNSM           */
#define SIE_IC1_STOSM   0x01            /* Intercept STOSM           */
#define SIE_IC2         ic[2]
#define SIE_IC2_STCK    0x80            /* Intercept STCK            */
#define SIE_IC2_ISKE    0x40            /* Intercept ISK/ISKE        */
#define SIE_IC2_SSKE    0x20            /* Intercept SSK/SSKE        */
#define SIE_IC2_RRBE    0x10            /* Intercept RRB/RRBE        */
#define SIE_IC2_PC      0x08            /* Intercept PC              */
#define SIE_IC2_PT      0x04            /* Intercept PT              */
#define SIE_IC2_TPROT   0x02            /* Intercept TPROT           */
#define SIE_IC2_LASP    0x01            /* Intercept LASP            */
#define SIE_IC3         ic[3]
#define SIE_IC3_VACSV   0x80            /* Intercept VACSV           */
#define SIE_IC3_SPT     0x40            /* Intercept SPT and STPT    */
#define SIE_IC3_SCKC    0x20            /* Intercept SCKC and STCKC  */
#define SIE_IC3_VACRS   0x10            /* Intercept VACRS           */
#define SIE_IC3_PR      0x08            /* Intercept PR              */
#define SIE_IC3_BAKR    0x04            /* Intercept BAKR            */
#define SIE_IC3_PGX     0x02            /* Intercept PGIN/PGOUT      */
/*04C*/ FWORD ec;                       /* Execution Controls        */
#define SIE_EC0         ec[0]
#define SIE_EC0_EXTA    0x80            /* External Interrupt Assist */
#define SIE_EC0_INTA    0x40            /* Intervention Bypass Assist*/
#define SIE_EC0_WAIA    0x20            /* Wait State Assist         */
#define SIE_EC0_SIGPA   0x10            /* SIGP Assist               */
#define SIE_EC0_ALERT   0x08            /* Alert Monitoring          */
#define SIE_EC0_IOA     0x04            /* I/O Assist                */
#define SIE_EC0_MVPG    0x01            /* Interpret MVPG and IESBE  */
#define SIE_EC1         ec[1]
#define SIE_EC1_EC370   0x20            /* 370 I/O Assist            */
#define SIE_EC1_VFONL   0x04            /* Virtual VF online         */
#define SIE_EC2         ec[2]
#define SIE_EC2_PROTEX  0x20            /* Intercept prot exception  */
#define SIE_EC3         ec[3]
#define SIE_EC3_SIGAA   0x04            /* SIGA Assist               */
/*050*/ BYTE  c;                        /* Interception Code         */
#define SIE_C_INST         4            /* Instruction interception  */
#define SIE_C_PGMINT       8            /* Program interruption      */
#define SIE_C_PGMINST     12            /* Program/instruction int   */
#define SIE_C_EXTREQ      16            /* External request          */
#define SIE_C_EXTINT      20            /* External interruption     */
#define SIE_C_IOREQ       24            /* I/O request               */
#define SIE_C_WAIT        28            /* Wait state                */
#define SIE_C_VALIDITY    32            /* Validity                  */
#define SIE_C_STOPREQ     40            /* Stop request              */
#define SIE_C_OPEREXC     44            /* Operation Exception       */
#define SIE_C_IOINT       60            /* I/O Interruption          */
#define SIE_C_IOINST      64            /* I/O Instruction           */
#define SIE_C_EXP_RUN     68            /* Expedited Run Intercept   */
#define SIE_C_EXP_TIMER   72            /* Expedited Timer Intercept */
/*051*/ BYTE  f;                        /* Interception Status       */
#define SIE_F           f
#define SIE_F_IN        0x80            /* Intercept format 2        */
#define SIE_F_IF        0x02            /* Instruction fetch PER     */
#define SIE_F_EX        0x01            /* Icept for target of EX    */
/*052*/ HWORD lhcpu;                    /* Last Host CPU addr        */
/*054*/ HWORD todpf;                    /* TOD programmable field    */
/*056*/ HWORD ipa;                      /* Instruction parameter A   */
/*058*/ FWORD ipb;                      /* Instruction parameter B   */
/*05C*/ FWORD ipc;                      /* Instruction parameter C   */
/*060*/ FWORD rcpo;                     /* RCP area origin           */
#define SIE_RCPO0       rcpo[0]
#define SIE_RCPO0_SKA   0x80            /* Storage Key Assist        */
#define SIE_RCPO0_SKAIP 0x40            /* SKA in progress           */
#define SIE_RCPO2       rcpo[2]
#define SIE_RCPO2_RCPBY 0x10            /* RCP Bypass                */
/*064*/ FWORD scao;                     /* SCA area origin           */
/*068*/ FWORD subchtabo;                /* Subchannel table origin   */
/*06C*/ FWORD resv6Cf;
/*070*/ HWORD tch_ctl;                  /* Test Channel control      */
/*072*/ HWORD resv72h;
/*074*/ BYTE  zone;                     /* Zone Number               */
/*075*/ BYTE  resv075;
/*076*/ BYTE  tschds;                   /* TSCH device status        */
/*077*/ BYTE  tschsc;                   /* TSCH subchannel status    */
/*078*/ BYTE  xslim[3];                 /* Extended stor upper lim   */
/*07B*/ BYTE  resv7Bb;
/*07C*/ FWORD resv7Cf;
/*080*/ FWORD cr[16];                   /* Guest Control registers   */
/*0C0*/ BYTE  ip[34];                   /* Interruption parameters   */
#define SIE_IP_PSA_OFFSET   0x40        /* Offset of the IP field
                                           relative to the ipfields
                                           in the PSA                */
#define SIE_II_PSA_OFFSET   0x30        /* Offset of the IP field
                                           relative to the I/O fields
                                           in the PSA for ESAME guest*/
/*0E2*/ BYTE  xso[3];                   /* Expanded storage origin   */
/*0E5*/ BYTE  xsl[3];                   /* Expanded storage limit    */
/*0E8*/ BYTE  resvE8b[24];
} SIE1BK;

/* SIE Format 2 State Descriptor Block */
typedef struct _SIE2BK {                /* SIE State Descriptor      */
/*000*/ BYTE  v;                        /* Intervention requests     */
#define SIE_V           v
#define SIE_V_WAIT      0x10            /* Wait/Run bit              */
#define SIE_V_EXTCALL   0x08            /* External call pending     */
#define SIE_V_STOP      0x04            /* SIE Stop control          */
#define SIE_V_IO        0x02            /* I/O Interrupt pending     */
#define SIE_V_EXT       0x01            /* EXT Interrupt pending     */
/*001*/ BYTE  s;                        /* State controls            */
#define SIE_S           s
#define SIE_S_T         0x80            /* Interval timer irpt pend  */
#define SIE_S_RETENTION 0x40            /* SIE State retained        */
#define SIE_S_EXP_TIMER 0x02            /* Expedite timer enabled    */
#define SIE_S_EXP_RUN   0x01            /* Expedite run enabled      */
/*002*/ BYTE  mx;                       /* Machine mode control      */
#define SIE_MX          mx
#define SIE_MX_RRF      0x80            /* Region Relocate Installed */
#define SIE_MX_XC       0x01            /* XC mode guest             */
#define SIE_MX_ESAME    0x08            /* ESAME mode guest          */
/*003*/ BYTE  m;                        /* Mode controls             */
#define SIE_M           m
#define SIE_M_VCC       0x40            /* Vector change control     */
#define SIE_M_XA        0x20            /* XA mode guest             */
#define SIE_M_370       0x10            /* 370 mode guest            */
#define SIE_M_VR        0x08            /* V=R mode guest            */
#define SIE_M_ITMOF     0x04            /* Guest ival timer disabled */
#define SIE_M_GPE       0x01            /* Guest per enhancement     */
/*004*/ FWORD  prefix;                  /* Guest prefix register     */
/*008*/ FWORD  resv008f;
/*00C*/ FWORD  resv00cf;
/*010*/ DBLWRD resv010d;
/*018*/ DBLWRD resv018d;
/*020*/ DBLWRD resv020d;
/*028*/ DBLWRD cputimer;                /* CPU timer                 */
/*030*/ DBLWRD clockcomp;               /* Clock comparator          */
/*038*/ DBLWRD epoch;                   /* Guest/Host epoch diff.    */
/*040*/ FWORD  svc_ctl;                 /* SVC Controls              */
#define SIE_SVC0        svc_ctl[0]
#define SIE_SVC0_ALL    0x80            /* Intercept all SVCs        */
#define SIE_SVC0_1N     0x40            /* Intercept SVC 1n          */
#define SIE_SVC0_2N     0x20            /* Intercept SVC 2n          */
#define SIE_SVC0_3N     0x10            /* Intercept SVC 3n          */
/*044*/ HWORD lctl_ctl;                 /* LCTL Control              */
#define SIE_LCTL0       lctl_ctl[0]
#define SIE_LCTL0_CR0   0x80            /* Intercept LCTL 0          */
#define SIE_LCTL0_CR1   0x40            /* Intercept LCTL 1          */
#define SIE_LCTL0_CR2   0x20            /* Intercept LCTL 2          */
#define SIE_LCTL0_CR3   0x10            /* Intercept LCTL 3          */
#define SIE_LCTL0_CR4   0x08            /* Intercept LCTL 4          */
#define SIE_LCTL0_CR5   0x04            /* Intercept LCTL 5          */
#define SIE_LCTL0_CR6   0x02            /* Intercept LCTL 6          */
#define SIE_LCTL0_CR7   0x01            /* Intercept LCTL 7          */
#define SIE_LCTL1       lctl_ctl[1]
#define SIE_LCTL1_CR8   0x80            /* Intercept LCTL 8          */
#define SIE_LCTL1_CR9   0x40            /* Intercept LCTL 9          */
#define SIE_LCTL1_CR10  0x20            /* Intercept LCTL 10         */
#define SIE_LCTL1_CR11  0x10            /* Intercept LCTL 11         */
#define SIE_LCTL1_CR12  0x08            /* Intercept LCTL 12         */
#define SIE_LCTL1_CR13  0x04            /* Intercept LCTL 13         */
#define SIE_LCTL1_CR14  0x02            /* Intercept LCTL 14         */
#define SIE_LCTL1_CR15  0x01            /* Intercept LCTL 15         */
/*046*/ HWORD cpuad;                    /* Virtual CPU address       */
/*048*/ FWORD ic;                       /* Interception Controls     */
#define SIE_IC0         ic[0]
#define SIE_IC0_OPEREX  0x80            /* Intercept operation exc.  */
#define SIE_IC0_PRIVOP  0x40            /* Intercept priv. op. exc.  */
#define SIE_IC0_PGMALL  0x20            /* Intercept program ints    */
#define SIE_IC0_TS1     0x08            /* Intercept TS cc1          */
#define SIE_IC0_CS1     0x04            /* Intercept CS cc1          */
#define SIE_IC0_CDS1    0x02            /* Intercept CDS cc1         */
#define SIE_IC0_IPTECSP 0x01            /* Intercept IPTE or CSP     */
#define SIE_IC1         ic[1]
#define SIE_IC1_LPSW    0x40            /* Intercept LPSW/LPSWE      */
#define SIE_IC1_PXLB    0x20            /* Intercept PTLB or PALB    */
#define SIE_IC1_SSM     0x10            /* Intercept SSM             */
#define SIE_IC1_BSA     0x08            /* Intercept BSA             */
#define SIE_IC1_STCTL   0x04            /* Intercept STCTL           */
#define SIE_IC1_STNSM   0x02            /* Intercept STNSM           */
#define SIE_IC1_STOSM   0x01            /* Intercept STOSM           */
#define SIE_IC2         ic[2]
#define SIE_IC2_STCK    0x80            /* Intercept STCK            */
#define SIE_IC2_ISKE    0x40            /* Intercept ISK/ISKE        */
#define SIE_IC2_SSKE    0x20            /* Intercept SSK/SSKE        */
#define SIE_IC2_RRBE    0x10            /* Intercept RRB/RRBE        */
#define SIE_IC2_PC      0x08            /* Intercept PC              */
#define SIE_IC2_PT      0x04            /* Intercept PT              */
#define SIE_IC2_TPROT   0x02            /* Intercept TPROT           */
#define SIE_IC2_LASP    0x01            /* Intercept LASP            */
#define SIE_IC3         ic[3]
#define SIE_IC3_VACSV   0x80            /* Intercept VACSV           */
#define SIE_IC3_SPT     0x40            /* Intercept SPT and STPT    */
#define SIE_IC3_SCKC    0x20            /* Intercept SCKC and STCKC  */
#define SIE_IC3_VACRS   0x10            /* Intercept VACRS           */
#define SIE_IC3_PR      0x08            /* Intercept PR              */
#define SIE_IC3_BAKR    0x04            /* Intercept BAKR            */
#define SIE_IC3_PGX     0x02            /* Intercept PGIN/PGOUT      */
/*04C*/ FWORD ec;                       /* Execution Controls        */
#define SIE_EC0         ec[0]
#define SIE_EC0_EXTA    0x80            /* External Interrupt Assist */
#define SIE_EC0_INTA    0x40            /* Intervention Bypass Assist*/
#define SIE_EC0_WAIA    0x20            /* Wait State Assist         */
#define SIE_EC0_SIGPA   0x10            /* SIGP Assist               */
#define SIE_EC0_ALERT   0x08            /* Alert Monitoring          */
#define SIE_EC0_IOA     0x04            /* I/O Assist                */
#define SIE_EC0_MVPG    0x01            /* Interpret MVPG and IESBE  */
#define SIE_EC1         ec[1]
#define SIE_EC1_EC370   0x20            /* 370 I/O Assist            */
#define SIE_EC1_VFONL   0x04            /* Virtual VF online         */
#define SIE_EC2         ec[2]
#define SIE_EC2_PROTEX  0x20            /* Intercept prot exception  */
#define SIE_EC3         ec[3]
#define SIE_EC3_SIGAA   0x04            /* SIGA Assist               */
/*050*/ BYTE  c;                        /* Interception Code         */
#define SIE_C_INST         4            /* Instruction interception  */
#define SIE_C_PGMINT       8            /* Program interruption      */
#define SIE_C_PGMINST     12            /* Program/instruction int   */
#define SIE_C_EXTREQ      16            /* External request          */
#define SIE_C_EXTINT      20            /* External interruption     */
#define SIE_C_IOREQ       24            /* I/O request               */
#define SIE_C_WAIT        28            /* Wait state                */
#define SIE_C_VALIDITY    32            /* Validity                  */
#define SIE_C_STOPREQ     40            /* Stop request              */
#define SIE_C_OPEREXC     44            /* Operation Exception       */
#define SIE_C_EXP_RUN     68            /* Expedited Run Intercept   */
#define SIE_C_EXP_TIMER   72            /* Expedited Timer Intercept */
/*051*/ BYTE  f;                        /* Interception Status       */
#define SIE_F           f
#define SIE_F_IN        0x80            /* Intercept format 2        */
#define SIE_F_IF        0x02            /* Instruction fetch PER     */
#define SIE_F_EX        0x01            /* Icept for target of EX    */
/*052*/ HWORD lhcpu;                    /* Last Host CPU addr        */
/*054*/ HWORD resv054h;
/*056*/ HWORD ipa;                      /* Instruction parameter A   */
#define vi_who  ipa[0]
#define vi_when ipa[1]
#define vi_why  ipb
#define vi_zero ipb+2
/*058*/ FWORD ipb;                      /* Instruction parameter B   */
/*05C*/ FWORD ipc;                      /* Instruction parameter C   */
/*060*/ FWORD rcpo;                     /* RCP area origin           */
#define SIE_RCPO0       rcpo[0]
#define SIE_RCPO0_SKA   0x80            /* Storage Key Assist        */
#define SIE_RCPO0_SKAIP 0x40            /* SKA in progress           */
#define SIE_RCPO2       rcpo[2]
#define SIE_RCPO2_RCPBY 0x10            /* RCP Bypass                */
/*064*/ FWORD  scao;                    /* SCA area origin           */
/*068*/ FWORD  resv068f;
/*06C*/ HWORD  todpfh;                  /* TOD pf high half          */
/*06E*/ HWORD  todpf;                   /* TOD programmable field    */
/*070*/ FWORD  resv070f;
/*074*/ BYTE   zone;                    /* Zone Number               */
/*075*/ BYTE   resv075;
/*076*/ BYTE   tschds;                  /* TSCH device status        */
/*077*/ BYTE   tschsc;                  /* TSCH subchannel status    */
/*078*/ FWORD  resv078f;
/*07C*/ FWORD  resv07cf;
/*080*/ DBLWRD mso;                     /* Main Storage Origin       */
/*088*/ DBLWRD mse;                     /* Main Storage Extend
                                           The actual guest machine
                                           size is (mse+1)-mso       */
#define SIE2_MS_MASK    0xFFFFFFFFFFF00000ULL
/*090*/ QWORD  psw;                     /* Guest PSW                 */
/*0A0*/ DBLWRD gr14;                    /* Guest gr 14               */
/*0A8*/ DBLWRD gr15;                    /* Guest gr 15               */
/*0B0*/ DBLWRD recv0b0d;
/*0B8*/ HWORD  recv0b8d;
/*0BA*/ BYTE   xso[3];                  /* Expanded storage origin   */
/*0BD*/ BYTE   xsl[3];                  /* Expanded storage limit    */
/*0C0*/ BYTE   ip[52];                  /* Interruption parameters   */
#define SIE_IP_PSA_OFFSET       0x40    /* Offset of the IP field
                                           relative to the ipfields
                                           in the PSA for ESAME guest*/
#define SIE_II_PSA_OFFSET       0x30    /* Offset of the IP field
                                           relative to the I/O fields
                                           in the PSA for ESAME guest*/
/*0F4*/ BYTE   resv0f4b[6];
/*0FA*/ HWORD  ief;                     /* Migration Emulation cnlt  */
/*0FC*/ FWORD  resv0fcf;
/*100*/ DBLWRD cr[16];                  /* Control registers         */
/*180*/ BYTE  resv180b[128];
} SIE2BK;


#define SIE_VI_WHO_LVLM   0xF0     /* Mask for "source level" field:
                                      If non-zero, this is the inter-
                                      pretive-execution depth at which
                                      the problem was originally
                                      detected.  It is set to 1 (or 2
                                      for Interpreted SIE) by the
                                      entity (hardware or vSIE software)
                                      that reports the problem, and is
                                      incremented (to a max of 15) by
                                      every level of vSIE that passes
                                      the interception along.        */
#define SIE_VI_WHO_LVL1   0x10     /* Condition recognized one level
                                      down in interpretive execution */
#define SIE_VI_WHO_LVLMX  0xF0     /* Maximum source level reported  */
#define SIE_VI_WHO_INITM  0x0F     /* Mask for "initiator" field,
                                      identifying the type of entity
                                      that detected the problem.     */
#define SIE_VI_WHO_CPU    0x01     /* Initiator was a CPU            */
#define SIE_VI_WHO_VSIE   0x08     /* Initiator was vSIE software    */
#define SIE_VI_WHEN_RECPM 0xF0     /* Mask for "recognition point" field
                                      the normal processing that
                                      recognized the condition
                                      necessitating the validity
                                      interception.                  */
#define SIE_VI_WHEN_SIENT 0x10     /* Condition recognized during SIE
                                      entry                          */
#define SIE_VI_WHEN_INST  0x20     /* Condition recognized during
                                      instruction interpretation     */
#define SIE_VI_WHEN_IRPT  0x30     /* Condition recognized during
                                      interruption interpretation    */
#define SIE_VI_WHEN_SIEXT 0x40     /* Condition recognized during SIE
                                      exit                           */
#define SIE_VI_WHY_MODE  0x0001    /* Invalid guest mode or invalid
                                      combination of modes           */
#define SIE_VI_WHY_ARCHM 0x0002    /* Invalid architecture mode specified
                                      (neither or both S/370 and ESA/390) */
#define SIE_VI_WHY_370NI 0x0003    /* S/370 interpretation requested but
                                      not installed                  */
#define SIE_VI_WHY_PRMCS 0x0004    /* Preferred and MCDS modes specified
                                      together                       */
#define SIE_VI_WHY_MCS37 0x0005    /* MCDS and S/370 modes specified
                                      together                       */
#define SIE_VI_WHY_RRFNI 0x0006    /* RRF requested but not installed */
#define SIE_VI_WHY_ISINI 0x0007    /* iSIE requested but not installed */
#define SIE_VI_WHY_PFOUT 0x0010    /* Guest prefix outside guest extent */
#define SIE_VI_WHY_SCHPF 0x0011    /* SCA origin nonzero and its frame
                                      address matches host prefix reg */
#define SIE_VI_WHY_SDOVL 0x0030    /* State description overlaps guest
                                      storage                        */
#define SIE_VI_WHY_SCOVL 0x0031    /* SCA overlaps guest storage     */
#define SIE_VI_WHY_APOVL 0x0032    /* APCB overlaps guest storage    */
#define SIE_VI_WHY_SCADR 0x0034    /* SCA at invalid host address    */
#define SIE_VI_WHY_APADR 0x0035    /* APCB at invalid host address   */
#define SIE_VI_WHY_PFACC 0x0037    /* Access exception on guest prefix
                                      area                           */
#define SIE_VI_WHY_SCZER 0x0038    /* SCA origin nonzero but under 4K
                                      (8K for ESAME host)            */
#define SIE_VI_WHY_APZER 0x0039    /* APCB origin is zero when an
                                      APCB is needed                 */
#define SIE_VI_WHY_PSADR 0x003A    /* PGSTE at invalid host address  */
#define SIE_VI_WHY_SCBDY 0x003B    /* SCA crosses 4KB boundary       */
#define SIE_VI_WHY_APBDY 0x003C    /* APCB crosses 4KB boundary      */
#define SIE_VI_WHY_MSLEX 0x003D    /* MSL exceeds maximum host
                                      address supported for guest
                                      storage (ESAME SIE only)       */
#define SIE_VI_WHY_MSDEF 0x0041    /* MSO exceeds MSL (ESAME SIE only) */
#define SIE_VI_WHY_MSDF2 0x0042    /* Alternate for MSDEF detected
                                      during guest prefix access     */
#define SIE_VI_WHY_RRHPF 0x0046    /* RRF guest extent (zone) includes
                                      host prefix area               */
#define SIE_VI_WHY_PRHPF 0x0050    /* Preferred guest extent includes
                                      host prefix area               */
#define SIE_VI_WHY_MSONZ 0x0051    /* MSO not zero for preferred guest */
#define SIE_VI_WHY_CDXNI 0x0070    /* Crypto Domain Index not installed */
#define SIE_VI_WHY_DRFNI 0x1001    /* DRF requested but not installed */
#define SIE_VI_WHY_PRALE 0x1002    /* Alerting enabled for preferred
                                      non-DRF guest                  */
#define SIE_VI_WHY_AZNNI 0x1005    /* Zone identified by AZN is not
                                      installed                      */
#define SIE_VI_WHY_AZNNZ 0x1006    /* Nonzero AZN for non-RRF/DRF guest
                                      (or level-1 DSC nonzero for
                                      pageable guest)                */
/* The following occur only under ESA/390 SIE: */
#define SIE_VI_WHY_MSSTL 0x0043    /* Guest extent exceeds host STL  */
#define SIE_VI_WHY_HOSTF 0x0060    /* Invalid host translation format
                                      (CR0.8-12)                     */
#define SIE_VI_WHY_MSOFL 0x0061    /* Pageable guest extent exceeds
                                      2G-1                           */
#define SIE_VI_WHY_RCOFL 0x0062    /* RCP area extends beyond 2G-1 in
                                      host virtual                   */
#define SIE_VI_WHY_RCSTL 0x0063    /* RCP area exceeds host STL      */
#define SIE_VI_WHY_SCOFL 0x0064    /* SCA extends beyond 2G-1        */
#define SIE_VI_WHY_APOFL 0x0065    /* APCB extends beyond 2G-1       */
#define SIE_VI_WHY_RCZER 0x0067    /* RCP area origin is zero        */
#define SIE_VI_WHY_PFSTL 0x0068    /* Guest prefix exceeds host STL  */
#define SIE_VI_WHY_PTBDY 0x0069    /* PTO/PGST not on 2K boundary    */
#define SIE_VI_WHY_MSODS 0x006A    /* MSO nonzero for MCDS guest     */
#define SIE_VI_WHY_SNOVL 0x1009    /* SNT overlaps guest storage     */
#define SIE_VI_WHY_SNHPF 0x100C    /* SNT overlaps host prefix area  */
/* The following occur only in virtual machines under VM/ESA: */
#define SIE_VI_WHY_PFRDO 0xF000    /* Guest prefix maps to read-only
                                      storage (e.g. in a DCSS)       */
#define SIE_VI_WHY_SCRDO 0xF001    /* SCA in read-only storage       */
#define SIE_VI_WHY_OBMSB 0xF003    /* MSO/MSE not multiple of 1Meg...
                                      ..not supported in ESAME gen   */

/* Zone Parameter Block */
typedef struct _ZPB1 {
        FWORD   mso;                    /* Main Storage Origin
                                           bits 0-15 must be 0       */
        FWORD   msl;                    /* Main Storage Limit
                                           bits 0-15 must be 0       */
        FWORD   eso;                    /* Expanded Storage Origin
                                           bits 0-7 must be 0        */
        FWORD   esl;                    /* Expanded Storage Limit
                                           bits 0-7 must be 0        */
        FWORD   res[4];                 /* Reserved bits - must be 0 */
} ZPB1;

typedef struct _ZPB2 {
        DBLWRD  mso;                    /* Main Storage Origin
                                           bits 0-19 must be 0       */
        DBLWRD  msl;                    /* Main Storage Limit
                                           bits 0-19 must be 0       */
#define ZPB2_MS_VALID 0x00000FFFFFFFFFFFULL
        DBLWRD  eso;                    /* Expanded Storage Origin
                                           bits 0-7 must be 0        */
        DBLWRD  esl;                    /* Expanded Storage Limit
                                           bits 0-7 must be 0        */
#define ZPB2_ES_VALID 0x00FFFFFFFFFFFFFFULL
} ZPB2;

#define LKPG_GPR0_LOCKBIT       0x00000200
#define LKPG_GPR0_RESV          0x0000FD00

#define STSI_GPR0_FC_MASK       0xF0000000
#define STSI_GPR0_FC_CURRNUM    0x00000000
#define STSI_GPR0_FC_BASIC      0x10000000
#define STSI_GPR0_FC_LPAR       0x20000000
#define STSI_GPR0_FC_VM         0x30000000
#define STSI_GPR0_FC_CURRINFO   0xF0000000
#define STSI_GPR0_SEL1_MASK     0x000000FF
#define STSI_GPR0_RESERVED      0x0FFFFF00

#define STSI_GPR1_SEL2_MASK     0x0000FFFF
#define STSI_GPR1_RESERVED      0xFFFF0000

typedef struct _SYSIB111 {              /* Basic Machine Config      */
        BYTE    flag1;                  /* 1.1.1 SYSIB Flag          */
#define SYSIB111_PFLAG  0x80            /* Type percentage present   */
        BYTE    resv1[3];               /* Reserved                  */
        FWORD   resv2[7];               /* Reserved                  */
        BYTE    manufact[16];           /* Manufacturer              */
        BYTE    type[4];                /* Type                      */
        FWORD   resv3[3];               /* Reserved                  */
        BYTE    modcapaid[16];          /* Model capacity identifier */
        BYTE    seqc[16];               /* Sequence Code             */
        BYTE    plant[4];               /* Plant of manufacture      */
        BYTE    model[16];              /* System Model              */
        BYTE    mpci[16];               /* Model Perm Capacity ID    */
        BYTE    mtci[16];               /* Model Temp Capacity ID    */
        FWORD   mcaprating;             /* Model Capacity Rating     */
        FWORD   mpcaprating;            /* Model Perm Capacity Rating*/
        FWORD   mtcaprating;            /* Model temp Capacity Rating*/
        BYTE    typepct[5];             /* Secondary CPU types pct   */
    }   SYSIB111;

typedef struct _SYSIB121 {              /* Basic Machine CPU         */
        FWORD   resv1[20];              /* Reserved                  */
        BYTE    seqc[16];               /* Sequence Code             */
        BYTE    plant[4];               /* Plant of manufacture      */
        HWORD   resv2;                  /* Reserved                  */
        HWORD   cpuad;                  /* CPU address               */
    }   SYSIB121;

typedef struct _SYSIB221 {              /* Logical partition CPU     */
        FWORD   resv1[20];              /* Reserved                  */
        BYTE    seqc[16];               /* Logical CPU Sequence Code */
        BYTE    plant[4];               /* Plant of manufacture      */
        HWORD   lcpuid;                 /* Logical CPU ID            */
        HWORD   cpuad;                  /* CPU address               */
    }   SYSIB221;

typedef struct _SYSIB222 {              /* Logical partition CPUs    */
        FWORD   resv1[8];               /* Reserved                  */
        HWORD   lparnum;                /* LPAR number               */
        BYTE    resv2;                  /* Reserved                  */
        BYTE    lcpuc;                  /* Logical CPU characteristic*/
#define SYSIB222_LCPUC_DEDICATED    0x80
#define SYSIB222_LCPUC_SHARED       0x40
#define SYSIB222_LCPUC_CAPPED       0x20
        HWORD   totcpu;                 /* Total CPU count           */
        HWORD   confcpu;                /* Configured CPU count      */
        HWORD   sbcpu;                  /* Standby CPU count         */
        HWORD   resvcpu;                /* Reserved CPU count        */
        BYTE    lparname[8];            /* LPAR name                 */
        FWORD   lparcaf;                /* LPAR capability adjustment*/
        FWORD   mdep[2];                /* Model Dependent           */
        FWORD   resv3[2];               /* Reserved                  */
        HWORD   dedcpu;                 /* Dedicated CPU count       */
        HWORD   shrcpu;                 /* Shared CPU count          */
    }   SYSIB222;

typedef struct _SYSIB322 {              /* Virtual Machines CPUs     */
        BYTE    resv1[4*7];             /* Reserved                  */
        BYTE    resv2[3*1];             /* Reserved                  */
        BYTE    dbct;                   /* Four bit desc block count */
        BYTE    vmdb[4*16];             /* Virtual Machine desc block*/
    }   SYSIB322;

typedef struct _SYSIB1512 {             /* Configuration Topology    */
        HWORD   resv1;                  /* Reserved                  */
        HWORD   len;                    /* Length                    */
        BYTE    mag[6];                 /* Magnitudes 6, 5, ... 1    */
        BYTE    resv2;                  /* Reserved                  */
        BYTE    mnest;                  /* Nesting Level             */
        FWORD   resv3;                  /* Reserved                  */
        BYTE    tles[FLEXIBLE_ARRAY];   /* Topology List Entries     */
    }   SYSIB1512;

typedef struct _TLECNTNR {              /* Container TLE             */
        BYTE    nl;                     /* Nesting Level             */
        BYTE    resv1[3];               /* Reserved                  */
        BYTE    resv2;                  /* Reserved                  */
        BYTE    resv3[2];               /* Reserved                  */
        BYTE    cntnrid;                /* Container Id              */
    }   TLECNTNR;

typedef struct _TLECPU {                /* CPU TLE                   */
        BYTE    nl;                     /* Nesting Level             */
        BYTE    resv1[3];               /* Reserved                  */
        BYTE    flags;                  /* Flags                     */
        BYTE    cputype;                /* CPU Type                  */
        U16     cpuadorg;               /* CPU Address Origin        */
        DW      cpumask;                /* CPU Mask                  */
    }   TLECPU;

#define CPUTLE_FLAG_RESERVED    0xF8
#define CPUTLE_FLAG_DEDICATED   0x04
#define CPUTLE_FLAG_VPOLARMASK  0x03

typedef struct _SYSIBVMDB {             /* Virtual Machine Desc Block*/
        BYTE    resv1[4*1];             /* Reserved                  */
        HWORD   totcpu;                 /* Total CPU count           */
        HWORD   confcpu;                /* Configured CPU count      */
        HWORD   sbcpu;                  /* Standby CPU count         */
        HWORD   resvcpu;                /* Reserved CPU count        */
        BYTE    vmname[8];              /* VM userid                 */
        FWORD   vmcaf;                  /* VM capability adjustment  */
        BYTE    cpid[4*4];              /* Control Program ID        */
    }   SYSIBVMDB;

#define PTFF_GPR0_RESV          0x00000080
#define PTFF_GPR0_FC_MASK       0x0000007F
#define PTFF_GPR0_FC_QAF        0x00
#define PTFF_GPR0_FC_QTO        0x01
#define PTFF_GPR0_FC_QSI        0x02
#define PTFF_GPR0_FC_QPT        0x03
#define PTFF_GPR0_FC_ATO        0x40
#define PTFF_GPR0_FC_STO        0x41
#define PTFF_GPR0_FC_SFS        0x42
#define PTFF_GPR0_FC_SGS        0x43

typedef struct _PTFFQAF {               /* Query Available Functions */
        FWORD   sb[4];                  /* Status Bits words         */
    }   PTFFQAF;

typedef struct _PTFFQTO {               /* Query TOD Offset          */
        DBLWRD  physclk;                /* Physical Clock            */
        DBLWRD  todoff;                 /* TOD Offset                */
        DBLWRD  ltodoff;                /* Logical TOD Offset        */
        DBLWRD  todepoch;               /* TOD Epoch Difference      */
    }   PTFFQTO;

typedef struct _PTFFQSI {               /* Query Steering Information*/
        DBLWRD  physclk;                /* Physical Clock            */
        DBLWRD  oldestart;              /* Old Episode Start Time    */
        DBLWRD  oldebase;               /* Old Episode Base Offset   */
        FWORD   oldfsr;                 /* Old Episode Fine St. Rate */
        FWORD   oldgsr;                 /* Old Episode Gross St. Rate*/
        DBLWRD  newestart;              /* New Episode Start Time    */
        DBLWRD  newebase;               /* New Episode Base Offset   */
        FWORD   newfsr;                 /* New Episode Fine St. Rate */
        FWORD   newgsr;                 /* New Episode Gross St. Rate*/
    }   PTFFQSI;


#define SIGA_FC_W       0               /* Initiate Output           */
#define SIGA_FC_R       1               /* Initiate Input            */
#define SIGA_FC_S       2               /* Synchronize               */

#define SIGA_FC_MAX     SIGA_FC_S

/* Bit definitions for floating-point-control register */
#define FPC_MASK        0xF8000000
#define FPC_MASK_IMI    0x80000000
#define FPC_MASK_IMZ    0x40000000
#define FPC_MASK_IMO    0x20000000
#define FPC_MASK_IMU    0x10000000
#define FPC_MASK_IMX    0x08000000
#define FPC_FLAG        0x00F80000
#define FPC_FLAG_SFI    0x00800000
#define FPC_FLAG_SFZ    0x00400000
#define FPC_FLAG_SFO    0x00200000
#define FPC_FLAG_SFU    0x00100000
#define FPC_FLAG_SFX    0x00080000
#define FPC_DXC         0x0000FF00
#define FPC_DXC_I       0x00008000
#define FPC_DXC_Z       0x00004000
#define FPC_DXC_O       0x00002000
#define FPC_DXC_U       0x00001000
#define FPC_DXC_X       0x00000800
#define FPC_DXC_Y       0x00000400
#define FPC_DRM         0x00000070
#define FPC_BRM         0x00000003
#define FPC_RESERVED    0x0707008C

/* Shift counts to allow alignment of each field in the FPC register */
#define FPC_MASK_SHIFT  27
#define FPC_FLAG_SHIFT  19
#define FPC_DXC_SHIFT   8
#define FPC_DRM_SHIFT   4
#define FPC_BRM_SHIFT   0

/* Data exception codes */
#define DXC_DECIMAL             0x00    /* Decimal operand exception */
#define DXC_AFP_REGISTER        0x01    /* AFP register exception    */
#define DXC_BFP_INSTRUCTION     0x02    /* BFP instruction exception */
#define DXC_DFP_INSTRUCTION     0x03    /* DFP instruction exception */
#define DXC_IEEE_INEXACT_TRUNC  0x08    /* IEEE inexact, truncated   */
#define DXC_IEEE_INEXACT_IISE   0x0B    /* IEEE inexact (IISE)    DFP*/
#define DXC_IEEE_INEXACT_INCR   0x0C    /* IEEE inexact, incremented */
#define DXC_IEEE_UF_EXACT       0x10    /* IEEE underflow. exact     */
#define DXC_IEEE_UF_EXACT_IISE  0x13    /* IEEE u/flow,exact(IISE)DFP*/
#define DXC_IEEE_UF_INEX_TRUNC  0x18    /* IEEE u/flow,inexact,trunc */
#define DXC_IEEE_UF_INEX_IISE   0x1B    /* IEEE u/flow,inex(IISE) DFP*/
#define DXC_IEEE_UF_INEX_INCR   0x1C    /* IEEE u/flow,inexact,incr  */
#define DXC_IEEE_OF_EXACT       0x20    /* IEEE overflow. exact      */
#define DXC_IEEE_OF_EXACT_IISE  0x23    /* IEEE o/flow,exact(IISE)DFP*/
#define DXC_IEEE_OF_INEX_TRUNC  0x28    /* IEEE o/flow,inexact,trunc */
#define DXC_IEEE_OF_INEX_IISE   0x2B    /* IEEE o/flow,inex(IISE) DFP*/
#define DXC_IEEE_OF_INEX_INCR   0x2C    /* IEEE o/flow,inexact,incr  */
#define DXC_IEEE_DIV_ZERO       0x40    /* IEEE division by zero     */
#define DXC_IEEE_DIV_ZERO_IISE  0x43    /* IEEE div by zero(IISE) DFP*/
#define DXC_IEEE_INVALID_OP     0x80    /* IEEE invalid operation    */
#define DXC_IEEE_INV_OP_IISE    0x83    /* IEEE invalid op (IISE) DFP*/
#define DXC_COMPARE_AND_TRAP    0xFF    /* Compare-and-trap exception*/
/* Note: IISE = IEEE-interruption-simulation event */

/* Decimal rounding modes */
#define DRM_RNE                 0       /* Round to nearest tie even */
#define DRM_RTZ                 1       /* Round toward zero         */
#define DRM_RTPI                2       /* Round toward +infinity    */
#define DRM_RTMI                3       /* Round toward -infinity    */
#define DRM_RNAZ                4       /* Round nearest tie away 0  */
#define DRM_RNTZ                5       /* Round nearest tie toward 0*/
#define DRM_RAFZ                6       /* Round away from zero      */
#define DRM_RFSP                7       /* Prepare shorter precision */

/* Binary rounding modes */
#define BRM_RNE                 0       /* Round to nearest tie even */
#define BRM_RTZ                 1       /* Round toward zero         */
#define BRM_RTPI                2       /* Round toward +infinity    */
#define BRM_RTMI                3       /* Round toward -infinity    */

/* Mask bits for conditional SSKE facility */
#define SSKE_MASK_MR            0x04    /* Reference bit update mask */
#define SSKE_MASK_MC            0x02    /* Change bit update mask    */

#endif // _ESA390_H
