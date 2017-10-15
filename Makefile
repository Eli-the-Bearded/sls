# Used if you "make install"
BIN=/dir/to/install/it
MAN=/dir/to/install/it

# One of these should be fine.
# CFLAGS= 
# CFLAGS= -g
CFLAGS= -O

# Pick one, or your favaorite c compiler.
# CC=cc
CC=gcc

sls:	sls.o
	$(CC) -o sls sls.o

sls.o: sls.c sls-conf.h

install: sls
	-rm -f $(BIN)/sls
	strip sls
	chmod 755 sls
	mv sls $(BIN)
	chmod 444 sls.1
	cp sls.1 $(MAN)

clean:
	rm -f *.o core

distclean: clean
	rm -f sls

