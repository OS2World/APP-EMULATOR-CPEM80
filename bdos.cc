// 	$Id: bdos.cc 1.5 1994/04/13 08:02:31 rg Exp $
//      Copyright (c) 1994 by R. Griech
//
//  BDOS Simulation
//  ---------------
//  Trial of a CP/M 2.2 and partially 3.0 emulation.  This emu seems to run very
//  nice for many compilers (turbo pascal 3.0, turbo modula 1.0, another modula2-
//  compiler, the slr180-assembler, m80-assembler, strukta-preprocessor, some
//  debuggers and corresponding tools).  Maybe some system tools like pip and stat
//  will not work, but who cares...
//
//  possible problem areas
//  ----------------------
//  - the simulation is based mostly on 2.2.  Anyway there are some other functions
//    taken from CP/M3.0 for proper working of some proggies (e.g. DSD 'needs' the
//    system time (IMO pseudo copy protection))
//  - the simulation is not complete
//  - things not simulated:
//    - there is no multi sector counting for disk I/O (CP/M3.0 feature)
//    - no file attribute handling
//  - the CP/M BDOS can't be simulated completely by this OS;  this is because
//    - CP/M doesn't have any file handles and thus the FCBs can be moved around
//      in memory, thrown away and so on, without any problems for deleting/
//      renaming etc files.
//    - thus the simulations close is a NOOP.  On other specific operations, the 
//      simulation tries to close internally hold FCBs;  this might lead into trouble,
//      because (may be) there are some ambiguities between the found (thru search
//      algo) and the intended files
//  - the mapping of drives and pathes and so on might be ambigious (e.g. the path
//    of the calling OS-shell has priority)
//  - wildcard features are not fully supported, e.g you can't find deleted
//    directory entries (SearchFirst/Next)
//  - SearchFirst/Next don't reflect a CP/M drive (remember dir-extensions);
//    also they are much more tolerant according to the call order (it is
//    possible to do some other BDOS file calls intermediately);
//    the return dir-block is always zero
//  - CON/LPT as files are not allowed (because they can't be 'lseek'ed, but
//    nevertheless not caught
//  - sequential access after random access is limited to 2MByte (???)
//  - information about disk allocation is not reliable.  Instead a default
//    configuration is returned, i.e. there are some KB free on the current drive
//    but the existing files are NOT mirrored into the allocation map
//  - the write protect functions (28/29) have no effect
//  - Turbo-M2 for CP/M uses a not yet supported type of FCB access (the FCBs
//    for one file are moved around in memory according to calling conventions,
//    that the current version of the BDOS simulation doesn't understand
//    (WILL BE FIXED IN A FUTURE RELEASE)
//
//  real bugs
//  ---------
//  - the FCB->Hnd mapping is not very clear (even to me)
//  - some CP/M functions are real dummies (these thingies can be caught thru
//    the -NT option :-))
//
//  versions
//  --------
//  040494  after changing the FCB detection (for equality) from the FCB address
//          to the FCB contents, turbo-M2 seems to run without any problems
//  110494  SearchForFirst/Next are now handled thru opendir() et al.
//          Former version used _fnexplode(), which also found subdirs (not so
//          nice)
//



#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <conio.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
#include <time.h>
#include <Strng.h>
#include "glob.h"



#define DBP_ADR (BDOS_BEG+0x18)     /* 17 Bytes L„nge */
#define ALO_ADR (BDOS_BEG+0x40)     /* 30 Bytes L„nge */



struct KbdBuf {
   unsigned char mx;
   unsigned char nc;
   unsigned char c[0];
};

static unsigned short DmaAdr;
static unsigned short User;
static unsigned short DefDrive;     // 0=A:,...
static unsigned short WriteProtMap;
static DIR *SearchDir = NULL;
static FCB SearchPat;

struct {
   int Hnd;
   char Name[12];
} FcbHndMap[MAX_FCBS+1];

#define FCB_NULL (FCB *)-1
#define GET_FCB(A) ((FCB *)(Mem+A))
#define SET_EXIT_REGS16 { r.b.A = r.b.L;  r.b.B = r.b.H; }



