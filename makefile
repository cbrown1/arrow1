arrow1: src/cli.cpp src/io.cpp src/jack_client.cpp src/log.cpp src/main.cpp src/reactor.cpp 
	g++ -std=gnu++14 -B -Wall src/cli.cpp src/io.cpp src/jack_client.cpp src/log.cpp src/main.cpp src/reactor.cpp -o out/arrow1 -lsndfile -ljack -lpthread -lboost_program_options

install:
	install out/arrow1 /usr/local/bin

uninstall:
	-rm /usr/local/bin/arrow1
