olinout: cli.cpp io.cpp jack_client.cpp log.cpp main.cpp reactor.cpp
	g++ -std=gnu++14 -B -Wall cli.cpp io.cpp jack_client.cpp log.cpp main.cpp reactor.cpp -o olinout -lsndfile -ljack -lpthread -lboost_program_options

install:
	install olinout /usr/local/bin

uninstall:
	-rm /usr/local/bin/olinout