static unsigned char ConSts( void )
{
   REGS rr;
   rr.w.PC = BIOS_BEG+0x06;  //CONST
   Bios( &rr );
   return( rr.b.A );
}   // ConSts



static unsigned char ConIn( void )
{
   REGS rr;
   rr.w.PC = BIOS_BEG+0x09;  // CONIN
   Bios( &rr );
   return( rr.b.A );
}   // ConIn



static void ConOut( unsigned char ch )
{
   REGS rr;
   rr.w.PC = BIOS_BEG+0x0c;  // CONOUT
   rr.b.C  = ch;
   Bios( &rr );
}   // ConOut



static void ConOutS( unsigned char *s )
{
   while (*s)
      ConOut( *(s++) );
}   // ConOutS



static String FcbToString( FCB *fcb )
//
//  konvertiert einen FCB in einen String (groágeschrieben)
//
{
   String FName;
   int i;
   unsigned char f[sizeof(fcb->f)+1];
   unsigned char t[sizeof(fcb->t)+1];

   for (i = 0;  i < sizeof(fcb->f);  i++)
      f[i] = toupper((fcb->f[i]) & 0x7f);
   f[i] = 0;
   for (i = 0;  i < sizeof(fcb->t);  i++)
      t[i] = toupper((fcb->t[i]) & 0x7f);
   t[i] = 0;

   //
   //  es wird jeweils bis zum ersten Blank weggeworfen, d.h.
   //  in den Dateinamen k”nnen keine Blanks vorkommen !!!
   //
   FName = String(fcb->dr + '@') + ":";
   FName += String(f);
   FName = FName.at( Regex("[^ ]*") );
   if (String(t) == String("COM"))
      FName += String(".CPM");
   else
   if (String(t) != String("   ")) {
      FName += "." + String(t);
      FName = FName.at( Regex("[^ ]*") );
   }

   if (fcb->dr > 16  &&  fcb->dr != '?') {
      fprintf( DIAG,"\r\n*** Fatal in FcbToString:  Drive > 16 (%s)\r\n",
              (char *)FName );
      exit( 3 );
   }
   return( FName );
}   // FcbToString



//
//
//  search a specific FCB in FcbHndMap
//  returns index into the FcbHndMap or -1 if not found
//  returns not, if fatal is set and the FCB is not found
//
static int SearchFcb( FCB *fcb, int fatal )
{
   int i;
   char Nam[sizeof(FcbHndMap[0].Name)];

   memcpy( Nam,fcb,sizeof(Nam) );
   if (Nam[0] == 0)
      Nam[0] = DefDrive + 1;

   for (i = 0;  memcmp(FcbHndMap[i].Name,Nam,sizeof(Nam)) != 0;  i++) {
      if (i >= MAX_FCBS) {
         if (fatal) {
            fprintf( DIAG,"\r\n*** Fatal in SearchFcb:  not found (%s)\r\n",
		    (char *)FcbToString(fcb));
            exit( 3 );
         }
         return( -1 );
      }
   }
   return( i );
}   // SearchFcb



static void MakeNewFcb( FCB *fcb, int Hnd )
{
   int FreeFcbHnd;

   for (FreeFcbHnd = 0;  FcbHndMap[FreeFcbHnd].Hnd != -1;  FreeFcbHnd++) {
      if (FreeFcbHnd >= MAX_FCBS) {
	 fprintf( DIAG,"\r\n*** Fatal in MakeNewFcb:  no more FCBs free\r\n" );
	 exit( 3 );
      }
   }
   memcpy( FcbHndMap[FreeFcbHnd].Name,fcb,sizeof(FcbHndMap[0].Name) );
   if (fcb->dr == 0)
      FcbHndMap[FreeFcbHnd].Name[0] = DefDrive + 1;
   FcbHndMap[FreeFcbHnd].Hnd = Hnd;
}   // MakeNewFcb



