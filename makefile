arrow1: cli.cpp io.cpp jack_client.cpp log.cpp main.cpp reactor.cpp
	g++ -std=gnu++14 -B -Wall cli.cpp io.cpp jack_client.cpp log.cpp main.cpp reactor.cpp -o arrow1 -lsndfile -ljack -lpthread -lboost_program_options

install:
	install arrow1 /usr/local/bin

uninstall:
	-rm /usr/local/bin/arrow1
