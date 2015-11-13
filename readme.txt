CP/M80 2.2 emulation for OS/2 and DOS                        130494
-------------------------------------

This is a more or less complete emulation of the CP/M80 "operating
system" under OS/2 and DOS (porting to other OSes should be no
problem, porting to other CPUs might be a problem due to big/little
endian).

For me the emulation is/was required, because I have to develop S/W
for Z80 based systems, without actually having a CP/M80 machine (~8
years ago I had, using a Z80-card in dedicated DOS-WSs is annoying).
Other PD/SW emulations lack some features, I do require - thus
self-developing was announced.  The idea to put this piece of S/W
under the GPL were the benefits I have taken from the many projects of
the FSF.  I hope the emulator will be useful for at least a handful of
people having access to the Internet (but in fact Internet is not a
requirement).

But again: there is absolutely NO warranty about performance,
reliability and so on for this product.  In fact this piece of S/W is
not intended as a product - there will be no support in any direction.

For the complete bunch of limitations, have a look at the GPL in
COPYING or the most current version available at prep.ai.mit.edu.

Anyway, if you have ideas/complains/bug fixes or praise, do not
hesitate to post them to me (esp. the last ones).


Installation
------------
Installation is easy: just copy the binary C.EXE to a sub-dir in your
PATH, setup the environment and that's it.  The env-vars contain the
following:

CPEMOPT
    contains your set of standard options for the CPEM-emulation

CPEMMAP
    contains the standard CP/M-drive to OS-subdir mapping.  The
    drivers are separated by ';', i.e. 'CPMMAP=f:\bin;c:\os2\cpm'
    means map CPM-'A:' to OS-'f:\bin' and CPM-'B:' to OS-'c:\os2\cpm'
CPEMMAP0
  :     these are the dedicated maps for CP/M-user 0..15;  initial
  :     setup for the maps is done thru CPEMAP
CPEMMAP15
    Anyway the default drive/dir of the OS is inherited as default
    drive to CP/M.  Also for the matching CP/M drive only this directory
    can be used for read/write access of files.

CPEMPATH
    these is the most 'advanced' feature of this CP/M emulation !
    Files, that aren't found thru the mapping, are searched thru this
    path.  Another 'remarkable' thing is, that files found in this way
    are marked read-only (so put your compilers and so on in the
    CPEMPATH and not in CPEMAP!)

TERM & TERMCAP
    these are the optional env-vars for termcap usage.  Currently only
    tvi920/debug/none on the CP/M-side are emulated (for most of the
    screen based applications use the '-o' option to speed up screen
    output significantly -> output buffering).  Thanks to termcap, the
    emulator will run with nearly every terminal around (on OS-side) -
    standard output terminal for OS/2 and DOS will be ANSI (or
    something close) anyway.


Commandline-Options
-------------------
Most of the options are self-explaining, just type 'c' or whatever,
you have called the CPEM-emulator.

Don't mix-up the terminal emulation on CP/M-side and the one on
OS-side.  The term-emulation on CP/M-side is setup thru the '-t'
command-line option and the capabilities built in the CP/M-emulator,
whereas the term-emulation on OS-side is determined by the env-vars
TERM & TERMCAP; the capabilities on OS-side are characterized by the
TERMCAP-entry and the terminal (-emulator) on OS-side.

There exists an optimized version (C.EXE) and a debugging version
(CDEB.EXE) of the program.  The debugging version offers you more
command-line options for 'debugging' purposes (I'd never thought).
E.g. there is an option for register output in single steps (lots of
output!) and one for profiling for finding the most used opcodes and
so on...  The optimized version offers slightly higher thru-put.


File Searching
--------------
During open, files are searched in the following order:
-  if the CP/M drive equals the OS start drive, look for file
   in OS start drive/dir;  if found, file is read/write
-  if not found, search file thru mapping (CPEMAP);  if found and not
   OS start drive, file is read/write;  if found and OS start
   drive, file is read-only
-  if not found, search for file in the user-0 map;  if found, file
   is read-only
-  otherwise, search file thru path (CPEMPATH);  if found, the file
   is marked read-only

Note1: file creation/deletion and so on skips the mapping for user-0
       and file searching, i.e. the resulting OS path is generated
       directly thru map / start drive/dir
Note2: the above also implies, that the mapping is ignored for
       creation of files for the OS start drive
Note3: OS start drive/dir is the place, you started the emulator from
Note4: CP/M80 binaries have the extension .CPM (not .COM).  All
       binary-names are remapped internally.  The one exception is the
       name of CPMPROG in the commandline, where no remapping takes
       place;  anyway default-extension is .CPM again.


Weak Points
-----------
-  not complete (neither the CPU nor the BDOS nor the BIOS emulation)
-  documentation
-  comments (if present) are sometimes in german
-  drive remapping/searching may lead to unintended results,
   i.e. copying files from drive x: to drive x: can result in files in
   different subdirectories, which is necessarily not the intended
   thing
-  not the fastest emulation - but nevertheless:  it's written in C/C++
   with some optimizations for 386
-  there will be problems in porting the emulator to big-endian machines


Stronger Points
---------------
works with many CP/M80 software including turbo-pascal 3.0, turbo-modula
1.0, WordStar, Slr180-Assembler, Strukta-preprocessor, DSD-Debugger, ...


Requirements
------------
-  For usage under OS/2 no adds are required - anyway for usage under OS/2
   inside a DOS-box the RSX-DPMI-extender (RSX.EXE) is highly recommended.
-  For usage under DOS, you will need EMX.EXE or RSX.EXE as extenders.
-  For usage of the termcap-capabilities, you will need a definition-file
   termcap.dat


Acknowledgements
----------------
FSF               for GCC, G++ (2.5.8 used), libg++, EMACS (19.22 used)
Eberhard Mattes   for the GCC,EMACS ports to OS/2
Rainer Schnitker  for the RSX DPMI-DOS extender
others            (many of them) for porting utilities


EMAIL:    s_griech@ira.uka.de            (Karlsruhe, Germany)