static void ImplicitClose( FCB *fcb )
{
   int FreeFcbHnd = SearchFcb( fcb,0 );

   if (FreeFcbHnd >= 0) {
#ifdef DEBUG
      if (opt_BsCalls)
         fprintf( DIAG,"close implicitly fcb '%s'\r\n",(char *)FcbToString(fcb) );
#endif
      close( FcbHndMap[FreeFcbHnd].Hnd );
      FcbHndMap[FreeFcbHnd].Hnd = -1;
   }
}   // ImplicitClose



//
//
//  implicit close for FCBs containing wildcards (delete file)
//
static void ImplicitCloseWild( FCB *fcb )
{
   int i,j;
   int Match;
   char Nam[sizeof(FcbHndMap[0].Name)];

   memcpy( Nam,fcb,sizeof(Nam) );
   if (Nam[0] == 0)
      Nam[0] = DefDrive + 1;

   for (i = 0;  i < MAX_FCBS;  i++) {
      if (FcbHndMap[i].Hnd != -1  &&  Nam[0] == FcbHndMap[i].Name[0]) {
	 Match = 1;
	 for (j = 1;  j < sizeof(FcbHndMap[0].Name);  j++) {
	    if (Nam[j] != '?'  &&  Nam[j] != FcbHndMap[i].Name[j]) {
	       Match = 0;
	       break;
	    }
	 }
	 if (Match) {
#ifdef DEBUG
	    if (opt_BsCalls)
	       fprintf( DIAG,"close implicitly wild-fcb '%s'\r\n",
		       (char *)FcbToString((FCB *)FcbHndMap[i].Name) );
#endif
	    close( FcbHndMap[i].Hnd );
	    FcbHndMap[i].Hnd = -1;
	 }
      }
   }
}   // ImplicitCloseWild



static inline int fexist( char *path )
{
   int Hnd = open( path,O_RDONLY );
   if (Hnd < 0)
      return( 0 );
   close( Hnd );
   return( 1 );
}   // fexist



static String _SearchPath( int drive, String fn, int Search )
{
   char path[_MAX_PATH];

   if (drive == '@')
      drive = DefDrive;
   else
      drive -= 'A';

   if (drive == StartDrive) {
      _makepath( path,NULL,StartPath,(char *)fn,NULL );
      if ( !Search)
         return( String(path) );
#ifdef DEBUG
      if (opt_BsCalls)
         fprintf( DIAG,"open:  trying '%s'\r\n",path );
#endif
      if (fexist(path))
         return( String(path) );
   }

   _makepath( path,NULL,CpmMap[User][drive],(char *)fn,NULL );
   if ( !Search)
      return( String(path) );
#ifdef DEBUG
   if (opt_BsCalls)
      fprintf( DIAG,"open:  trying '%s'\r\n",path );
#endif
   if (fexist(path))
      return( String(path) );

   if (User != 0) {
#ifdef DEBUG
      if (opt_BsCalls)
         fprintf( DIAG,"open:  trying '%s'\r\n",path );
#endif
      _makepath( path,NULL,CpmMap[0][drive],(char *)fn,NULL );
      if (fexist(path))
         return( String(path) );
   }
   return( "" );
}   // _SearchPath



static String SearchPath( String fn, int Search )
//
//  Datei suchen.
//  Die ersten beiden Buchstaben von 'fn' sind immer das Laufwerk.
//  Folgendermaáen wird gesucht:
//  - suche User,Laufwerk
//  - suche User=0,Laufwerk
//  - suche ber CPEMPATH
//  Wird im Startlaufwerk gesucht, so wird als erstes im Startver-
//  zeichnis gesucht...
//  Wird nix gefunden, so wird "" zurckgegeben, sonst der komplette Name
//  'Search' entscheidet ber die Operation:
//  0 - Filenamen nur entsprechend Laufwerk erweitern (fr MAKE, DELETE)
//  1 - File versuchen zu ”ffnen, mit Suchpfad (fr OPEN)
//  andere Werte reserviert
//
{
   String sn;
   int dr = fn[0];
   fn = fn.after(1);

   sn = _SearchPath(dr,fn,Search);
   if (sn != String(""))
      return( sn );

   if (Search) {
      char path[_MAX_PATH], opath[_MAX_PATH];

      strcpy( path,(char *)fn);
      _searchenv( path,ARG_PATH,opath );
#ifdef DEBUG
      if (opt_BsCalls)
         fprintf( DIAG,"open _searchenv found: '%s'\r\n",opath );
#endif
      return( String(opath) );
   }
   return( String("") );
}   // SearchPath



