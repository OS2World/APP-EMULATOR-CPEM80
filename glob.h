/* 	$Id: glob.h 1.2 1994/04/12 22:05:16 rg Exp $
 *      Copyright (c) 1994 by R. Griech
 *
 *  Global definitions for the CP/M emulator.
 *  Maybe in some parts it is no good programming style - but who cares
 */

#ifndef _GLOB_H_
#define _GLOB_H_


#include <stdlib.h>
#include <Strng.h>



#ifdef DEF_GLOBALS
#define EXTERN
#else
#define EXTERN extern
#endif



#define VERSION     "0.9.120494"
#define ARG_OPT     "CPEMOPT"
#define ARG_MAP     "CPEMAP"
#define ARG_PATH    "CPEMPATH"

#define EXT_CPM     "CPM"
#define EXT_COM     "COM"

#define BDOS_BEG    0xfe00
#define BIOS_BEG    0xff00
#define TPA_BEG     0x100

#define DEF_FCB1    0x005c
#define DEF_FCB2    0x006c
#define DEF_DMA     0x0080

#define MAX_FCBS    32

#define CORE        "./core.cpm"
#define CORESIZE    0xc000

struct _wordregs {
   unsigned short AF,BC,DE,HL,AF2,BC2,DE2,HL2,IR,IX,IY,SP,PC;
};
struct _byteregs {
   unsigned char  F, A, C, B, E, D, L, H;
   unsigned char F2,A2,C2,B2,E2,D2,L2,H2;
   unsigned char R,I,XL,XH,YL,YH,SL,SH,PL,PH;
};
union REGS {
   struct _wordregs w;
   struct _byteregs b;
};
#define F_S   0x80
#define F_Z   0x40
#define F_H   0x10
#define F_PE  0x04
#define F_N   0x02
#define F_C   0x01

#define W(P)  (*((unsigned short *)(P)))
#define B(P)  (*((unsigned char  *)(P)))
#define SB(P) (*((  signed char  *)(P)))

struct _FCB {
   unsigned char dr;
   unsigned char f[8];
   unsigned char t[3];			// muá hinter f stehen
   unsigned char ex;
   unsigned char s1;
   unsigned char s2;
   unsigned char rc;
   unsigned char d[16];
   unsigned char cr;
   unsigned char r[3];
};
typedef struct _FCB FCB;



EXTERN int    opt_OutBuf;
EXTERN int    opt_Verbose;
EXTERN int    opt_Quest;
EXTERN String opt_CpmArg;
EXTERN String opt_CpmCmd;
EXTERN int    opt_User;
EXTERN int    opt_CoreDump;
EXTERN int    opt_SingleStep;
EXTERN int    opt_BsCalls;
EXTERN int    opt_Profile;
EXTERN String opt_Term;
EXTERN String CpmMap[16][16];			// user,drive

EXTERN char          StartPath[_MAX_PATH];
EXTERN char          StartDrive;		// 0=A:,...
EXTERN unsigned char Mem[0x11000];		// Z80-memory (a little bit more than 64K
EXTERN int           ExitCode;			// if != 0, terminate

extern Regex CpmFName;
extern Regex CpmFNameStart;


#ifdef DEBUG
#  define DIAG stdout
#else
#  define DIAG stderr
#endif


void RegisterDump( REGS r, char *ErrMsg, int DeArea );
void CoreDump( void );
void Z80Emu( REGS *preg );
void Z80EmuInit( void );
void Z80EmuExit( void );
void Bdos( REGS *preg );
void BdosInit( void );
void BdosExit( void );
void Bios( REGS *preg );
void BiosInit( void );
void BiosExit( void );

void fatal( char *fmt, char *msg = NULL );


//
//  due to a missing declaration in stdio.h / termcap.h
//
extern "C" {
   int _fsetmode( FILE *stream, const char *mode );
   char *tparam( const char *s, char *out, int outlen, int arg0, int arg1, ... );
}


#endif
