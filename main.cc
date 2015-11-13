// 	$Id: main.cc 1.3 1994/04/13 08:02:53 rg Exp $
//      Copyright (c) 1994 by R. Griech
//
//  CP/M emulator
//  -------------
//  This is the CmdArgs-interpreter and the CCP emulation of the CP/M
//  emulator.
//
//  environment-variablen:
//  ----------------------
//  CPEMOPT     Standardoptionen
//  CPEMAP      Default Mapping
//  CPEMAP0     Mapping fÅr User 0
//    :
//  CPEMAP15    Mapping fÅr User 15
//  CPEMPATH    Path for open file
//



#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
#include <Strng.h>

#define DEF_GLOBALS
#include "glob.h"

const Regex CpmFName("\\([A-P]:\\)?[-A-Z0-9$?*/]*\\(\\.[-A-Z0-9$?*/]*\\)?",1);
const Regex CpmFNameStart("[-A-Z0-9$?*/]",1);



static void Usage( void )
{
   fprintf( stderr,"\r\nCP/M emulation by rg (%s)\r\n", VERSION);
   fprintf( stderr,"\r\nusage:  cpem [-v[+|-]]                | verbose\r\n" );
//   fprintf( stderr,"\t     [-b bdospage]            | 2hex-digit for bdos beginning\r\n" );
   fprintf( stderr,"\t     [-o[+|-|{num}]]          | buffer output\r\n" );
   fprintf( stderr,"\t     [-t[{term}]]             | terminal name\r\n" );
   fprintf( stderr,"\t     [-u{usernum}]            | cpm user-number\r\n" );
   fprintf( stderr,"\t     [-C]                     | make a core dump after initialization\r\n" );
   fprintf( stderr,"\t     [-NT]                    | show questionable BDOS/BIOS calls\r\n" );
   fprintf( stderr,"\t                              | or not-tested opcodes\r\n" );
#ifdef DEBUG
   fprintf( stderr,"\t     [-B]                     | show BDOS/BIOS calls\r\n" );
   fprintf( stderr,"\t     [-P]                     | show a profile of the Z80 opcodes\r\r\n" );
   fprintf( stderr,"\t     [-S]                     | single step thru program\r\n" );
#endif
   fprintf( stderr,"\t     CPMPROG [cpm-cmdline]    | cpm command line\r\n");
   fprintf( stderr,"\t                              | CPMPROG is an OS/2-path\r\n" );
   fprintf( stderr,"optional environment paras: '%s','%s','%s',\r\n",ARG_OPT,ARG_MAP,ARG_PATH );
   fprintf( stderr,"\t\t\t    'TERM', 'TERMCAP'\r\n" );
   exit(3);
}   // Usage