static void IncCr( FCB *fcb )
{
   int x = fcb->cr;

   ++x;
   if (x >= 128) {
      x = 0;
      ++(fcb->ex);
   }
   fcb->cr = x;
}   // IncCr



static int SearchPatOk( FCB *pat, dirent *DirEnt )
{
   int i;
   char fname[_MAX_FNAME], fext[_MAX_EXT];
   FCB name;

   String FName( DirEnt->d_name,DirEnt->d_namlen );
   FName.upcase();

   if (DirEnt->d_mode & A_DIR)
      return( 0 );
   if ( !FName.matches(CpmFName))
      return( 0 );
   _splitpath( FName, NULL,NULL,fname,fext );
   if (strlen(fname) > sizeof(name.f)  ||  strlen(fext) > sizeof(name.t)+1)
      return( 0 );

   memset( name.f,' ',sizeof(name.f)+sizeof(name.t) );
   memcpy( name.f,fname,strlen(fname) );
   if (strlen(fext) > 1)
      memcpy( name.t,fext+1,strlen(fext+1) );

   for (i = 0;  i < sizeof(name.f)+sizeof(name.t);  i++) {
      if (pat->f[i] != '?'  &&  pat->f[i] != name.f[i])
	 return( 0 );
   }
   return( 1 );
}   // SearchPatOk



static void FillSearcher( char *Name )
{
   char fname[_MAX_FNAME], ext[_MAX_EXT];
   int i;

   memset( Mem+DmaAdr,0,128 );
   memset( Mem+DmaAdr+1,' ',11 );
   _splitpath( Name, NULL,NULL,fname,ext );
   if ( !stricmp(ext,"."EXT_CPM))
      strcpy( ext,"."EXT_COM );

   B(Mem+DmaAdr+0) = User;
   for (i = 0;  i < 8;  i++) {
      if ( !fname[i])
         break;
      B(Mem+DmaAdr+1+i) = toupper( fname[i] );
   }
   if (ext[0]) {
      for (i = 0;  i < 3;  i++) {
         if ( !ext[i+1])
            break;
         B(Mem+DmaAdr+9+i) = toupper( ext[i+1] );
      }
   }
}   // FillSearcher



//////////////////////////////////////////////////////////////////////



