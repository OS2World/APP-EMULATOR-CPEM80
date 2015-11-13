// 	$Id: z80.cc 1.3 1994/04/12 22:08:35 rg Exp $	
//      Copyright (c) 1994 by R. Griech
//
//  Z80 Simulation
//  --------------
//  Here the Z80 emulation is contained.  The Z80 is simulated thru big
//  switch statements.  For GNUC && 386 there exists an optimization with
//  asm-statements.  No other secrets exists...
//
//  possible problem areas
//  ----------------------
//  - a 486/33MHz emulates a ~4-6MHz Z80
//  - this is not a complete emulation, especially I/O has no definition
//
//  doubtful constructs
//  -------------------
//  - sometimes the switch-statements have definitions of 0 and 255 without
//    corresponding opcodes.  This is required for the peephole to detect
//    a complete range for a switch for unsigned char
//



#include <stdio.h>
#include "glob.h"



#define ALIGN ".align 2,0x90; "



//
//
//  Organisation:
//  Carry,B,A, d.h. fÅr SBC A,D  wird mit SubTab[carry][regD][regA] zugegriffen
//  Dies wird zwecks dem Cache gemacht (denke an INC, DEC)
//  Platz zwischen AddTab und SubTab lassen, da die sich im Cache gegenseitig stîren
//
unsigned char FTab[256];
struct _bytewise { unsigned char F,A; };    // same order as in struct _byteregs
typedef union {
   unsigned short AF;
   struct _bytewise b;
} MatAF;
#if !(__i386__  &&  __GNUC__)
   MatAF AddTab[2][256][256];
#endif
MatAF RlcTab[256][2],  RlTab[256][2],
      RrcTab[256][2],  RrTab[256][2],
      SlaTab[256][2],  SraTab[256][2], SrlTab[256][2];
#if !(__i386__  &&  __GNUC__)
   MatAF SubTab[2][256][256];
#endif



//
//
//  opcode counter for profiling
//
#ifdef DEBUG
int CntOp[256], CntOpCB[256], CntOpED[256],
    CntOpIX[256], CntOpIXCB[256], CntOpIY[256], CntOpIYCB[256];
#endif



#ifdef __i386__

#define SIGN8(X)  ((int)( (signed char)(X) ))
#define SIGN16(X) ((int)( (short)(X) ))

#else

static inline int SIGN8( int n )
{
   return( (n <= 127) ? n : n-0x100 );
}   // SIGN8

static inline int SIGN16( int n )
{
   return( (n <= 32767) ? n : n-0x10000 );
}   // SIGN16

#endif



//////////////////////////////////////////////////////////////////////



#if __i386__  &&  __GNUC__
#define ADD( s,l )									\
{											\
   asm("movb %2,%%al ; addb %%al,%0 ; lahf ; movb $4,%%al ; jo 0f ; xorb %%al,%%al ;"	\
       ALIGN "0: ; andb $209,%%ah ; orb %%ah,%%al ; movb %%al,%1"			\
       : "=m" (r.b.A), "=m" (r.b.F)							\
       : "m"  (s)									\
       : "%eax", "%cc" );								\
   rPC += l;										\
}											\
break
#else
#define ADD( s,l )				\
{						\
   r.w.AF = AddTab[0][s][r.b.A].AF;		\
   rPC += l;					\
}						\
break
#endif


#if __i386__  &&  __GNUC__
#define ADC( s,l )									\
{											\
   asm("movb %1,%%ah ; sahf ;"								\
       "movb %2,%%al ; adcb %%al,%0 ; lahf ; movb $4,%%al ; jo 0f ; xorb %%al,%%al ;"	\
       ALIGN "0: ; andb $209,%%ah ; orb %%ah,%%al ; movb %%al,%1"			\
       : "=m" (r.b.A), "=m" (r.b.F)							\
       : "m"  (s)									\
       : "%eax", "%cc" );								\
   rPC += l;										\
}											\
break
#else
#define ADC( s,l )				\
{						\
   unsigned char c = (r.b.F & F_C) ? 1 : 0;	\
   r.w.AF = AddTab[c][s][r.b.A].AF;		\
   rPC += l;					\
}						\
break
#endif


#if __i386__  &&  __GNUC__
#define ADC16( s )									\
{											\
   asm("movb %1,%%ah ; sahf ;"								\
       " movw %2,%%ax ; adcw %%ax,%0 ; lahf ; movb $4,%%al ; jo 0f ; xorb %%al,%%al ;"	\
       ALIGN "0: ; andb $193,%%ah ; orb %%ah,%%al ; movb %%al,%1"			\
       : "=m" (r.w.HL), "=m" (r.b.F)							\
       : "m"  (s)									\
       : "%eax", "%cc" );								\
   rPC += 2;										\
}											\
break
#else
#define ADC16( s )					\
{							\
   unsigned char c = (r.b.F & F_C) ? 1 : 0;		\
   unsigned ures = r.w.HL + s + c;			\
   int sres = SIGN16(r.w.HL) + SIGN16(s) + c;		\
   r.b.F = 0;						\
   if (sres > 32767  ||  sres < -32768)			\
       r.b.F = F_PE;					\
   r.w.HL = ures & 0xffff;				\
   r.b.F = r.b.F | ((r.w.HL >= 0x8000) ? F_S : 0)  |	\
                   ((ures   >= 0x10000) ? F_C : 0) |	\
                   ((r.w.HL == 0) ? F_Z : 0);		\
   rPC += 2;						\
}							\
break
#endif


#if __i386__  &&  __GNUC__
#define ADD16( d,s,l )									\
{											\
   asm("movb %0,%%cl ; andb $196,%%cl ; movw %2,%%ax ; addw %%ax,%1 ; movb $1,%%al ;"	\
       "jc 0f ; xorb %%al,%%al ;" ALIGN "0: ; orb %%cl,%%al ; movb %%al,%0"		\
       : "=m" (r.b.F), "=m" (d)								\
       : "m" (s)									\
       : "%eax", "%cc", "%ecx" );							\
   rPC += l;										\
}											\
break
#else
#define ADD16( d,s,l )							\
{									\
   int ures = d + s;							\
   d = ures & 0xffff;							\
   r.b.F = (r.b.F & (F_S|F_Z|F_PE)) | ((ures >= 0x10000) ? F_C : 0);	\
   rPC += l;								\
}									\
break
#endif


#if __i386__  &&  __GNUC__
#define AND( s,l )						\
{								\
   asm("movb %2,%%al ; andb %%al,%0 ; lahf ; andb $196,%%ah ;"	\
       "orb $16,%%ah ; movb %%ah,%1"				\
       : "=m" (r.b.A), "=m" (r.b.F)				\
       : "m" (s)						\
       : "%eax", "%cc");					\
   rPC += l;							\
}								\
break
#else
#define AND( s,l )				\
{						\
   r.b.A &= s;					\
   r.b.F = FTab[r.b.A] | F_H;			\
   rPC += l;					\
}						\
break
#endif


#if __i386__  &&  __GNUC__
#define BIT( bi,d,l )							 	\
{									 	\
   asm("movb %0,%%al ; andb $1,%%al ; orb $16,%%al ;"			 	\
       "testb %1,%2 ; jnz 0f ; orb $64,%%al ;" ALIGN "0: ; movb %%al,%0"	\
       : "=m" (r.b.F)							 	\
       : "n" (1 << bi), "m" (d)						 	\
       : "%eax", "%cc" );						 	\
   rPC += l;								 	\
}									 	\
break
#else
#define BIT( bi,d,l )						\
{								\
   r.b.F = (r.b.F & F_C) | F_H | ((d & (1 << bi)) ? 0 : F_Z);	\
   rPC += l;							\
}								\
break
#endif


#define CALL( a,l )				\
{						\
   r.w.SP -= 2;					\
   W( Mem+r.w.SP ) = rPC+l-Mem;			\
   rPC = a+Mem;					\
}						\
break

#define CALLf( flg )				\
{						\
   rPC += 3;					\
   if (!(r.b.F & flg)) {			\
      r.w.SP -= 2;				\
      W(Mem+r.w.SP) = rPC-Mem;			\
      rPC = W(rPC-2)+Mem;			\
   }						\
}						\
break

#define CALLt( flg )				\
{						\
   rPC += 3;					\
   if (r.b.F & flg) {				\
      r.w.SP -= 2;				\
      W(Mem+r.w.SP) = rPC-Mem;			\
      rPC = W(rPC-2)+Mem;			\
   }						\
}						\
break


#if __i386__  &&  __GNUC__
#define CP( s,l )									\
{											\
   asm("movb %2,%%al ; cmpb %%al,%0 ; lahf ; movb $4,%%al ; jo 0f ; xorb %%al,%%al ;"	\
       ALIGN "0: ; andb $211,%%ah ; orb %%ah,%%al ; movb %%al,%1"			\
       : "=m" (r.b.A), "=m" (r.b.F)							\
       : "m"  (s)									\
       : "%eax", "%cc" );								\
   rPC += l;										\
}
#else
#define CP( s,l )				\
{						\
   r.b.F = SubTab[0][s][r.b.A].b.F;		\
   rPC += l;					\
}
#endif


#if __i386__  &&  __GNUC__
#define DEC( d,l )								\
{										\
   asm("movb %1,%%ah ; sahf ;"							\
       " decb %0 ; lahf ; movb $4,%%al ; jo 0f ; xorb %%al,%%al ;"		\
       ALIGN "0: ; andb $211,%%ah ; orb %%ah,%%al ; movb %%al,%1"		\
       : "=m" (d), "=m" (r.b.F)							\
       :									\
       : "%eax", "%cc" );							\
   rPC += l;									\
}										\
break
#else
#define DEC( d,l )						\
{								\
   r.b.F = (SubTab[0][1][d].b.F & ~F_C)  |  (r.b.F & F_C);	\
   d--;								\
   rPC += l;							\
}								\
break
#endif


