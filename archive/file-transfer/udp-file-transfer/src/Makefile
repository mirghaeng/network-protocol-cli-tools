# Makefile

myclient:
	gcc -std=c99 -Wall -Werror -lpthread -o myclient myclient.c
	mv myclient ../bin

myserver:
	gcc -std=c99 -Wall -Werror -o myserver myserver.c
	mv myserver ../bin

clean:
	rm -f ../bin/myclient ../bin/myserver