void Bdos( REGS *preg )
{
   REGS r = *preg;

#ifdef DEBUG
   if (opt_BsCalls)
      RegisterDump( r,"BDOS call entry",1 );
#endif

   r.b.A  = 0xff;
   r.w.HL = 0;
   switch(r.b.C)
      {
      case 0:                                                   // system reset
         ExitCode = -10;
         break;
      case 1:                                                   // console input
         r.b.A = ConIn();
         if (r.b.A == 0x03) {
#ifdef DEBUG
            if (opt_Quest)
               RegisterDump( r,"Ctrl-C pressed (BDOS 1)",0 );
#endif
            ExitCode = 3;
            break;
         }
         ConOut( r.b.A );
         break;
      case 2:                                                   // console output
         ConOut( r.b.E );
         break;
      case 6:                                                   // direct console I/O
         r.b.A = 0;
         if (r.b.E == 0xff) {
            if (ConSts())
               r.b.A = ConIn();
         }
         else if (r.b.E == 0xfe) {
            if (ConSts())
               r.b.A = 0xff;
         }
         else if (r.b.E == 0xfd)
            r.b.A = ConIn();
         else {
            ConOut( r.b.E );
            r.b.A = r.b.E;
         }
         break;
      case 9:                                                   // print string
         {
            unsigned char *cp = Mem + r.w.DE;
            while (*cp != '$')
               ConOut( *(cp++) );
         }
         break;
      case 10:                                                  // read console buffer
         {
            unsigned char *ConBuff = Mem + ((r.w.DE == 0) ? DmaAdr : r.w.DE);
            unsigned char Buff[255];
            unsigned char ch;
            int  i;
            int  imax;

            i = 0;
            imax = ConBuff[0];
            for (;;) {
               ch = ConIn();
               if (ch == '\b') {
                  ConOutS( "\b \b" );
                  i--;
               }
               else if (ch == 0x1b) {
                  while (i--)
                     ConOutS( "\b \b" );
                  i = 0;
               }
               else if (ch == 0x03) {
                  if (i == 0) {
#ifdef DEBUG
                     if (opt_Quest)
                        RegisterDump( r,"Ctrl-C pressed (BDOS 10)",0 );
#endif
                     ExitCode = -10;
                     break;
                  }
               }
               else if (ch == '\n'  ||  ch == '\r')
                  break;
               else if (isprint(ch)) {
                  if (i >= imax)
                     ConOut( 0x07 );
                  else {
                     ConOut( ch );
                     Buff[i++] = ch;
                  }
               }
            }
            ConBuff[1] = i;
            strncpy( ConBuff+2,Buff,i );
         }
         break;
      case 11:                                                  // get console status
         {
            r.b.A = 0;
            if (ConSts())
               r.b.A = 1;
         }
         break;
      case 12:                                                  // return version number
         r.w.HL = 0x0022;
         SET_EXIT_REGS16;
         if (opt_Quest)
            RegisterDump( r,"BDOS Query Version Number",0 );
         break;
      case 13:                                                  // reset disk system
         DefDrive     = 0;
         DmaAdr       = DEF_DMA;
	 WriteProtMap = 0x0000;
         break;
      case 14:                                                  // select disk
         {
            DefDrive = r.b.E;
            r.b.A = 0;
         }
         break;
      case 15:                                                  // open file
         {
	    FCB *fcb = GET_FCB(r.w.DE);
            String fn = FcbToString( fcb );
            String OsPath;
            int Hnd;
            int fmode;

            ImplicitClose( fcb );

            fmode = O_RDONLY | O_BINARY;
            OsPath = SearchPath(fn,0);
            if (fexist((char *)OsPath))    // wenn Datei ohne search gefunden wird, dann
               fmode = O_RDWR | O_BINARY;  // darf sie auch beschrieben werden...
            else
               OsPath = SearchPath(fn,1);

#ifdef DEBUG
            if (opt_BsCalls)
               fprintf( DIAG,"open '%s'\r\n",(char *)OsPath );
#endif
            if (OsPath != String("")) {
               Hnd = open( (char *)OsPath,fmode );
               if (Hnd < 0) {
                  fprintf( DIAG,"\r\n*** Fatal during open: '%s' not found\r\n",
                           (char *)OsPath );
                  exit( 3 );
               }
	       MakeNewFcb( fcb,Hnd );
               r.b.A = 0;
            }
            else
               r.b.A = 0xff;
         }
         break;
      case 16:                                                  // close file
         {
#ifdef DEBUG
            if (opt_BsCalls) {
               String fn = FcbToString(GET_FCB(r.w.DE));
               String OsPath = SearchPath(fn,0);
               fprintf( DIAG,"close '%s' (in fact NoOp)\r\n",(char *)OsPath );
            }
#endif
	    r.b.A = 0;
         }
         break;
      case 17:                                                  // search for first
	 {
	    String fn = FcbToString(GET_FCB(r.w.DE));
	    String OsPath = SearchPath(fn.before(2),0);

	    if (fn[0] == '?') {
	       RegisterDump( r,"search for first with dr='?' not implemented",1 );
	       ExitCode = 3;
	       break;
	    }

	    if (SearchDir) {
	       closedir( SearchDir );
	       SearchDir = NULL;
	    }
	    SearchDir = opendir( (char *)OsPath );
	    rewinddir( SearchDir );
	    memcpy( &SearchPat,GET_FCB(r.w.DE),sizeof(FCB) );
	 }  // fall into 'search for next'
      case 18:                                                  // search for next
         {
            r.b.A = 0xff;
	    if (SearchDir) {
	       struct dirent *p;
	       while ((p = readdir(SearchDir)) != NULL  &&  !SearchPatOk(&SearchPat,p))
		  ;
	       if (p != NULL) {
		  FillSearcher( p->d_name );
		  r.b.A = 0;
	       }
	    }
         }
         break;
      case 19:                                                  // delete file
         {
	    FCB *fcb = GET_FCB(r.w.DE);
            String OsPath;
            char **flist;
            int p;
            String fn = FcbToString( fcb );

            ImplicitCloseWild( fcb );
            OsPath = SearchPath(fn,0);
            flist = _fnexplode( (char *)OsPath );

#ifdef DEBUG
            if (opt_BsCalls)
               fprintf( DIAG,"delete try '%s'\r\n",(char *)OsPath );
#endif
            r.b.A = 0xff;
            if (flist) {
	       r.b.A = 0;
               for (p = 0;  flist[p];  p++) {
#ifdef DEBUG
                  if (opt_BsCalls)
                     fprintf( DIAG,"delete '%s'\r\n",flist[p] );
#endif
                  if (unlink(flist[p]) != 0)
                     r.b.A = 0xff;
               }
               _fnexplodefree( flist );
            }
         }
         break;
      case 20:                                                  // read sequential
         {
            FCB *fcb = GET_FCB(r.w.DE);
            int Hnd = SearchFcb( fcb,1 );
            int radr = fcb->cr + 128*(fcb->ex);
            lseek( FcbHndMap[Hnd].Hnd,128*radr,SEEK_SET );
            int res = read( FcbHndMap[Hnd].Hnd,Mem+DmaAdr,128 );
            if (res <= 0)
               r.b.A = 1;
            else {
               if (res < 128)
                  memset( Mem+DmaAdr+res,0x1a,128-res );
               r.b.A = 0;
               IncCr( fcb );
            }
         }
         break;
      case 21:                                                  // write sequential
         {
            FCB *fcb = GET_FCB(r.w.DE);
            int Hnd = SearchFcb( fcb,1 );
            int wadr = fcb->cr + 128*(fcb->ex);
            lseek( FcbHndMap[Hnd].Hnd,128*wadr,SEEK_SET );
            int res = write( FcbHndMap[Hnd].Hnd,Mem+DmaAdr,128 );
            if (res < 128)
               r.b.A = 2;
            else {
               r.b.A = 0;
               IncCr( fcb );
            }
         }
         break;
      case 22:                                                  // make file
         {
	    FCB *fcb = GET_FCB(r.w.DE);
            String fn = FcbToString( fcb );
            String OsPath;

            ImplicitClose( fcb );
            OsPath = SearchPath(fn,0);

#ifdef DEBUG
            if (opt_BsCalls)
               fprintf( DIAG,"create '%s'\r\n",(char *)OsPath );
#endif
            if (fexist((char *)OsPath))
               r.b.A = 0xff;
            else {
               int Hnd = open( (char *)OsPath,O_RDWR|O_BINARY|O_CREAT|O_TRUNC,0666 );
               if (Hnd < 0)
                  r.b.A = 0xff;
               else {
		  MakeNewFcb( fcb,Hnd );
                  r.b.A = 0;
               }
            }
         }
         break;
      case 23:                                                  // rename file
         {
            String OsPath1, OsPath2;
            String f1 = FcbToString( GET_FCB(r.w.DE) );
            String f2 = FcbToString( GET_FCB(r.w.DE+0x10) );

	    //
	    //  first, look for wildcards
	    //
	    if (memchr(GET_FCB(r.w.DE+0x00),'?',12) != NULL  ||
		memchr(GET_FCB(r.w.DE+0x00),'*',12) != NULL  ||
		memchr(GET_FCB(r.w.DE+0x80),'?',12) != NULL  ||
		memchr(GET_FCB(r.w.DE+0x80),'*',12) != NULL)
	       r.b.A = 0xff;
	    else {
	       ImplicitClose( GET_FCB(r.w.DE) );

	       f2[0] = f1[0];    // Laufwerke gleichsetzen
	       OsPath1 = SearchPath( f1,0 );
	       OsPath2 = SearchPath( f2,0 );
#ifdef DEBUG
	       if (opt_BsCalls)
		  fprintf( DIAG,"rename '%s' to '%s'\r\n",(char *)OsPath1,(char *)OsPath2 );
#endif
	       r.b.A = 0xff;
	       if (fexist((char *)OsPath1)  &&  !fexist((char *)OsPath2)) {
		  if (rename(OsPath1,OsPath2) > 0)
		     r.b.A = 0;                  // ok !
	       }
            }
         }
         break;
      case 25:                                                  // return current disk
         r.b.A = DefDrive;
         break;
      case 26:                                                  // set DMA address
         DmaAdr = r.w.DE;
         break;
      case 27:                                                  // get addr(alloc)
         r.w.HL = ALO_ADR;
         SET_EXIT_REGS16;
         if (opt_Quest)
            RegisterDump( r,"BDOS Get Alloc Vector",0 );
         break;
      case 28:							// write protect disk
	 WriteProtMap |= (1 << DefDrive);
	 if (opt_Quest)
	    RegisterDump( r,"write protect disk",0 );
	 break;
      case 29:							// get read-only vector
	 r.w.HL = WriteProtMap;
	 if (opt_Quest)
	    RegisterDump( r,"get read-only vector",0 );
	 break;
      case 31:                                                  // get addr(dpb parms)
         r.w.HL = DBP_ADR;
         SET_EXIT_REGS16;
         if (opt_Quest)
            RegisterDump( r,"BDOS Query DPB",0 );
         break;
      case 32:                                                  // set/get user code
         if (r.b.E != 0xff)
            opt_User = r.b.E % 16;
         r.b.A = opt_User;
         break;
      case 33:                                                  // read random
         {
            FCB *fcb = GET_FCB(r.w.DE);
            int Hnd = SearchFcb( fcb,1 );
            int radr = 65536*(fcb->r[2]) + 256*(fcb->r[1]) + fcb->r[0];
            lseek( FcbHndMap[Hnd].Hnd,128*radr,SEEK_SET );
            int res = read( FcbHndMap[Hnd].Hnd,Mem+DmaAdr,128 );
            if (res <= 0)
               r.b.A = 1;
            else {
               if (res < 128)
                  memset( Mem+DmaAdr+res,0x1a,128-res );
               r.b.A = 0;
               fcb->cr = radr % 128;
               fcb->ex = radr / 128;
            }
         }
         break;
      case 34:                                                  // write random
         {
            FCB *fcb = GET_FCB(r.w.DE);
            int Hnd = SearchFcb( fcb,1 );
            int wadr = 65536*(fcb->r[2]) + 256*(fcb->r[1]) + fcb->r[0];
            lseek( FcbHndMap[Hnd].Hnd,128*wadr,SEEK_SET );
            int res = write( FcbHndMap[Hnd].Hnd,Mem+DmaAdr,128 );
            if (res <= 0)
               r.b.A = 2;
            else {
               r.b.A = 0;
               fcb->cr = wadr % 128;
               fcb->ex = wadr / 128;
            }
         }
         break;
      case 35:							// compute file size
	 {
            FCB *fcb = GET_FCB(r.w.DE);
            int Hnd = SearchFcb( fcb,1 );
	    long size;
	    size = (filelength(FcbHndMap[Hnd].Hnd) + 127) / 128;
	    fcb->r[2] = (size >> 16) & 0xff;
	    fcb->r[1] = (size >>  8) & 0xff;
	    fcb->r[0] = (size      ) & 0xff;
	    r.b.A = 0;
#ifdef DEBUG
	    if (opt_BsCalls)
	       fprintf( DIAG,"compute file size: %ld\r\n",size );
#endif
	 }
      case 36:							// get random record
	 r.b.A = 0;
	 if (opt_Quest)
	    RegisterDump( r,"get random record (ignored)",1 );
	 break;
      case 37:                                                  // reset drive
	 WriteProtMap &= ~r.w.DE;
         r.b.A = 0;
         if (opt_Quest)
            RegisterDump( r,"BDOS Reset Drive",0 );
         break;
      case 105:                                                 // get date and time
         {
            //
            //  diese Rechnerei mag nicht ganz korrekt sein, ist aber egal,
            //  da dies sowieso nur ein Hack fr den DSD ist
            //
            time_t Time = time(NULL);
            struct tm *Cur;
            unsigned Date;

            Cur  = localtime( &Time );
            Date = Cur->tm_yday + 365*(Cur->tm_year-78) +
	       (Cur->tm_year-78+2)/4;
            W(Mem+r.w.DE+0) = Date;
            B(Mem+r.w.DE+2) = Cur->tm_hour;
            B(Mem+r.w.DE+3) = Cur->tm_min;
            r.b.A = Cur->tm_sec;
         }
         break;
      default:
         RegisterDump( r,"BDOS function not implemented",1 );
         CoreDump();
         ExitCode = 3;
         break;
      }

#ifdef DEBUG
   if (opt_BsCalls)
      RegisterDump( r,"BDOS call exit",1 );
#endif

//   r.b.A = r.b.L;
//   r.b.H = r.b.B;
   *preg = r;
}   // Bdos