#define DEC16( d,l )				\
{						\
   d--;						\
   rPC += l;					\
}						\
break

#define DJNZ()					\
{						\
   rPC += 2;					\
   r.b.B--;					\
   if (r.b.B)					\
      rPC += SB(rPC-1);				\
}						\
break


//
//
//  first operand should contain the more complicated address expression
//
#if __i386__  &&  __GNUC__
#define _EX16(s1,s2)					\
{							\
   asm("movw %1,%%ax ; xchgw %%ax,%0 ; movw %%ax,%1"	\
       : "=m" (s1), "=m" (s2)				\
       :						\
       : "%eax" );					\
}
#else
#define _EX16(s1,s2)				\
{						\
   unsigned short t = s1;			\
   s1 = s2;					\
   s2 = t;					\
}
#endif


#if __i386__  &&  __GNUC__
#define INC( d,l )								\
{										\
   asm("movb %1,%%ah ; sahf ;"							\
       "incb %0 ; lahf ; movb $4,%%al ; jo 0f ; xorb %%al,%%al ;"		\
       ALIGN "0: ; andb $209,%%ah ; orb %%ah,%%al ; movb %%al,%1"		\
       : "=m" (d), "=m" (r.b.F)							\
       :									\
       : "%eax", "%cc" );							\
   rPC += l;									\
}										\
break
#else
#define INC( d,l )						\
{								\
   r.b.F = (AddTab[0][1][d].b.F & ~F_C)  |  (r.b.F & F_C);	\
   d++;								\
   rPC += l;							\
}								\
break
#endif


#define INC16( d,l )				\
{						\
   d++;						\
   rPC += l;					\
}						\
break

#define JP( a )					\
{						\
   rPC = a+Mem;					\
}						\
break

#define JPf( flg )				\
{						\
   rPC += 3;					\
   if (!(r.b.F & flg))				\
      rPC = W(rPC-2)+Mem;			\
}						\
break

#define JPt( flg )				\
{						\
   rPC += 3;					\
   if (r.b.F & flg)				\
      rPC = W(rPC-2)+Mem;			\
}						\
break

#define JR()					\
{						\
   rPC += 2+SB(rPC+1);				\
}						\
break

#define JRf( flg )				\
{						\
   rPC += 2;					\
   if (!(r.b.F & flg))				\
      rPC += SB(rPC-1);				\
}						\
break

#define JRt( flg )				\
{						\
   rPC += 2;					\
   if (r.b.F & flg)				\
      rPC += SB(rPC-1);				\
}						\
break

#define LD16( d,s,l )				\
{						\
   d = s;					\
   rPC += l;					\
}						\
break

#define LD8( d,s,l )				\
{						\
   d = s;					\
   rPC += l;					\
}						\
break


#if __i386__  &&  __GNUC__
#define NEG8( d )								\
{										\
   asm("negb %0 ; lahf ; movb $4,%%al ; jo 0f ; xorb %%al,%%al ;"		\
       ALIGN "0: ; andb $211,%%ah ; orb %%ah,%%al ; movb %%al,%1"		\
       : "=m" (d), "=m" (r.b.F)							\
       :									\
       : "%eax", "%cc");							\
   rPC += 2;									\
}
#else
#define NEG8( d )				\
{						\
   r.w.AF = SubTab[0][r.b.A][0].AF;		\
   rPC += 2;					\
}
#endif


#if __i386__  &&  __GNUC__
#define OR( s,l )						\
{								\
   asm("movb %2,%%al ; orb %%al,%0 ; lahf ; andb $196,%%ah ;"	\
       "movb %%ah,%1"						\
       : "=m" (r.b.A), "=m" (r.b.F)				\
       : "m" (s)						\
       : "%eax", "%cc");					\
   rPC += l;							\
}								\
break
#else
#define OR( s,l )				\
{						\
   r.b.A |= s;					\
   r.b.F = FTab[r.b.A];				\
   rPC += l;					\
}						\
break
#endif


#define POP( d,l )				\
{						\
   d = W( Mem+r.w.SP );				\
   r.w.SP += 2;					\
   rPC += l;					\
}						\
break

#define PUSH( s,l )				\
{						\
   r.w.SP -= 2;					\
   W( Mem+r.w.SP ) = s;				\
   rPC += l;					\
}						\
break


#if __i386__  &&  __GNUC__
#define RES( b,d,l )				\
{						\
   asm("andb %1,%0"				\
       : "=m" (d)				\
       : "n" (~(1 << b))			\
       : "%cc" );				\
   rPC += l;					\
}						\
break
#else
#define RES( b,d,l )				\
{						\
   d &= ~(1 << b);				\
   rPC += l;					\
}						\
break
#endif


#define RET()					\
{						\
   rPC = W( Mem+r.w.SP )+Mem;			\
   r.w.SP += 2;					\
}						\
break

#define RETf( flg )				\
{						\
   if (!(r.b.F & flg)) {			\
      rPC = W( Mem+r.w.SP )+Mem;		\
      r.w.SP += 2;				\
   }						\
   else						\
      rPC += 1;					\
}						\
break

#define RETt( flg )				\
{						\
   if (r.b.F & flg) {				\
      rPC = W( Mem+r.w.SP )+Mem;		\
      r.w.SP += 2;				\
   }						\
   else						\
       rPC += 1;				\
}						\
break

#define _ROT( d,TAB,l )				\
{						\
   unsigned char c = (r.b.F & F_C) ? 1 : 0;	\
   r.b.F = TAB[d][c].b.F;			\
   d = TAB[d][c].b.A;				\
   rPC += l;					\
}						\
break

#define _ROTA( TAB,l )				\
{						\
   unsigned char c = (r.b.F & F_C) ? 1 : 0;	\
   r.w.AF = TAB[r.b.A][c].AF;			\
   rPC += l;					\
}						\
break

#define _ROTAA( TAB )							\
{									\
   unsigned char c = (r.b.F & F_C) ? 1 : 0;				\
   r.b.F = (TAB[r.b.A][c].b.F & F_C)  |  (r.b.F & (F_S|F_Z|F_PE));	\
   r.b.A = TAB[r.b.A][c].b.A;						\
   rPC += 1;								\
}									\
break


#if __i386__  &&  __GNUC__
#define SBC( s,l )									\
{											\
   asm("movb %1,%%ah ; sahf ;"								\
       "movb %2,%%al ; sbbb %%al,%0 ; lahf ; movb $4,%%al ; jo 0f ; xorb %%al,%%al ;"	\
       ALIGN "0: ; andb $211,%%ah ; orb %%ah,%%al ; movb %%al,%1"			\
       : "=m" (r.b.A), "=m" (r.b.F)							\
       : "m"  (s)									\
       : "%eax", "%cc" );								\
   rPC += l;										\
}											\
break
#else
#define SBC( s,l )				\
{						\
   unsigned char c = (r.b.F & F_C) ? 1 : 0;	\
   r.w.AF = SubTab[c][s][r.b.A].AF;		\
   rPC += l;					\
}						\
break
#endif


#if __i386__  &&  __GNUC__
#define SBC16( s )									\
{											\
   asm("movb %1,%%ah ; sahf ;"								\
       "movw %2,%%ax ; sbbw %%ax,%0 ; lahf ; movb $4,%%al ; jo 0f ; xorb %%al,%%al ;"	\
       ALIGN "0: ; andb $195,%%ah ; orb %%ah,%%al ; movb %%al,%1"			\
       : "=m" (r.w.HL), "=m" (r.b.F)							\
       : "m"  (s)									\
       : "%eax", "%cc" );								\
   rPC += 2;										\
}											\
break
#else
#define SBC16( s )					\
{							\
   unsigned char c = (r.b.F & F_C) ? 1 : 0;		\
   unsigned ures = r.w.HL - s - c;			\
   int sres = SIGN16(r.w.HL) - SIGN16(s) - c;		\
   r.b.F = 0;						\
   if (sres > 32767  ||  sres < -32768)			\
       r.b.F = F_PE;					\
   r.w.HL = ures & 0xffff;				\
   r.b.F = r.b.F | ((r.w.HL >= 0x8000) ? F_S : 0)  |	\
                   ((ures   >= 0x10000) ? F_C : 0) |	\
                   ((r.w.HL == 0) ? F_Z : 0) | F_N;	\
   rPC += 2;						\
}							\
break
#endif


#if __i386__  &&  __GNUC__
#define SUB( s,l )									\
{											\
   asm("movb %2,%%al ; subb %%al,%0 ; lahf ; movb $4,%%al ; jo 0f ; xorb %%al,%%al ;"	\
       ALIGN "0: ; andb $211,%%ah ; orb %%ah,%%al ; movb %%al,%1"			\
       : "=m" (r.b.A), "=m" (r.b.F)							\
       : "m"  (s)									\
       : "%eax", "%cc" );								\
   rPC += l;										\
}											\
break
#else
#define SUB( s,l )				\
{						\
   r.w.AF = SubTab[0][s][r.b.A].AF;		\
   rPC += l;					\
}						\
break
#endif


#if __i386__  &&  __GNUC__
#define SET( b,d,l )				\
{						\
   asm("orb %1,%0"				\
       : "=m" (d)				\
       : "n" (1 << b)				\
       : "%cc" );				\
   rPC += l;					\
}						\
break
#else
#define SET( b,d,l )				\
{						\
   d |= (1 << b);				\
   rPC += l;					\
}						\
break
#endif


