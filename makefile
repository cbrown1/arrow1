olinout: olinout.c cli.h cli.c
	gcc -B -Wall cli.c olinout.c -o olinout -lsndfile -ljack -lpthread

install:
	install olinout /usr/local/bin

uninstall:
	-rm /usr/local/bin/olinout