void BdosInit( void )
{
   int i;

   if (opt_Verbose)
      fprintf( DIAG,"BdosInit...\r\n" );

   if (opt_Verbose)
      fprintf( DIAG,"setting up BDOS jumps at 0x%x and 0x0005\r\n",BDOS_BEG);
   B(Mem+5) = 0xc3;
   W(Mem+6) = (BDOS_BEG + 6);
   B(Mem+BDOS_BEG+6) = 0xc3;
   W(Mem+BDOS_BEG+7) = (BDOS_BEG+6);

   if (opt_Verbose)
      fprintf( DIAG,"clearing FCB->Hnd-Map\r\n" );
   for (i = 0;  i < MAX_FCBS;  i++)
      FcbHndMap[i].Hnd = -1;

   if (opt_Verbose)
      fprintf( DIAG,"setting up misc (DMA,USER,DefDrive)\r\n" );
   DmaAdr       = DEF_DMA;
   User         = opt_User;
   DefDrive     = StartDrive;
   WriteProtMap = 0x0000;

   //
   //
   //  total 960K, 4K pro Block,  16K pro Track
   //  256 Dir-Eintr„ge, 512 Bytes physical sector size
   //
   if (opt_Verbose)
      fprintf( DIAG,"setting up Disk Parameter Block/Alloc Vector (dummy)\r\n" );
   W(Mem+DBP_ADR+ 0) = 128;      // SPT = 256 logical records/track (32K)
   B(Mem+DBP_ADR+ 2) = 5;        // BSH
   B(Mem+DBP_ADR+ 3) = 31;       // BLM    4K blocks
   B(Mem+DBP_ADR+ 4) = 3;        // EXM
   W(Mem+DBP_ADR+ 5) = 239;      // DSM = 240 blocks
   W(Mem+DBP_ADR+ 7) = 255;      // DRM = 256 dir entries
   B(Mem+DBP_ADR+ 9) = 0xc0;     // AL0 = 2 blocks reserved for dir entries
   B(Mem+DBP_ADR+10) = 0x00;     // AL1
   W(Mem+DBP_ADR+11) = 0x8000;   // CKS = permanently mounted
   W(Mem+DBP_ADR+13) = 2;        // OFF = 2 reserved tracks
   B(Mem+DBP_ADR+15) = 2;        // PSH
   B(Mem+DBP_ADR+16) = 3;        // PHM
   memset( Mem+ALO_ADR+00,0xea,18 );
   memset( Mem+ALO_ADR+20,0x50,12 );

}   // BdosInit



void BdosExit( void )
{
////   printf("stub BdosExit()\r\n");
}   // BdosExit
