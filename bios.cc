// 	$Id: bios.cc 1.4 1994/04/12 22:08:19 rg Exp $	
//      Copyright (c) 1994 by R. Griech
//
//  Bios Simulation
//  ---------------
//  Simulation of the BIOS part of a CP/M machine.  This is very simple, because
//  currently only char-I/O is handled.  One more or less difficult thing is the
//  terminal emulation, because the ESC-sequences have to be catched and interpreted
//  and then the appropriate termcap function have to be called.  Be aware, that
//  most standard ANSI-drivers wont handle INS/DEL-line sequences (ANSICALL.DLL of
//  OS/2 2.x, ANSI.SYS of D@*!).  But e.g. ZANSI.SYS is a good choice for D@*!.
//
//  terminal emulation
//  ------------------
//  - this file contains also the terminal emulation, it is based on termcap
//  - the terminal emulation is tested only with the OS/2 ansi emulation available
//    under OS/2 (DOSCALL.DLL), the ANSI.SYS of D@*! and ZANSI.SYS for the same
//    program loader
//  - take care, that the TERM/TERMCAP refer your terminal in the DOS / OS/2 world,
//    and the terminal-type in the emulators commandline to the terminal, the CP/M
//    program is EXPECTING.  This terminals shoud be compatible to obtain good results
//    (i.e. height/width, line wrapping should have the same behavior)
//  - termcap is initialized on the first usage of itself.  I hope this doesn't interfer
//    with the previously sent character
//  - the emulations are far from complete (and far from optimal)
//  - one possible problem area is, that usual (good old) terminal only have 24 lines...
//



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termcap.h>
#include "glob.h"



typedef enum {none,tvi920,debug} Terminals;

static int KbdBuff = -1;
static Terminals Terminal = none;
static int TermCapUsed = 0;
static int OutBufDownCnt = 0;



//
//
//  get termcap entry
//
static char *TermCapGetEntry( char *entry )
{
   char *res = tgetstr( entry,0 );
   if (res == 0) {
      fprintf( stderr,"terminal knows nothing about termcaps '%s'   \r\n",entry );
      fprintf( stderr,"sorry, but your terminal (or your termcap entry) is to dumb   \r\n" );
      ExitCode = 3;
   }
   return( res );
}   // TermCapGetEntry



//
//
//  send a character to the terminal (intended for termcap)
//
#define TermPutc( ch )  (putchar( ch ))



//
//
//  send a string to the terminal (intended for termcap)
//
static void TermPuts( char *s )
{
   if (s) {
      while (*s)
	 TermPutc( *(s++) );
   }
}   // TermPuts



static void TermPutsTi( void )
{
   // don't use TermCapGetEntry here (endless loop on failure)
   TermPuts( tgetstr("ti",0) );
}   // TermPutsTi



static void TermPutsTe( void )
{
   // don't use TermCapGetEntry here (endless loop on failure)
   TermPuts( tgetstr("te",0) );
}   // TermPutsTe



//
//
//  init the terminal + termcap
//
static void TermCapInit( void )
{
   char *termtype = getenv ("TERM");
   int success;
     
   if (TermCapUsed)
      return;
   TermCapUsed = 1;

   //
   //
   //  fetch termcap entry
   //
   {
      if (termtype == 0)
         fatal ("Specify a terminal type with `setenv TERM <yourtype>'.\n");
     
      success = tgetent ( NULL, termtype);
      if (success < 0)
         fatal ("Could not access the termcap data base.\n");
      if (success == 0)
         fatal ("Terminal type `%s' is not defined.\n", termtype);
   }

   //
   //
   //  init the thingie
   //
   TermPutsTi();
}   // TermCapInit



//
//
//  exit the termcap
//
static void TermCapExit( void )
{
   if (!TermCapUsed)
      return;
   TermCapUsed = 0;
   TermPutsTe();
}   // TermCapExit



//
//
//  position the cursor
//
static int TermPutsCm( int x, int y )
{
   static char *cm = NULL;
   unsigned char TermBuf[50];

   if (!TermCapUsed)
      TermCapInit();
   if (cm == NULL)
      cm = TermCapGetEntry( "cm" );
   TermPuts( tparam(cm,TermBuf,sizeof(TermBuf),x,y) );
}   // TermGoto