static void GetArgs( int argc, char *argv[] )
{
   int i;

   opt_CoreDump = 0;
   opt_CpmArg   = "";
   opt_CpmCmd   = "";
   opt_OutBuf   = 0;
   opt_Quest    = 0;
   opt_Term     = "";
   opt_User     = 0;
   opt_Verbose  = 0;
   ExitCode     = 0;
   _getcwd2( StartPath,sizeof(StartPath) );
   StartDrive = toupper(*StartPath) -'A';
   opt_BsCalls    = 0;
   opt_SingleStep = 0;

   for (i = 1;  i < argc;  i++) {
      if (argv[i][0] == '-') {
         switch (argv[i][1])
	    {
	    case 't':
	       opt_Term = argv[i]+2;
	       break;
	    case 'o':
	       if (opt_OutBuf)
		  opt_OutBuf = 0;
	       else
		  opt_OutBuf = 100;
	       if (argv[i][2] == '-')
		  opt_OutBuf = 0;
	       else if (argv[i][2] == '+')
		  opt_OutBuf = 100;
	       else if (isdigit(argv[i][2]))
		  opt_OutBuf = atoi(argv[i]+2);
	       else if (argv[i][2])
		  Usage();
	       break;
	    case 'u':
	       opt_User = atoi(argv[i]+2);
	       if (opt_User < 0  ||  opt_User > 15)
		  Usage();
	       break;
	    case 'v':
	       opt_Verbose = !opt_Verbose;
	       if (argv[i][2] == '-')
		  opt_Verbose = 0;
	       else if (argv[i][2] == '+')
		  opt_Verbose = 1;
	       else if (argv[i][2])
		  Usage();
	       break;
	    case 'C':
	       if ( argv[i][2] )
		  Usage();
	       opt_CoreDump =1 ;
	       break;
	    case 'N':
	       if ( strcmp(argv[i],"-NT"))
		  Usage();
	       opt_Quest = 1;
	       break;
#ifdef DEBUG
	    case 'B':
	       if (argv[i][2])
		  Usage();
	       opt_BsCalls = 1;
	       break;
	    case 'P':
	       if (argv[i][2])
		  Usage();
	       opt_Profile = 1;
	       break;
	    case 'S':
	       if (argv[i][2])
		  Usage();
	       opt_SingleStep = 1;
	       break;
#endif
	    default:
	       Usage();
	       break;
	    }
      }
      else {
         //
         //
         //  erstes Zeichen mu· ein Blank sein (wenn was angegeben)
         //
         int j;
         opt_CpmCmd = argv[i++];
         for (j = i;  j < argc;  j++)
            opt_CpmArg = opt_CpmArg + " " + argv[j];
         break;
      }
   }
   if (opt_CpmCmd == "")
      Usage();

   opt_CpmCmd.upcase();
   opt_CpmArg.upcase();
   opt_Term.upcase();
   if (opt_Verbose) {
      fprintf( stderr,"CpmCmd:    %s\r\n",(char *)opt_CpmCmd );
      fprintf( stderr,"CpmArg:    %s\r\n",(char *)opt_CpmArg );
      fprintf( stderr,"CpmUser:   %d\r\n",opt_User );
      fprintf( stderr,"CpmTerm:   %s\r\n",(char *)opt_Term );
      fprintf( stderr,"StartPath: %s (extra search for the drive)\r\n",StartPath );
      fprintf( stderr,"CpemPath:  %s\r\n",getenv(ARG_PATH) );
      fprintf( stderr,"OutBufCnt: %d\r\n",opt_OutBuf );
   }
}   // GetArgs



static void GetEnvVars( void )
{
   String Map;
   int n;
   String words[16];
   int  d,u;
   char drv;

   //
   //  init of maps
   //
   for (u = 0;  u < 16;  u++) {
      drv = 'A';
      for (d = 0;  d < 16;  d++) {
         CpmMap[u][d] = String(drv) + ":.";
         ++drv;
      }
   }

   //
   //  get default map
   //
   Map = getenv( ARG_MAP );
   n = split( Map,words,16,String(";") );
   for (d = 0;  d < n;  d++) {
      if (words[d] != "") {
         for (u = 0;  u < 16;  u++)
            CpmMap[u][d] = words[d];
      }
   }

   //
   //  get other maps
   //
   for (u = 0;  u < 16;  u++) {
      char tmp[40];
      sprintf( tmp,"%s%d",ARG_MAP,u );
      Map = getenv( tmp );
      n = split( Map,words,16,String(";") );
      for (d = 0;  d < n;  d++) {
         if (words[d] != "")
            CpmMap[u][d] = words[d];
      }
   }

   //
   //  if verbose, then list the maps
   //
   if (opt_Verbose) {
      for (u = 0;  u < 16;  u++) {
         fprintf( stderr,"uMap %2d:  ",u );
         for (d = 0;  d < 16;  d++)
            fprintf( stderr,"%s;",(char *)CpmMap[u][d] );
         fprintf( stderr,"\r\n" );
      }
   }
}   // GetEnvVars



//---------------------------------------------------------



