olinout: olinout.c
	gcc -B -Wall olinout.c -o olinout -lsndfile -ljack -lpthread

install:
	install olinout /usr/local/bin

uninstall:
	-rm /usr/local/bin/olinout
