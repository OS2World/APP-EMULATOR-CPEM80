CC        = gcc
GREP      = grep

ifeq ($(DEBUG),1)
   CCFLAGS   = -m486 -g -DDEBUG -Wall
else
   CCFLAGS   = -O2 -m486
endif

LDFLAGS   = -v
LIBS      = -lgpp -liostream -ltermcap
OBJS      = main.o bios.o bdos.o z80.o util.o

.cc.o:
	$(CC) $(CCFLAGS) -c $*.cc

c.exe:	$(OBJS)
	$(CC) $(CCFLAGS) -o c.exe $(OBJS) $(LIBS)

main.o:	main.cc glob.h

bios.o: bios.cc glob.h

bdos.o: bdos.cc glob.h

util.o: util.cc glob.h

z80.o: z80.cc glob.h
	$(CC) $(CCFLAGS) -mno-486 -c $*.cc

#	$(CC) $(CCFLAGS) -S $*.cc
#	$(GREP) -v align $*.s | $(CC) -c -o $*.o -x assembler -
