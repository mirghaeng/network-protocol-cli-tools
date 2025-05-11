# Makefile

myweb: myweb.o
	gcc -lssl -lcrypto -o myweb myweb.o
	mv myweb myweb.o ../bin

myweb.o: myweb.c
	gcc -c -std=c99 -Wall myweb.c

clean:
	rm -f ../bin/output.dat ../bin/myweb ../bin/myweb.o
