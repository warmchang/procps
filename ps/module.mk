INSTALL += install_ps

-----

all: ps

ps: escape.o global.o help.o select.o sortformat.o output.o parser.o display.o
	$(CC) -o ps   escape.o global.o help.o select.o sortformat.o output.o parser.o display.o -L../proc -lproc

# This just adds the stacktrace code
debug: escape.o global.o help.o select.o sortformat.o output.o parser.o display.o stacktrace.o
	$(CC) -o ps   escape.o global.o help.o select.o sortformat.o output.o parser.o display.o stacktrace.o -L../proc -lproc -lefence

sortformat.o: sortformat.c common.h

global.o: global.c common.h

escape.o: escape.c

help.o: help.c

select.o: select.c common.h

output.o: output.c common.h

parser.o: parser.c common.h

display.o: display.c common.h

stacktrace.o: stacktrace.c


install: ps
	install $(OWNERGROUP) --mode a=rx --strip ps $(BINDIR)/ps
	install $(OWNERGROUP) --mode a=r ps.1 $(MAN1DIR)/ps.1
	-rm -f $(DESTDIR)/var/catman/cat1/ps.1.gz $(DESTDIR)/var/man/cat1/ps.1.gz

clean:
	rm -f *.o DEADJOE *~ core ps gmon.out
