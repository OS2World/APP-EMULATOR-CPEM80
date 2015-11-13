// 	$Id: util.cc 1.2 1994/04/12 22:04:42 rg Exp $
//      Copyright (c) 1994 by R. Griech
//
//  utility functions for CP/M
//  --------------------------
//  here are some utility functions
//



#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include "glob.h"



void fatal( char *fmt, char *msg )
{
   fprintf( stderr,"fatal error: " );
   fprintf( stderr,fmt,msg );
   exit( 3 );
}   // fatal



void RegisterDump( REGS r, char *ErrMsg, int DeArea )
{
   int i;
   unsigned char *op = Mem+r.w.PC;

   fprintf( DIAG,"\r\n--------------------------------------------------------\r\n" );
   fprintf( DIAG,"%s\r\n",ErrMsg );
   fprintf( DIAG,"ops:     %02x %02x %02x %02x\r\n",op[0],op[1],op[2],op[3] );
   fprintf( DIAG,"regs:    AF : %04x   BC : %04x   DE : %04x   HL : %04x\r\n",
                   r.w.AF,r.w.BC,r.w.DE,r.w.HL );
   fprintf( DIAG,"         AF': %04x   BC': %04x   DE': %04x   HL': %04x\r\n",
                   r.w.AF2,r.w.BC2,r.w.DE2,r.w.HL2 );
   fprintf( DIAG,"         IX : %04x   IY : %04x   SP : %04x   PC : %04x\r\n",
                   r.w.IX,r.w.IY,r.w.SP,r.w.PC );
   fprintf( DIAG,"stack:   " );
   for (i = 0;  i < 16;  i++) {
      fprintf( DIAG,"%04x", W(Mem+r.w.SP+2*i) );
      if (i == 7)
         fprintf( DIAG,"\r\n         " );
      else
         fprintf( DIAG," " );
   }
   fprintf( DIAG,"\r\n" );
   if (DeArea) {
      fprintf( DIAG,"*DE:     " );
      for (i = 0;  i < 32;  i++) {
         fprintf( DIAG,"%02x", B(Mem+r.w.DE+i) );
         if (i == 15)
            fprintf( DIAG,"\r\n         " );
         else
            fprintf( DIAG," " );
      }
      fprintf( DIAG,"\r\n" );
   }
   fprintf( DIAG,"--------------------------------------------------------\r\n" );
}   // RegisterDump



void CoreDump( void )
{
   int Hnd, res;

   Hnd = open( CORE,O_WRONLY|O_BINARY|O_CREAT|O_TRUNC,0666 );
   res = write( Hnd,Mem+TPA_BEG,CORESIZE );
   if (res < 0)
      fprintf( stderr,"can't create '%s'\r\n",CORE );
   else
      fprintf( stderr,"'%s' created\r\n",CORE );
   close( Hnd );
}   // CoreDump