//
//
//  generate subroutines for termcap calls (names will be TermPuts #TYP)
//
#define TermPutsXx( TYP )			\
static void TermPuts ## TYP( void )		\
{						\
   static char *TYP = NULL;			\
						\
   if (!TermCapUsed)				\
      TermCapInit();				\
   if (TYP == NULL)				\
      TYP = TermCapGetEntry( #TYP );		\
   TermPuts( TYP );				\
}   // TermPutXx

TermPutsXx( al )
TermPutsXx( ce )
TermPutsXx( cl )
TermPutsXx( dl )
TermPutsXx( me )
TermPutsXx( md )



static void EmuTvi920( unsigned char ch )
{
   static int EscDownCounter = 0;
   static int EscAdvance, EscNdx;
   static unsigned char EscBuffer[4];

   if (EscDownCounter == 0) {
      switch (ch)
	 {
	 case 0x00:
	    break;
	 case 0x0a:
	    TermPutc( '\n' );
	    break;
	 case 0x0d:
	    TermPutc( '\r' );
	    break;
	 case 0x1a:
	    TermPutscl();
	    break;
	 case 0x1b:
	    EscDownCounter = 1;
	    EscAdvance     = 0;
	    EscNdx         = 0;
	    break;
	 default:
	    TermPutc( ch );
	    break;
	 }
   }
   else {
      EscBuffer[EscNdx++] = ch;
      if (--EscDownCounter == 0) {
	 switch (EscBuffer[0])
	    {
	    case '=':
	       if (EscAdvance == 0)
		  EscDownCounter = 2;
	       else
		  TermPutsCm( EscBuffer[1]-' ',EscBuffer[2]-' ' );
	       break;
	    case '(':
	       TermPutsmd();
	       break;
	    case ')':
	       TermPutsme();
	       break;
	    case 'B':				// don't know ??
	       if (EscAdvance == 0)
		  EscDownCounter = 1;
	       else
		  TermPutsmd();
	       break;
	    case 'C':				// don't know ??
	       if (EscAdvance == 0)
		  EscDownCounter = 1;
	       else
		  TermPutsme();
	       break;
	    case 'E':
	       TermPutsal();
	       break;
	    case 'R':
	       TermPutsdl();
	       break;
	    case 'T':
	       TermPutsce();
	       break;
	    default:
	       TermPuts( "<esc>" );
	       TermPutc( EscBuffer[0] );
	       break;
	    }
	 if (EscDownCounter)
	    ++EscAdvance;
      }
   }
}   // EmuTvi920



static void DoConOut( unsigned char ch )
{
   switch (Terminal)
      {
      case none:
	 putchar( ch );
	 break;
      case debug:
	 if (ch < ' ')
	    printf( "<%02x>",ch );
	 else
	    putchar( ch );
	 break;
      case tvi920:
	 EmuTvi920( ch );
	 break;
      default:
	 fprintf( stderr,"fatal:  seems, that terminal '%s' is not yet implemented\r\n",
		 (char *)opt_Term );
	 exit( 3 );
      }
}   // DoConOut



void Bios( REGS *preg )
{
   REGS r = *preg;

#ifdef DEBUG
   if (opt_BsCalls)
      RegisterDump( r,"BIOS call",0 );
#endif

   switch (r.w.PC - BIOS_BEG) {
      case 0x03:                                                // WBOOT
         ExitCode = -10;
         break;
      case 0x06:                                                // CONST
         r.b.A = 0x00;
         if (KbdBuff >= 0)
            r.b.A = 0xff;
         else {
            if ((KbdBuff = _read_kbd(0,0,0)) >= 0)
               r.b.A = 0xff;
         }
	 if (opt_OutBuf  &&  --OutBufDownCnt < 0) {
	    OutBufDownCnt = opt_OutBuf;
	    fflush( stdout );
	 }
         break;
      case 0x09:                                                // CONIN
	 if (opt_OutBuf)
	    fflush( stdout );
         if (KbdBuff >= 0) {
            r.b.A = KbdBuff;
            KbdBuff = -1;
         } else
            r.b.A = _read_kbd(0,1,0);
         break;
      case 0x0c:                                                // CONOUT
	 DoConOut( r.b.C );
         break;
      default:
         RegisterDump( r,"BIOS function not implemented",0 );
         ExitCode = 3;
         break;
   }
   *preg = r;
}   // Bios



void BiosInit( void )
{
   int i;

   if (opt_Verbose)
      fprintf( stderr,"BiosInit...\r\n" );

   if (opt_Verbose)
      fprintf( stderr,"clearing memory\r\n" );
   memset( Mem,0,sizeof(Mem) );

   if (opt_Verbose)
      fprintf( stderr,"setting up BIOS jumps at 0x%x and 0x0000\r\n",BIOS_BEG );
   for (i = 0;  i <=32;  i++) {
      B(Mem+BIOS_BEG+3*i+0) = 0xc3;
      W(Mem+BIOS_BEG+3*i+1) = BIOS_BEG + 3*i;
   }
   B(Mem+0) = 0xc3;
   W(Mem+1) = BIOS_BEG+3;

   //
   //
   //  determine terminal type
   //
   if (opt_Term == ""  ||  opt_Term == "NONE")
      Terminal = none;
   else if (opt_Term == "920"  ||  opt_Term == "TVI920")
      Terminal = tvi920;
   else if (opt_Term == "DEBUG")
      Terminal = debug;
   else {
      fprintf( stderr,"fatal:  terminal type '%s' not defined\r\n",(char *)opt_Term );
      fprintf( stderr,"currently only 'tvi920'/'920', 'DEBUG', 'NONE' defined (sorry)\r\n" );
      exit( 3 );
   }
}   // BiosInit



void BiosExit( void )
{
   ////   printf("stub BiosExit()\r\n");
   TermCapExit();
}   // BiosExit
