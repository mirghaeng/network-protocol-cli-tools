# Makefile

all: myserver myclient

myserver: myserver.o
	gcc -o myserver myserver.o
	mv myserver myserver.o ../bin

myserver.o: myserver.c
	gcc -c -std=gnu99 -Wall myserver.c

myclient: myclient.o
	gcc -lreadline -o myclient myclient.o
	mv myclient myclient.o ../bin

myclient.o: myclient.c
	gcc -c -std=gnu99 -Wall myclient.c

clean:
	rm -f ../bin/myserver ../bin/myserver.o ../bin/myclient ../bin/myclient.o