#if __i386__  &&  __GNUC__
#define XOR( s,l )						\
{								\
   asm("movb %2,%%al ; xorb %%al,%0 ; lahf ; andb $196,%%ah ;"	\
       "movb %%ah,%1"						\
       : "=m" (r.b.A), "=m" (r.b.F)				\
       : "m" (s)						\
       : "%eax", "%cc");					\
   rPC += l;							\
}								\
break
#else
#define XOR( s,l )				\
{						\
   r.b.A ^= s;					\
   r.b.F = FTab[r.b.A];				\
   rPC += l;					\
}						\
break
#endif



#ifdef DEBUG
#  define CNTOP( TAB,CODE )  (++TAB [ CODE ])
#else
#  define CNTOP( TAB,CODE )
#endif


#define INDEX_OPS(JJ)									\
{											\
   CNTOP( CntOp ## JJ, B(rPC+1) );							\
   switch( B(rPC+1) ) {									\
      case 0x00:  r.b.A = 0xff;  goto DEF_ ## JJ;					\
      case 0x09:  ADD16( r.w.JJ,r.w.BC,2 );						\
      case 0x19:  ADD16( r.w.JJ,r.w.DE,2 );						\
											\
      case 0x21:  LD16( r.w.JJ,W(rPC+2), 4 );						\
      case 0x22:  LD16( W(Mem+W(rPC+2)),r.w.JJ, 4 );					\
      case 0x23:  INC16( r.w.JJ, 2 );							\
      case 0x29:  ADD16( r.w.JJ,r.w.JJ,2 );						\
      case 0x2a:  LD16( r.w.JJ,W(Mem+W(rPC+2)), 4 );					\
      case 0x2b:  DEC16( r.w.JJ, 2 );							\
											\
      case 0x34:  INC( B(Mem+r.w.JJ+SB(rPC+2)), 3 );					\
      case 0x35:  DEC( B(Mem+r.w.JJ+SB(rPC+2)), 3 );					\
      case 0x36:  LD8( B(Mem+r.w.JJ+SB(rPC+2)),B(rPC+3), 4 );				\
      case 0x39:  ADD16( r.w.JJ,r.w.SP,2 );						\
											\
      case 0x46:  LD8( r.b.B,B(Mem+r.w.JJ+SB(rPC+2)), 3 );				\
      case 0x4e:  LD8( r.b.C,B(Mem+r.w.JJ+SB(rPC+2)), 3 );				\
											\
      case 0x56:  LD8( r.b.D,B(Mem+r.w.JJ+SB(rPC+2)), 3 );				\
      case 0x5e:  LD8( r.b.E,B(Mem+r.w.JJ+SB(rPC+2)), 3 );				\
											\
      case 0x66:  LD8( r.b.H,B(Mem+r.w.JJ+SB(rPC+2)), 3 );				\
      case 0x6e:  LD8( r.b.L,B(Mem+r.w.JJ+SB(rPC+2)), 3 );				\
											\
      case 0x70:  LD8( B(Mem+r.w.JJ+SB(rPC+2)),r.b.B, 3 );				\
      case 0x71:  LD8( B(Mem+r.w.JJ+SB(rPC+2)),r.b.C, 3 );				\
      case 0x72:  LD8( B(Mem+r.w.JJ+SB(rPC+2)),r.b.D, 3 );				\
      case 0x73:  LD8( B(Mem+r.w.JJ+SB(rPC+2)),r.b.E, 3 );				\
      case 0x74:  LD8( B(Mem+r.w.JJ+SB(rPC+2)),r.b.H, 3 );				\
      case 0x75:  LD8( B(Mem+r.w.JJ+SB(rPC+2)),r.b.L, 3 );				\
      case 0x77:  LD8( B(Mem+r.w.JJ+SB(rPC+2)),r.b.A, 3 );				\
      case 0x7e:  LD8( r.b.A,B(Mem+r.w.JJ+SB(rPC+2)), 3 );				\
											\
      case 0x86:  ADD( B(Mem+r.w.JJ+SB(rPC+2)), 3 );					\
      case 0x8e:  ADC( B(Mem+r.w.JJ+SB(rPC+2)), 3 );					\
      case 0x96:  SUB( B(Mem+r.w.JJ+SB(rPC+2)), 3 );					\
      case 0x9e:  SBC( B(Mem+r.w.JJ+SB(rPC+2)), 3 );					\
      case 0xa6:  AND( B(Mem+r.w.JJ+SB(rPC+2)), 3 );					\
      case 0xae:  XOR( B(Mem+r.w.JJ+SB(rPC+2)), 3 );					\
      case 0xb6:  OR( B(Mem+r.w.JJ+SB(rPC+2)), 3 );					\
      case 0xbe:  CP( B(Mem+r.w.JJ+SB(rPC+2)), 3 );    break;				\
											\
      case 0xcb:									\
         {										\
            CNTOP( CntOp ## JJ ## CB, B(rPC+3) );					\
            switch( B(rPC+3) ) {							\
               case 0x00:  r.b.A = 0xff;  goto DEF_ ## JJ ## _cb;			\
               case 0x06:  _ROT( B(Mem+r.w.JJ+SB(rPC+2)),RlcTab,4 );			\
               case 0x0e:  _ROT( B(Mem+r.w.JJ+SB(rPC+2)),RrcTab,4 );			\
               case 0x16:  _ROT( B(Mem+r.w.JJ+SB(rPC+2)),RlTab,4 );			\
               case 0x1e:  _ROT( B(Mem+r.w.JJ+SB(rPC+2)),RrTab,4 );			\
               case 0x26:  _ROT( B(Mem+r.w.JJ+SB(rPC+2)),SlaTab,4 );			\
               case 0x2e:  _ROT( B(Mem+r.w.JJ+SB(rPC+2)),SraTab,4 );			\
               case 0x3e:  _ROT( B(Mem+r.w.JJ+SB(rPC+2)),SrlTab,4 );			\
               case 0x46:  BIT( 0,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0x4e:  BIT( 1,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0x56:  BIT( 2,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0x5e:  BIT( 3,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0x66:  BIT( 4,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0x6e:  BIT( 5,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0x76:  BIT( 6,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0x7e:  BIT( 7,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0x86:  RES( 0,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0x8e:  RES( 1,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0x96:  RES( 2,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0x9e:  RES( 3,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0xa6:  RES( 4,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0xae:  RES( 5,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0xb6:  RES( 6,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0xbe:  RES( 7,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0xc6:  SET( 0,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0xce:  SET( 1,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0xd6:  SET( 2,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0xde:  SET( 3,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0xe6:  SET( 4,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0xee:  SET( 5,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0xf6:  SET( 6,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0xfe:  SET( 7,B(Mem+r.w.JJ+SB(rPC+2)), 4 );				\
               case 0xff:  r.b.A = 0xff;  goto DEF_ ## JJ ## _cb;			\
               default:									\
               DEF_ ## JJ ## _cb:							\
                  r.w.PC = rPC-Mem;							\
                  RegisterDump( r,"ill opcode detected (CB " #JJ " extension)",0 );	\
                  CoreDump();								\
                  ExitCode = 3;								\
                  goto finished;							\
                  break;								\
            }										\
         }										\
         break;										\
      case 0xe1:  POP( r.w.JJ, 2 );							\
      case 0xe3:                                    /* EX (SP),JJ */			\
         _EX16( W(Mem+r.w.SP),r.w.JJ );  rPC += 2;  break;				\
      case 0xe5:  PUSH( r.w.JJ, 2 );							\
      case 0xe9:  JP( r.w.JJ );								\
      case 0xf9:  LD16( r.w.SP,r.w.JJ,2 );						\
      case 0xff:  r.b.A = 0xff;  goto DEF_ ## JJ;					\
      default:										\
      DEF_ ## JJ:									\
         r.w.PC = rPC-Mem;								\
         RegisterDump( r,"ill opcode detected (" #JJ " extension)",0 );			\
         CoreDump();									\
         ExitCode = 3;									\
         goto finished;									\
         break;										\
   }											\
}											\
break



void Z80Emu( REGS *preg )
{
   REGS r;
#if __i386__  &&  __GNUC__
   register unsigned char *rPC asm("%ebx");
#else
   register unsigned char *rPC;
#endif

   if (opt_Verbose)
      fprintf( stderr,"Z80Emu...\r\n" );

   r   = *preg;
   rPC = r.w.PC+Mem;

   for (;;) {

#ifdef DEBUG
      ++CntOp[ B(rPC) ];
      if (opt_SingleStep) {
         r.w.PC = rPC-Mem;
         RegisterDump( r,"single step",0 );
      }
#endif

      switch ( B(rPC) )
	 {
	 case 0x00:                                                     // NOP
	    rPC += 1;
	    break;
         case 0x01:  LD16( r.w.BC,W(rPC+1), 3 );
         case 0x02:  LD8( B(Mem+r.w.BC),r.b.A, 1 );
         case 0x03:  INC16( r.w.BC, 1 );
         case 0x04:  INC( r.b.B, 1 );
         case 0x05:  DEC( r.b.B, 1 );
         case 0x06:  LD8( r.b.B,B(rPC+1), 2 );
         case 0x07:  _ROTAA( RlcTab );
         case 0x08:                                                     // EX AF,AF'
            _EX16( r.w.AF,r.w.AF2 );  rPC += 1;  break;
         case 0x09:  ADD16( r.w.HL,r.w.BC, 1 );
         case 0x0a:  LD8( r.b.A,B(Mem+r.w.BC), 1 );
         case 0x0b:  DEC16( r.w.BC, 1 );
         case 0x0c:  INC( r.b.C, 1 );
         case 0x0d:  DEC( r.b.C, 1 );
         case 0x0e:  LD8( r.b.C,B(rPC+1), 2 );
         case 0x0f:  _ROTAA( RrcTab );

         case 0x10:  DJNZ();
         case 0x11:  LD16( r.w.DE,W(rPC+1), 3 );
         case 0x12:  LD8( B(Mem+r.w.DE),r.b.A, 1 );
         case 0x13:  INC16( r.w.DE, 1 );
         case 0x14:  INC( r.b.D, 1 );
         case 0x15:  DEC( r.b.D, 1 );
         case 0x16:  LD8( r.b.D,B(rPC+1), 2 );
         case 0x17:  _ROTAA( RlTab );
         case 0x18:  JR();
         case 0x19:  ADD16( r.w.HL,r.w.DE, 1 );
         case 0x1a:  LD8( r.b.A,B(Mem+r.w.DE), 1 );
         case 0x1b:  DEC16( r.w.DE, 1 );
         case 0x1c:  INC( r.b.E, 1 );
         case 0x1d:  DEC( r.b.E, 1 );
         case 0x1e:  LD8( r.b.E,B(rPC+1), 2 );
         case 0x1f:  _ROTAA( RrTab );

         case 0x20:  JRf( F_Z );
         case 0x21:  LD16( r.w.HL,W(rPC+1), 3 );
         case 0x22:  LD16( W(Mem+W(rPC+1)),r.w.HL, 3 );
         case 0x23:  INC16( r.w.HL, 1 );
         case 0x24:  INC( r.b.H, 1 );
         case 0x25:  DEC( r.b.H, 1 );
         case 0x26:  LD8( r.b.H,B(rPC+1), 2 );
         case 0x27:                                                     // DAA
            if (r.b.F & F_N) {
               unsigned short c = r.b.A;
               unsigned char nf = 0;
               if (opt_Quest) {
                  r.w.PC = rPC-Mem;
                  RegisterDump( r,"DAA for F_N (not tested, before)",0 );
               }
               if (r.b.F & F_H  ||  (c & 0x0f) >= 0x0a) {
                  c -= 0x06;
                  if (c >= 0x100)
                     r.b.F |= F_C;
                  nf = F_H;
               }
               if (r.b.F & F_C  ||  c >= 0xa0) {
                  c -= 0x60;
                  nf |= F_C;
               }
               r.b.A = c;
               r.b.F = FTab[r.b.A] | nf;
               if (opt_Quest) {
                  r.w.PC = rPC-Mem;
                  RegisterDump( r,"DAA for F_N (not tested, after)",0 );
               }
            } else {
               unsigned short c = r.b.A;
               unsigned char nf = 0;
               if (r.b.F & F_H  ||  (c & 0x0f) >= 0x0a) {
                  c += 0x06;
                  if (c >= 0x100)
                     r.b.F |= F_C;
                  nf |= F_H;           // siehe 386er Buch fÅr Beschreibung von DAA
               }
               if (r.b.F & F_C  ||  c >= 0xa0) {
                  c += 0x60;
                  nf |= F_C;
               }
               r.b.A = c;
               r.b.F = FTab[r.b.A] | nf;
            }
            rPC += 1;
            break;
         case 0x28:  JRt( F_Z );
         case 0x29:  ADD16( r.w.HL,r.w.HL, 1 );
         case 0x2a:  LD16( r.w.HL,W(Mem+W(rPC+1)), 3 );
         case 0x2b:  DEC16( r.w.HL, 1 );
         case 0x2c:  INC( r.b.L, 1 );
         case 0x2d:  DEC( r.b.L, 1 );
         case 0x2e:  LD8( r.b.L,B(rPC+1), 2 );
         case 0x2f:                                                     // CPL
            r.b.A = ~r.b.A;
            r.b.F |= (F_H|F_N);
            rPC += 1;
            break;

         case 0x30:  JRf( F_C );
         case 0x31:  LD16( r.w.SP,W(rPC+1), 3 );
         case 0x32:  LD8( B(Mem+W(rPC+1)),r.b.A, 3 );
         case 0x33:  INC16( r.w.SP, 1 );
         case 0x34:  INC( B(Mem+r.w.HL), 1 );
         case 0x35:  DEC( B(Mem+r.w.HL), 1 );
         case 0x36:  LD8( B(Mem+r.w.HL),B(rPC+1), 2 );
         case 0x37:                                                     // SCF
            r.b.F = (r.b.F & (F_S|F_Z|F_PE)) | F_C;
            rPC += 1;
            break;
         case 0x38:  JRt( F_C );
         case 0x39:  ADD16( r.w.HL,r.w.SP, 1 );
         case 0x3a:  LD8( r.b.A,B(Mem+W(rPC+1)), 3 );
         case 0x3b:  DEC16( r.w.SP, 1 );
         case 0x3c:  INC( r.b.A, 1 );
         case 0x3d:  DEC( r.b.A, 1 );
         case 0x3e:  LD8( r.b.A,B(rPC+1), 2 );
         case 0x3f:                                                     // CCF
            r.b.F = (r.b.F & (F_S|F_Z|F_PE|F_C)) ^ F_C;
            rPC += 1;
            break;

         case 0x40:  LD8( r.b.B,r.b.B, 1 );
         case 0x41:  LD8( r.b.B,r.b.C, 1 );
         case 0x42:  LD8( r.b.B,r.b.D, 1 );
         case 0x43:  LD8( r.b.B,r.b.E, 1 );
         case 0x44:  LD8( r.b.B,r.b.H, 1 );
         case 0x45:  LD8( r.b.B,r.b.L, 1 );
         case 0x46:  LD8( r.b.B,B(Mem+r.w.HL), 1 );
         case 0x47:  LD8( r.b.B,r.b.A, 1 );
         case 0x48:  LD8( r.b.C,r.b.B, 1 );
         case 0x49:  LD8( r.b.C,r.b.C, 1 );
         case 0x4a:  LD8( r.b.C,r.b.D, 1 );
         case 0x4b:  LD8( r.b.C,r.b.E, 1 );
         case 0x4c:  LD8( r.b.C,r.b.H, 1 );
         case 0x4d:  LD8( r.b.C,r.b.L, 1 );
         case 0x4e:  LD8( r.b.C,B(Mem+r.w.HL), 1 );
         case 0x4f:  LD8( r.b.C,r.b.A, 1 );

         case 0x50:  LD8( r.b.D,r.b.B, 1 );
         case 0x51:  LD8( r.b.D,r.b.C, 1 );
         case 0x52:  LD8( r.b.D,r.b.D, 1 );
         case 0x53:  LD8( r.b.D,r.b.E, 1 );
         case 0x54:  LD8( r.b.D,r.b.H, 1 );
         case 0x55:  LD8( r.b.D,r.b.L, 1 );
         case 0x56:  LD8( r.b.D,B(Mem+r.w.HL), 1 );
         case 0x57:  LD8( r.b.D,r.b.A, 1 );
         case 0x58:  LD8( r.b.E,r.b.B, 1 );
         case 0x59:  LD8( r.b.E,r.b.C, 1 );
         case 0x5a:  LD8( r.b.E,r.b.D, 1 );
         case 0x5b:  LD8( r.b.E,r.b.E, 1 );
         case 0x5c:  LD8( r.b.E,r.b.H, 1 );
         case 0x5d:  LD8( r.b.E,r.b.L, 1 );
         case 0x5e:  LD8( r.b.E,B(Mem+r.w.HL), 1 );
         case 0x5f:  LD8( r.b.E,r.b.A, 1 );

         case 0x60:  LD8( r.b.H,r.b.B, 1 );
         case 0x61:  LD8( r.b.H,r.b.C, 1 );
         case 0x62:  LD8( r.b.H,r.b.D, 1 );
         case 0x63:  LD8( r.b.H,r.b.E, 1 );
         case 0x64:  LD8( r.b.H,r.b.H, 1 );
         case 0x65:  LD8( r.b.H,r.b.L, 1 );
         case 0x66:  LD8( r.b.H,B(Mem+r.w.HL), 1 );
         case 0x67:  LD8( r.b.H,r.b.A, 1 );
         case 0x68:  LD8( r.b.L,r.b.B, 1 );
         case 0x69:  LD8( r.b.L,r.b.C, 1 );
         case 0x6a:  LD8( r.b.L,r.b.D, 1 );
         case 0x6b:  LD8( r.b.L,r.b.E, 1 );
         case 0x6c:  LD8( r.b.L,r.b.H, 1 );
         case 0x6d:  LD8( r.b.L,r.b.L, 1 );
         case 0x6e:  LD8( r.b.L,B(Mem+r.w.HL), 1 );
         case 0x6f:  LD8( r.b.L,r.b.A, 1 );

         case 0x70:  LD8( B(Mem+r.w.HL),r.b.B, 1 );
         case 0x71:  LD8( B(Mem+r.w.HL),r.b.C, 1 );
         case 0x72:  LD8( B(Mem+r.w.HL),r.b.D, 1 );
         case 0x73:  LD8( B(Mem+r.w.HL),r.b.E, 1 );
         case 0x74:  LD8( B(Mem+r.w.HL),r.b.H, 1 );
         case 0x75:  LD8( B(Mem+r.w.HL),r.b.L, 1 );
         case 0x76:                                                     // HALT
            if (opt_Quest) {
               r.w.PC = rPC-Mem;
               RegisterDump( r,"HALT opcode",0 );
            }
            rPC += 1;
            break;
         case 0x77:  LD8( B(Mem+r.w.HL),r.b.A, 1 );
         case 0x78:  LD8( r.b.A,r.b.B, 1 );
         case 0x79:  LD8( r.b.A,r.b.C, 1 );
         case 0x7a:  LD8( r.b.A,r.b.D, 1 );
         case 0x7b:  LD8( r.b.A,r.b.E, 1 );
         case 0x7c:  LD8( r.b.A,r.b.H, 1 );
         case 0x7d:  LD8( r.b.A,r.b.L, 1 );
         case 0x7e:  LD8( r.b.A,B(Mem+r.w.HL), 1 );
         case 0x7f:  LD8( r.b.A,r.b.A, 1 );

         case 0x80:  ADD( r.b.B, 1 );
         case 0x81:  ADD( r.b.C, 1 );
         case 0x82:  ADD( r.b.D, 1 );
         case 0x83:  ADD( r.b.E, 1 );
         case 0x84:  ADD( r.b.H, 1 );
         case 0x85:  ADD( r.b.L, 1 );
         case 0x86:  ADD( B(Mem+r.w.HL), 1 );
         case 0x87:  ADD( r.b.A, 1 );
         case 0x88:  ADC( r.b.B, 1 );
         case 0x89:  ADC( r.b.C, 1 );
         case 0x8a:  ADC( r.b.D, 1 );
         case 0x8b:  ADC( r.b.E, 1 );
         case 0x8c:  ADC( r.b.H, 1 );
         case 0x8d:  ADC( r.b.L, 1 );
         case 0x8e:  ADC( B(Mem+r.w.HL), 1 );
         case 0x8f:  ADC( r.b.A, 1 );

         case 0x90:  SUB( r.b.B, 1 );
         case 0x91:  SUB( r.b.C, 1 );
         case 0x92:  SUB( r.b.D, 1 );
         case 0x93:  SUB( r.b.E, 1 );
         case 0x94:  SUB( r.b.H, 1 );
         case 0x95:  SUB( r.b.L, 1 );
         case 0x96:  SUB( B(Mem+r.w.HL), 1 );
         case 0x97:  SUB( r.b.A, 1 );
         case 0x98:  SBC( r.b.B, 1 );
         case 0x99:  SBC( r.b.C, 1 );
         case 0x9a:  SBC( r.b.D, 1 );
         case 0x9b:  SBC( r.b.E, 1 );
         case 0x9c:  SBC( r.b.H, 1 );
         case 0x9d:  SBC( r.b.L, 1 );
         case 0x9e:  SBC( B(Mem+r.w.HL), 1 );
         case 0x9f:  SBC( r.b.A, 1 );

         case 0xa0:  AND( r.b.B,1 );
         case 0xa1:  AND( r.b.C,1 );
         case 0xa2:  AND( r.b.D,1 );
         case 0xa3:  AND( r.b.E,1 );
         case 0xa4:  AND( r.b.H,1 );
         case 0xa5:  AND( r.b.L,1 );
         case 0xa6:  AND( B(Mem+r.w.HL),1 );
         case 0xa7:  AND( r.b.A,1 );
         case 0xa8:  XOR( r.b.B,1 );
         case 0xa9:  XOR( r.b.C,1 );
         case 0xaa:  XOR( r.b.D,1 );
         case 0xab:  XOR( r.b.E,1 );
         case 0xac:  XOR( r.b.H,1 );
         case 0xad:  XOR( r.b.L,1 );
         case 0xae:  XOR( B(Mem+r.w.HL),1 );
         case 0xaf:  XOR( r.b.A,1 );

         case 0xb0:  OR( r.b.B,1 );
         case 0xb1:  OR( r.b.C,1 );
         case 0xb2:  OR( r.b.D,1 );
         case 0xb3:  OR( r.b.E,1 );
         case 0xb4:  OR( r.b.H,1 );
         case 0xb5:  OR( r.b.L,1 );
         case 0xb6:  OR( B(Mem+r.w.HL),1 );
         case 0xb7:  OR( r.b.A,1 );
         case 0xb8:  CP( r.b.B,1 );            break;
         case 0xb9:  CP( r.b.C,1 );            break;
         case 0xba:  CP( r.b.D,1 );            break;
         case 0xbb:  CP( r.b.E,1 );            break;
         case 0xbc:  CP( r.b.H,1 );            break;
         case 0xbd:  CP( r.b.L,1 );            break;
         case 0xbe:  CP( B(Mem+r.w.HL),1 );    break;
         case 0xbf:  CP( r.b.A,1 );            break;

         case 0xc0:  RETf( F_Z );
         case 0xc1:  POP( r.w.BC,1 );
         case 0xc2:  JPf( F_Z );
         case 0xc3:  JP( W(rPC+1) );
         case 0xc4:  CALLf( F_Z );
         case 0xc5:  PUSH( r.w.BC,1 );
         case 0xc6:  ADD( B(rPC+1), 2 );
         case 0xc7:  CALL( 0x00,1 );
         case 0xc8:  RETt( F_Z );
         case 0xc9:  RET();
         case 0xca:  JPt( F_Z );
         case 0xcb:
            {
#ifdef DEBUG
	       ++CntOpCB[ B(rPC+1) ];
#endif
               switch( B(rPC+1) )
		  {
		  case 0x00:  _ROT( r.b.B,RlcTab,2 );
		  case 0x01:  _ROT( r.b.C,RlcTab,2 );
		  case 0x02:  _ROT( r.b.D,RlcTab,2 );
		  case 0x03:  _ROT( r.b.E,RlcTab,2 );
		  case 0x04:  _ROT( r.b.H,RlcTab,2 );
		  case 0x05:  _ROT( r.b.L,RlcTab,2 );
		  case 0x06:  _ROT( B(Mem+r.w.HL),RlcTab,2 );
		  case 0x07:  _ROTA( RlcTab,2 );
		  case 0x08:  _ROT( r.b.B,RrcTab,2 );
		  case 0x09:  _ROT( r.b.C,RrcTab,2 );
		  case 0x0a:  _ROT( r.b.D,RrcTab,2 );
		  case 0x0b:  _ROT( r.b.E,RrcTab,2 );
		  case 0x0c:  _ROT( r.b.H,RrcTab,2 );
		  case 0x0d:  _ROT( r.b.L,RrcTab,2 );
		  case 0x0e:  _ROT( B(Mem+r.w.HL),RrcTab,2 );
		  case 0x0f:  _ROTA( RrcTab,2 );

		  case 0x10:  _ROT( r.b.B,RlTab,2 );
		  case 0x11:  _ROT( r.b.C,RlTab,2 );
		  case 0x12:  _ROT( r.b.D,RlTab,2 );
		  case 0x13:  _ROT( r.b.E,RlTab,2 );
		  case 0x14:  _ROT( r.b.H,RlTab,2 );
		  case 0x15:  _ROT( r.b.L,RlTab,2 );
		  case 0x16:  _ROT( B(Mem+r.w.HL),RlTab,2 );
		  case 0x17:  _ROTA( RlTab,2 );
		  case 0x18:  _ROT( r.b.B,RrTab,2 );
		  case 0x19:  _ROT( r.b.C,RrTab,2 );
		  case 0x1a:  _ROT( r.b.D,RrTab,2 );
		  case 0x1b:  _ROT( r.b.E,RrTab,2 );
		  case 0x1c:  _ROT( r.b.H,RrTab,2 );
		  case 0x1d:  _ROT( r.b.L,RrTab,2 );
		  case 0x1e:  _ROT( B(Mem+r.w.HL),RrTab,2 );
		  case 0x1f:  _ROTA( RrTab,2 );

		  case 0x20:  _ROT( r.b.B,SlaTab,2 );
		  case 0x21:  _ROT( r.b.C,SlaTab,2 );
		  case 0x22:  _ROT( r.b.D,SlaTab,2 );
		  case 0x23:  _ROT( r.b.E,SlaTab,2 );
		  case 0x24:  _ROT( r.b.H,SlaTab,2 );
		  case 0x25:  _ROT( r.b.L,SlaTab,2 );
		  case 0x26:  _ROT( B(Mem+r.w.HL),SlaTab,2 );
		  case 0x27:  _ROTA( SlaTab,2 );
		  case 0x28:  _ROT( r.b.B,SraTab,2 );
		  case 0x29:  _ROT( r.b.C,SraTab,2 );
		  case 0x2a:  _ROT( r.b.D,SraTab,2 );
		  case 0x2b:  _ROT( r.b.E,SraTab,2 );
		  case 0x2c:  _ROT( r.b.H,SraTab,2 );
		  case 0x2d:  _ROT( r.b.L,SraTab,2 );
		  case 0x2e:  _ROT( B(Mem+r.w.HL),SraTab,2 );
		  case 0x2f:  _ROTA( SraTab,2 );

		  case 0x38:  _ROT( r.b.B,SrlTab,2 );
		  case 0x39:  _ROT( r.b.C,SrlTab,2 );
		  case 0x3a:  _ROT( r.b.D,SrlTab,2 );
		  case 0x3b:  _ROT( r.b.E,SrlTab,2 );
		  case 0x3c:  _ROT( r.b.H,SrlTab,2 );
		  case 0x3d:  _ROT( r.b.L,SrlTab,2 );
		  case 0x3e:  _ROT( B(Mem+r.w.HL),SrlTab,2 );
		  case 0x3f:  _ROTA( SrlTab,2 );

		  case 0x40:  BIT( 0,r.b.B, 2 );
		  case 0x41:  BIT( 0,r.b.C, 2 );
		  case 0x42:  BIT( 0,r.b.D, 2 );
		  case 0x43:  BIT( 0,r.b.E, 2 );
		  case 0x44:  BIT( 0,r.b.H, 2 );
		  case 0x45:  BIT( 0,r.b.L, 2 );
		  case 0x46:  BIT( 0,B(Mem+r.w.HL), 2 );
		  case 0x47:  BIT( 0,r.b.A, 2 );

		  case 0x48:  BIT( 1,r.b.B, 2 );
		  case 0x49:  BIT( 1,r.b.C, 2 );
		  case 0x4a:  BIT( 1,r.b.D, 2 );
		  case 0x4b:  BIT( 1,r.b.E, 2 );
		  case 0x4c:  BIT( 1,r.b.H, 2 );
		  case 0x4d:  BIT( 1,r.b.L, 2 );
		  case 0x4e:  BIT( 1,B(Mem+r.w.HL), 2 );
		  case 0x4f:  BIT( 1,r.b.A, 2 );

		  case 0x50:  BIT( 2,r.b.B, 2 );
		  case 0x51:  BIT( 2,r.b.C, 2 );
		  case 0x52:  BIT( 2,r.b.D, 2 );
		  case 0x53:  BIT( 2,r.b.E, 2 );
		  case 0x54:  BIT( 2,r.b.H, 2 );
		  case 0x55:  BIT( 2,r.b.L, 2 );
		  case 0x56:  BIT( 2,B(Mem+r.w.HL), 2 );
		  case 0x57:  BIT( 2,r.b.A, 2 );

		  case 0x58:  BIT( 3,r.b.B, 2 );
		  case 0x59:  BIT( 3,r.b.C, 2 );
		  case 0x5a:  BIT( 3,r.b.D, 2 );
		  case 0x5b:  BIT( 3,r.b.E, 2 );
		  case 0x5c:  BIT( 3,r.b.H, 2 );
		  case 0x5d:  BIT( 3,r.b.L, 2 );
		  case 0x5e:  BIT( 3,B(Mem+r.w.HL), 2 );
		  case 0x5f:  BIT( 3,r.b.A, 2 );

		  case 0x60:  BIT( 4,r.b.B, 2 );
		  case 0x61:  BIT( 4,r.b.C, 2 );
		  case 0x62:  BIT( 4,r.b.D, 2 );
		  case 0x63:  BIT( 4,r.b.E, 2 );
		  case 0x64:  BIT( 4,r.b.H, 2 );
		  case 0x65:  BIT( 4,r.b.L, 2 );
		  case 0x66:  BIT( 4,B(Mem+r.w.HL), 2 );
		  case 0x67:  BIT( 4,r.b.A, 2 );

		  case 0x68:  BIT( 5,r.b.B, 2 );
		  case 0x69:  BIT( 5,r.b.C, 2 );
		  case 0x6a:  BIT( 5,r.b.D, 2 );
		  case 0x6b:  BIT( 5,r.b.E, 2 );
		  case 0x6c:  BIT( 5,r.b.H, 2 );
		  case 0x6d:  BIT( 5,r.b.L, 2 );
		  case 0x6e:  BIT( 5,B(Mem+r.w.HL), 2 );
		  case 0x6f:  BIT( 5,r.b.A, 2 );

		  case 0x70:  BIT( 6,r.b.B, 2 );
		  case 0x71:  BIT( 6,r.b.C, 2 );
		  case 0x72:  BIT( 6,r.b.D, 2 );
		  case 0x73:  BIT( 6,r.b.E, 2 );
		  case 0x74:  BIT( 6,r.b.H, 2 );
		  case 0x75:  BIT( 6,r.b.L, 2 );
		  case 0x76:  BIT( 6,B(Mem+r.w.HL), 2 );
		  case 0x77:  BIT( 6,r.b.A, 2 );

		  case 0x78:  BIT( 7,r.b.B, 2 );
		  case 0x79:  BIT( 7,r.b.C, 2 );
		  case 0x7a:  BIT( 7,r.b.D, 2 );
		  case 0x7b:  BIT( 7,r.b.E, 2 );
		  case 0x7c:  BIT( 7,r.b.H, 2 );
		  case 0x7d:  BIT( 7,r.b.L, 2 );
		  case 0x7e:  BIT( 7,B(Mem+r.w.HL), 2 );
		  case 0x7f:  BIT( 7,r.b.A, 2 );

		  case 0x80:  RES( 0,r.b.B, 2 );
		  case 0x81:  RES( 0,r.b.C, 2 );
		  case 0x82:  RES( 0,r.b.D, 2 );
		  case 0x83:  RES( 0,r.b.E, 2 );
		  case 0x84:  RES( 0,r.b.H, 2 );
		  case 0x85:  RES( 0,r.b.L, 2 );
		  case 0x86:  RES( 0,B(Mem+r.w.HL), 2 );
		  case 0x87:  RES( 0,r.b.A, 2 );

		  case 0x88:  RES( 1,r.b.B, 2 );
		  case 0x89:  RES( 1,r.b.C, 2 );
		  case 0x8a:  RES( 1,r.b.D, 2 );
		  case 0x8b:  RES( 1,r.b.E, 2 );
		  case 0x8c:  RES( 1,r.b.H, 2 );
		  case 0x8d:  RES( 1,r.b.L, 2 );
		  case 0x8e:  RES( 1,B(Mem+r.w.HL), 2 );
		  case 0x8f:  RES( 1,r.b.A, 2 );

		  case 0x90:  RES( 2,r.b.B, 2 );
		  case 0x91:  RES( 2,r.b.C, 2 );
		  case 0x92:  RES( 2,r.b.D, 2 );
		  case 0x93:  RES( 2,r.b.E, 2 );
		  case 0x94:  RES( 2,r.b.H, 2 );
		  case 0x95:  RES( 2,r.b.L, 2 );
		  case 0x96:  RES( 2,B(Mem+r.w.HL), 2 );
		  case 0x97:  RES( 2,r.b.A, 2 );

		  case 0x98:  RES( 3,r.b.B, 2 );
		  case 0x99:  RES( 3,r.b.C, 2 );
		  case 0x9a:  RES( 3,r.b.D, 2 );
		  case 0x9b:  RES( 3,r.b.E, 2 );
		  case 0x9c:  RES( 3,r.b.H, 2 );
		  case 0x9d:  RES( 3,r.b.L, 2 );
		  case 0x9e:  RES( 3,B(Mem+r.w.HL), 2 );
		  case 0x9f:  RES( 3,r.b.A, 2 );

		  case 0xa0:  RES( 4,r.b.B, 2 );
		  case 0xa1:  RES( 4,r.b.C, 2 );
		  case 0xa2:  RES( 4,r.b.D, 2 );
		  case 0xa3:  RES( 4,r.b.E, 2 );
		  case 0xa4:  RES( 4,r.b.H, 2 );
		  case 0xa5:  RES( 4,r.b.L, 2 );
		  case 0xa6:  RES( 4,B(Mem+r.w.HL), 2 );
		  case 0xa7:  RES( 4,r.b.A, 2 );

		  case 0xa8:  RES( 5,r.b.B, 2 );
		  case 0xa9:  RES( 5,r.b.C, 2 );
		  case 0xaa:  RES( 5,r.b.D, 2 );
		  case 0xab:  RES( 5,r.b.E, 2 );
		  case 0xac:  RES( 5,r.b.H, 2 );
		  case 0xad:  RES( 5,r.b.L, 2 );
		  case 0xae:  RES( 5,B(Mem+r.w.HL), 2 );
		  case 0xaf:  RES( 5,r.b.A, 2 );

		  case 0xb0:  RES( 6,r.b.B, 2 );
		  case 0xb1:  RES( 6,r.b.C, 2 );
		  case 0xb2:  RES( 6,r.b.D, 2 );
		  case 0xb3:  RES( 6,r.b.E, 2 );
		  case 0xb4:  RES( 6,r.b.H, 2 );
		  case 0xb5:  RES( 6,r.b.L, 2 );
		  case 0xb6:  RES( 6,B(Mem+r.w.HL), 2 );
		  case 0xb7:  RES( 6,r.b.A, 2 );

		  case 0xb8:  RES( 7,r.b.B, 2 );
		  case 0xb9:  RES( 7,r.b.C, 2 );
		  case 0xba:  RES( 7,r.b.D, 2 );
		  case 0xbb:  RES( 7,r.b.E, 2 );
		  case 0xbc:  RES( 7,r.b.H, 2 );
		  case 0xbd:  RES( 7,r.b.L, 2 );
		  case 0xbe:  RES( 7,B(Mem+r.w.HL), 2 );
		  case 0xbf:  RES( 7,r.b.A, 2 );

		  case 0xc0:  SET( 0,r.b.B, 2 );
		  case 0xc1:  SET( 0,r.b.C, 2 );
		  case 0xc2:  SET( 0,r.b.D, 2 );
		  case 0xc3:  SET( 0,r.b.E, 2 );
		  case 0xc4:  SET( 0,r.b.H, 2 );
		  case 0xc5:  SET( 0,r.b.L, 2 );
		  case 0xc6:  SET( 0,B(Mem+r.w.HL), 2 );
		  case 0xc7:  SET( 0,r.b.A, 2 );

		  case 0xc8:  SET( 1,r.b.B, 2 );
		  case 0xc9:  SET( 1,r.b.C, 2 );
		  case 0xca:  SET( 1,r.b.D, 2 );
		  case 0xcb:  SET( 1,r.b.E, 2 );
		  case 0xcc:  SET( 1,r.b.H, 2 );
		  case 0xcd:  SET( 1,r.b.L, 2 );
		  case 0xce:  SET( 1,B(Mem+r.w.HL), 2 );
		  case 0xcf:  SET( 1,r.b.A, 2 );

		  case 0xd0:  SET( 2,r.b.B, 2 );
		  case 0xd1:  SET( 2,r.b.C, 2 );
		  case 0xd2:  SET( 2,r.b.D, 2 );
		  case 0xd3:  SET( 2,r.b.E, 2 );
		  case 0xd4:  SET( 2,r.b.H, 2 );
		  case 0xd5:  SET( 2,r.b.L, 2 );
		  case 0xd6:  SET( 2,B(Mem+r.w.HL), 2 );
		  case 0xd7:  SET( 2,r.b.A, 2 );

		  case 0xd8:  SET( 3,r.b.B, 2 );
		  case 0xd9:  SET( 3,r.b.C, 2 );
		  case 0xda:  SET( 3,r.b.D, 2 );
		  case 0xdb:  SET( 3,r.b.E, 2 );
		  case 0xdc:  SET( 3,r.b.H, 2 );
		  case 0xdd:  SET( 3,r.b.L, 2 );
		  case 0xde:  SET( 3,B(Mem+r.w.HL), 2 );
		  case 0xdf:  SET( 3,r.b.A, 2 );

		  case 0xe0:  SET( 4,r.b.B, 2 );
		  case 0xe1:  SET( 4,r.b.C, 2 );
		  case 0xe2:  SET( 4,r.b.D, 2 );
		  case 0xe3:  SET( 4,r.b.E, 2 );
		  case 0xe4:  SET( 4,r.b.H, 2 );
		  case 0xe5:  SET( 4,r.b.L, 2 );
		  case 0xe6:  SET( 4,B(Mem+r.w.HL), 2 );
		  case 0xe7:  SET( 4,r.b.A, 2 );

		  case 0xe8:  SET( 5,r.b.B, 2 );
		  case 0xe9:  SET( 5,r.b.C, 2 );
		  case 0xea:  SET( 5,r.b.D, 2 );
		  case 0xeb:  SET( 5,r.b.E, 2 );
		  case 0xec:  SET( 5,r.b.H, 2 );
		  case 0xed:  SET( 5,r.b.L, 2 );
		  case 0xee:  SET( 5,B(Mem+r.w.HL), 2 );
		  case 0xef:  SET( 5,r.b.A, 2 );

		  case 0xf0:  SET( 6,r.b.B, 2 );
		  case 0xf1:  SET( 6,r.b.C, 2 );
		  case 0xf2:  SET( 6,r.b.D, 2 );
		  case 0xf3:  SET( 6,r.b.E, 2 );
		  case 0xf4:  SET( 6,r.b.H, 2 );
		  case 0xf5:  SET( 6,r.b.L, 2 );
		  case 0xf6:  SET( 6,B(Mem+r.w.HL), 2 );
		  case 0xf7:  SET( 6,r.b.A, 2 );

		  case 0xf8:  SET( 7,r.b.B, 2 );
		  case 0xf9:  SET( 7,r.b.C, 2 );
		  case 0xfa:  SET( 7,r.b.D, 2 );
		  case 0xfb:  SET( 7,r.b.E, 2 );
		  case 0xfc:  SET( 7,r.b.H, 2 );
		  case 0xfd:  SET( 7,r.b.L, 2 );
		  case 0xfe:  SET( 7,B(Mem+r.w.HL), 2 );
		  case 0xff:  SET( 7,r.b.A, 2 );

		  default:
		     r.w.PC = rPC-Mem;
		     RegisterDump( r,"ill opcode detected (CB extension)",0 );
		     CoreDump();
		     ExitCode = 3;
		     goto finished;
		     break;
		  }
            }
            break;
         case 0xcc:  CALLt( F_Z );
         case 0xcd:  CALL( W(rPC+1),3 );
         case 0xce:  ADC( B(rPC+1), 2 );
         case 0xcf:  CALL( 0x08,1 );

         case 0xd0:  RETf( F_C );
         case 0xd1:  POP( r.w.DE,1 );
         case 0xd2:  JPf( F_C );
         case 0xd4:  CALLf( F_C );
         case 0xd5:  PUSH( r.w.DE,1 );
         case 0xd6:  SUB( B(rPC+1), 2 );
         case 0xd8:  RETt( F_C );
         case 0xd9:                                                     // EXX
            _EX16( r.w.BC,r.w.BC2 );  _EX16( r.w.DE,r.w.DE2 );
            _EX16( r.w.HL,r.w.HL2 );  rPC += 1;  break;
         case 0xda:  JPt( F_C );
         case 0xd7:  CALL( 0x10,1 );
         case 0xdc:  CALLt( F_C );
         case 0xdd:  INDEX_OPS( IX );
         case 0xde:  SBC( B(rPC+1), 2 );
         case 0xdf:  CALL( 0x18,1 );

         case 0xe0:  RETf( F_PE );
         case 0xe1:  POP( r.w.HL,1 );
         case 0xe2:  JPf( F_PE );
         case 0xe3:                                                     // EX (SP),HL
            _EX16( W(Mem+r.w.SP),r.w.HL );  rPC += 1;  break;
         case 0xe4:  CALLf( F_PE );
         case 0xe5:  PUSH( r.w.HL,1 );
         case 0xe6:  AND( B(rPC+1),2 );
         case 0xe7:  CALL( 0x20,1 );
         case 0xe8:  RETt( F_PE );
         case 0xe9:  JP( r.w.HL );
         case 0xea:  JPt( F_PE );
         case 0xeb:                                                     // EX DE,HL
            _EX16( r.w.DE,r.w.HL );  rPC += 1;  break;
         case 0xec:  CALLt( F_PE );
         case 0xed:
            {
#ifdef DEBUG
	       ++CntOpED[ B(rPC+1) ];
#endif
               switch( B(rPC+1) )
		  {
		  case 0x00:
		     r.b.A = 0xff;
		     goto DEF_ed;

		  case 0x42:  SBC16( r.w.BC );
		  case 0x43:  LD16( W(Mem+W(rPC+2)),r.w.BC, 4 );
		  case 0x44:  NEG8( r.b.A );  break;
		  case 0x4a:  ADC16( r.w.BC );
		  case 0x4b:  LD16( r.w.BC,W(Mem+W(rPC+2)), 4 );

		  case 0x52:  SBC16( r.w.DE );
		  case 0x53:  LD16( W(Mem+W(rPC+2)),r.w.DE, 4 );
		  case 0x57:						// LD A,I
		     {
			if (opt_Quest) {
			   r.w.PC = rPC-Mem;
			   RegisterDump( r,"LD A,I (not meaningful)",0 );
			}
		     }
		     LD8( r.b.A,r.b.I,2 );
		  case 0x5a:  ADC16( r.w.DE );
		  case 0x5b:  LD16( r.w.DE,W(Mem+W(rPC+2)), 4 );

		  case 0x62:  SBC16( r.w.HL );
		  case 0x63:  LD16( W(Mem+W(rPC+2)),r.w.HL, 4 );
		  case 0x67:                                            // RRD
		     {
			unsigned char d1 = (r.b.A << 4) & 0xf0;
			unsigned char d2 = (B(Mem+r.w.HL) >> 4) & 0x0f;
			unsigned char d3 = B(Mem+r.w.HL) & 0x0f;
			B(Mem+r.w.HL) = d1 + d2;
			r.b.A = (r.b.A & 0xf0) + d3;
			r.b.F = (r.b.F & F_C) | FTab[r.b.A];
		     }
		     rPC += 2;
		     break;
		  case 0x6a:  ADC16( r.w.HL );
		  case 0x6b:  LD16( r.w.HL,W(Mem+W(rPC+2)), 4 );
		  case 0x6f:                                            // RLD
		     {
			unsigned char d1 = r.b.A & 0x0f;
			unsigned char d2 = (B(Mem+r.w.HL) >> 4) & 0x0f;
			unsigned char d3 = (B(Mem+r.w.HL) << 4) & 0xf0;
			B(Mem+r.w.HL) = d1 + d3;
			r.b.A = (r.b.A & 0xf0) + d2;
			r.b.F = (r.b.F & F_C) | FTab[r.b.A];
		     }
		     rPC += 2;
		     break;

		  case 0x72:  SBC16( r.w.SP );
		  case 0x73:  LD16( W(Mem+W(rPC+2)),r.w.SP, 4 );
		  case 0x7a:  ADC16( r.w.SP );
		  case 0x7b:  LD16( r.w.SP,W(Mem+W(rPC+2)), 4 );

		  case 0xa0:                                            // LDI
		     {
			B(Mem + r.w.DE++) = B(Mem + r.w.HL++);
			r.w.BC--;
			r.b.F = (r.b.F & (F_S|F_Z|F_C)) | (r.w.BC ? F_PE : 0);
		     }
		     rPC += 2;
		     break;
		  case 0xa1:                                            // CPI
		     {
			CP( B(Mem+r.w.HL),0 );       // changes r.b.F
			r.w.HL++;
			r.w.BC--;
			r.b.F = (r.b.F & ~F_PE)  |  (r.w.BC ? F_PE : 0);
		     }
		     rPC += 2;
		     break;
		  case 0xa8:                                            // LDD
		     {
			B(Mem + r.w.DE--) = B(Mem + r.w.HL--);
			r.w.BC--;
			r.b.F = (r.b.F & (F_S|F_Z|F_C)) | (r.w.BC ? F_PE : 0);
		     }
		     rPC += 2;
		     break;

		  case 0xb0:                                            // LDIR
		     do {
			B(Mem + r.w.DE++) = B(Mem + r.w.HL++);
		     } while (--r.w.BC != 0);
		     r.b.F &= (F_S|F_Z|F_C);
		     rPC += 2;
		     break;
		  case 0xb1:                                            // CPIR
		     {
			do {
			   CP( B(Mem+r.w.HL),0 );     // changes r.b.F
			   r.w.HL++;
			   r.w.BC--;
			} while (r.w.BC  &&  (r.b.F & F_Z) == 0);
			r.b.F = (r.b.F & ~F_PE)  |  (r.w.BC ? F_PE : 0);
		     }
		     rPC += 2;
		     break;
		  case 0xb8:                                            // LDDR
		     do {
			B(Mem + r.w.DE--) = B(Mem + r.w.HL--);
		     } while (--r.w.BC != 0);
		     r.b.F &= (F_S|F_Z|F_C);
		     rPC += 2;
		     break;
		  case 0xb9:                                            // CPDR
		     {
			do {
			   CP( B(Mem+r.w.HL),0 );     // changes r.b.F
			   r.w.HL--;
			   r.w.BC--;
			} while (r.w.BC  &&  (r.b.F & F_Z) == 0);
			r.b.F = (r.b.F & ~F_PE)  |  (r.w.BC ? F_PE : 0);
		     }
		     rPC += 2;
		     break;

		  case 0xff:
		     r.b.A = 0xff;
		     goto DEF_ed;

		  default:
		    DEF_ed:
		     r.w.PC = rPC-Mem;
		     RegisterDump( r,"ill opcode detected (ED extension)",0 );
		     CoreDump();
		     ExitCode = 3;
		     goto finished;
		     break;
		  }
            }
            break;
         case 0xee:  XOR( B(rPC+1),2 );
         case 0xef:  CALL( 0x28,1 );

         case 0xf0:  RETf( F_S );
         case 0xf1:  POP( r.w.AF,1 );
         case 0xf2:  JPf( F_S );
         case 0xf3:                                                     // DI
	    {
	       if (opt_Quest) {
		  r.w.PC = rPC-Mem;
		  RegisterDump( r,"DI (not meaningful)",0 );
	       }
	    }
            rPC += 1;
            break;
         case 0xf4:  CALLf( F_S );
         case 0xf5:  PUSH( r.w.AF,1 );
         case 0xf6:  OR( B(rPC+1),2 );
         case 0xf7:  CALL( 0x30,1 );
         case 0xf8:  RETt( F_S );
         case 0xf9:  LD16( r.w.SP,r.w.HL, 1 );
         case 0xfa:  JPt( F_S );                                        // JP M
         case 0xfb:                                                     // EI
	    {
	       if (opt_Quest) {
		  r.w.PC = rPC-Mem;
		  RegisterDump( r,"EI (not meaningful)",0 );
	       }
	    }
            rPC += 1;
            break;
         case 0xfc:  CALLt( F_S );
         case 0xfd:  INDEX_OPS( IY );
         case 0xfe:  CP( B(rPC+1),2 );   break;
         case 0xff:  CALL( 0x38,1 );
         default:
            r.w.PC = rPC-Mem;
            RegisterDump( r,"ill opcode detected",0 );
            CoreDump();
            ExitCode = 3;
            goto finished;
            break;
	 }

      if (rPC-Mem >= BDOS_BEG) {
         if (rPC-Mem == BDOS_BEG+6) {
            r.w.PC = rPC-Mem;
            Bdos( &r );
         }
         else if (rPC-Mem >= BIOS_BEG) {
            r.w.PC = rPC-Mem;
            Bios( &r );
         }
         else {
            r.w.PC = rPC-Mem;
            RegisterDump( r,"ill PC in BDOS/BIOS area",1 );
            ExitCode = 3;
            goto finished;
         }
         if (ExitCode != 0) {
            if (ExitCode < 0)
               ExitCode = 0;
            goto finished;
         }
         rPC = W(Mem+r.w.SP)+Mem;
         r.w.SP += 2;
      }
   }
  finished:
   r.w.PC = rPC-Mem;
   *preg = r;
}   // Z80Emu



static inline int ParEven( int A )
{
   int cnt = 0;
   for (;  A != 0;  A &= (A-1))
      cnt++;
   return( (cnt % 2) == 0 );
}   // ParEven



void Z80EmuInit( void )
//
//  materialisierte Funktionen (FB) berechnen
//
{
   static int RunningFromCore = 0;
   int A,C;

#ifdef DEBUG
   int i;
   for (i = 0;  i <= 255;  i++) {
      CntOp[i] = 0;
      CntOpED[i] = 0;
      CntOpCB[i] = 0;
      CntOpIX[i] = 0;
      CntOpIXCB[i] = 0;
      CntOpIY[i] = 0;
      CntOpIYCB[i] = 0;
   }
#endif

   if (RunningFromCore)
      return;

   RunningFromCore = 1;
   for (A = 0;  A <= 0xff;  A++)
      FTab[A] = ((A == 0)   ? F_Z  : 0)  |
                ((A >= 128) ? F_S  : 0)  |
                (ParEven(A) ? F_PE : 0);

   for (A = 0;  A <= 0xff;  A++) {
      for (C = 0;  C <= 1;  C++) {
         unsigned char res;

         res = (A & 0x80) ? ((A << 1) + 1) : (A << 1);
         RlcTab[A][C].b.F  = FTab[res] | ((A & 0x80) ? F_C : 0);
         RlcTab[A][C].b.A  = res;

         res = (C) ? ((A << 1) + 1) : (A << 1);
         RlTab[A][C].b.F   = FTab[res] | ((A & 0x80) ? F_C : 0);
         RlTab[A][C].b.A   = res;

         res = (A & 0x01) ? ((A >> 1) | 0x80) : (A >> 1);
         RrcTab[A][C].b.F  = FTab[res] | ((A & 0x01) ? F_C : 0);
         RrcTab[A][C].b.A  = res;

         res = (C) ? ((A >> 1) + 0x80) : (A >> 1);
         RrTab[A][C].b.F   = FTab[res] | ((A & 0x01) ? F_C : 0);
         RrTab[A][C].b.A   = res;

         res = A << 1;
         SlaTab[A][C].b.F  = FTab[res] | ((A & 0x80) ? F_C : 0);
         SlaTab[A][C].b.A  = res;

         res = (A & 0x80) ? ((A >> 1) + 0x80) : (A >> 1);
         SraTab[A][C].b.F  = FTab[res] | ((A & 0x01) ? F_C : 0);
         SraTab[A][C].b.A  = res;

         res = A >> 1;
         SrlTab[A][C].b.F  = FTab[res] | ((A & 0x01) ? F_C : 0);
         SrlTab[A][C].b.A  = res;
      }
   }

#if !(__i386__  &&  __GNUC__)
   for (C = 0;  C <= 1;  C++) {
      int B;
      for (B = 0;  B <= 0xff;  B++) {
         for (A = 0;  A <= 0xff;  A++) {
            int sres;
            unsigned char flg;

            flg = FTab[(A+B+C) & 0xff] & ~F_PE;
            if (A+B+C >= 0x100)
               flg |= F_C;
            if ((A&0x0f) + (B&0x0f) + C >= 0x10)
               flg |= F_H;
            sres = SIGN8(A) + SIGN8(B) + C;
            if (sres > 127  ||  sres < -128)
               flg |= F_PE;
            AddTab[C][B][A].b.F = flg;
            AddTab[C][B][A].b.A = A+B+C;

            flg = FTab[(A-B-C) & 0xff] & ~F_PE;
            if (A-B-C < 0)
               flg |= F_C;
            if ((A&0x0f) - (B&0x0f) - C < 0)
               flg |= F_H;
            sres = SIGN8(A) - SIGN8(B) - C;
            if (sres > 127  ||  sres < -128)
               flg |= F_PE;
            SubTab[C][B][A].b.F = flg | F_N;
            SubTab[C][B][A].b.A = A-B-C;
         }
      }
   }
#endif
}   // Z80EmuInit



//////////////////////////////////////////////////////////////////



#ifdef DEBUG
static void ShowTab( int Tab[], char *Id )
{
   int cnt,i;
   int max,maxndx,total;
   int first = 1;

   for (total = 0, i = 0;  i <= 255;  total += Tab[i++] )
      ;
   for (cnt = 0;  cnt < 40;  cnt++) {
      max    = Tab[0];
      maxndx = 0;
      for (i = 1;  i <= 255;  i++) {
	 if (Tab[i] > max) {
	    max    = Tab[i];
	    maxndx = i;
	 }
      }
      if (max == 0)
	 break;
      if (first) {
	 fprintf( DIAG,"\r\n%s, total %d:", Id,total );
	 first = 0;
      }
      if (cnt % 5 == 0)
	 fprintf( DIAG,"\r\n" );
      else
	 fprintf( DIAG," | " );
      fprintf( DIAG,"%02x: %-8d", maxndx,max );
      Tab[maxndx] = 0;
   }
   if (cnt)
      fprintf( DIAG,"\r\n" );
}   // ShowTab
#endif



void Z80EmuExit( void )
{
#ifdef DEBUG
   if (opt_Profile) {
      ShowTab( CntOp,    "Opcodes" );
      ShowTab( CntOpED,  "ED-Ext" );
      ShowTab( CntOpCB,  "CB-Ext" );
      ShowTab( CntOpIX,  "DD-Ext" );
      ShowTab( CntOpIXCB,"DDCB-Ext" );
      ShowTab( CntOpIY,  "FD-Ext" );
      ShowTab( CntOpIYCB,"FDCB-Ext" );
   }
#endif
}   // Z80EmuExit