static void SetFcb( unsigned adr, String fn )
{
   String Drive, Ext;
   FCB *fcb = (FCB *)(Mem+adr);

   Drive = fn.through(":");
   if (Drive != String(""))
      fn = fn.after(":");
   Ext = fn.after(".",-1);
   if (Ext != String(""))
      fn = fn.before(".");

   fcb->dr = 0;
   if (Drive != String(""))
      fcb->dr = Drive[0] - '@';
   memset( fcb->f,' ',sizeof(fcb->f) );
   if (fn != String(""))
      memcpy( fcb->f,&(fn[0]),(fn.length() > 8) ? 8 : fn.length() );
   memset( fcb->t,' ',sizeof(fcb->t) );
   if (Ext != String(""))
      memcpy( fcb->t,&(Ext[0]),(Ext.length() > 3) ? 3 : Ext.length() );

   fcb->ex = 0;
   fcb->rc = 0;
}   // SetFcb


 
static void SetUpCmdLine( void )
{
   if (opt_Verbose)
      fprintf( stderr,"SetUpCmdLine...\r\n");

   strncpy( Mem+DEF_DMA+1,opt_CpmArg,0x7f );
   B(Mem+DEF_DMA) = (strlen(opt_CpmArg) > 0x7f) ? 0x7f : strlen(opt_CpmArg);
   {
      String tmp;
      String FName1, FName2;

      tmp = opt_CpmArg.from( CpmFNameStart );
      FName1 = tmp.through( CpmFName );
      if (opt_Verbose)
         fprintf( stderr,"FName1: %s\r\n",(char *)FName1 );
      tmp = tmp.after( FName1 );
      tmp = tmp.from( CpmFNameStart );
      FName2 = tmp.through( CpmFName );
      if (opt_Verbose)
         fprintf( stderr,"FName2: %s\r\n",(char *)FName2 );
      SetFcb( DEF_FCB1,FName1 );
      SetFcb( DEF_FCB2,FName2 );
   }
}   // SetUpCmdLine



static String SearchPath( String Path, String DefExt )
{
   char path[_MAX_PATH];
   char opath[_MAX_PATH];

   strcpy( path,Path );
   _defext( path,DefExt );
   _searchenv( path,ARG_PATH,opath );
   return( String(opath) );
}   // SearchPath



static void LoadProg( void )
{
   int hnd, res;
   String p;

   if (opt_Verbose)
      fprintf( stderr,"LoadProg...\r\n" );

   p = SearchPath( opt_CpmCmd,EXT_CPM );
   if (p == "") {
      fprintf( stderr,"fatal:  '%s' not found.\r\n",(char *)opt_CpmCmd );
      exit(3);
   }
   if (opt_Verbose)
      fprintf( stderr,"loading '%s'\r\n",(char *)p );

   hnd = open( p,O_RDONLY | O_BINARY );
   res = read( hnd,Mem+TPA_BEG,BDOS_BEG-TPA_BEG );
   if (res < 0  ||  res >= BDOS_BEG-TPA_BEG) {
      fprintf( stderr,"can't execute '%s'\r\n",(char *)p );
      exit( 3 );
   }
   close( hnd );
}   // LoadProg



static void SetUpRegs( REGS *r )
{
   if (opt_Verbose)
      fprintf( stderr,"SetUpRegs...\r\n" );
   r->w.AF  = 0;
   r->w.BC  = 0;
   r->w.DE  = 0;
   r->w.HL  = 0;
   r->w.AF2 = 0;
   r->w.BC2 = 0;
   r->w.DE2 = 0;
   r->w.HL2 = 0;
   r->w.IR  = 0;
   r->w.IX  = 0;
   r->w.IY  = 0;
   r->w.SP  = BDOS_BEG+0xfe;    // bei *0xfe ist 0 als RÅcksprungadresse
   r->w.PC  = TPA_BEG;
}   // SetUpRegs



static void DoCCP( void )
{
   REGS regs;

   if (opt_Verbose)
      fprintf( stderr,"DoCCP...\r\n" );
   LoadProg();
   SetUpCmdLine();
   SetUpRegs( &regs );
   Z80Emu( &regs );
}   // DoCCP



int main( int argc, char *argv[] )
{
   _fsetmode( stdout,"b" );
   _fsetmode( stderr,"b" );

   _envargs( &argc,&argv,ARG_OPT );
   GetArgs( argc,argv );
   if (opt_OutBuf)
      setvbuf( stdout,NULL,_IOFBF,1024 );

   GetEnvVars();

   Z80EmuInit();
   BiosInit();
   BdosInit();

   if (opt_CoreDump) {
      int Hnd = open("core",O_BINARY|O_TRUNC|O_CREAT|O_WRONLY,0666);
      _core(Hnd);
      fprintf( stderr,"core dumped...\r\n" );
      fprintf( stderr,"please rebind prog with 'emxbind -c cpem'\r\n" );
      exit(3);
   }

   DoCCP();
   BdosExit();
   BiosExit();
   Z80EmuExit();
   exit( ExitCode );
}   // main
