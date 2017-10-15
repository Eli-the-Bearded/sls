BIN= /usr/local/bin
MAN= /usr/local/man/man1
CFLAGS= -O

sls:	sls.o
	cc sls.o -o sls

install: sls
	rm -f $(BIN)/sls
	strip sls
	mv sls $(BIN)
	cp sls.1 $(MAN)

clean:
	rm *.o
