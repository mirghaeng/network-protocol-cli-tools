# Makefile

client: client.o
	gcc -o client client.o
	mv client client.o ../bin

client.o: client.c
	gcc -c -std=c99 -Wall client.c	

clean:
	rm -f ../bin/*.dat ../bin/client ../bin/client.o	
