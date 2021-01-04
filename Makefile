all: server receiver agent

server:
	g++ server.cpp -o server `pkg-config --cflags --libs opencv` -pthread -std=c++11
receiver:
	g++ receiver.cpp -o receiver `pkg-config --cflags --libs opencv` -pthread -std=c++11
agent:
	g++ agent.c -o agent
clean:
	rm server receiver agent -rf