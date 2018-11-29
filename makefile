udp: client server

server: server.cpp event.h
	g++ ./server.cpp -o server -Wno-format

client: client.cpp event.h
	g++ ./client.cpp -o client -Wno-format

clean:
	-rm *.o
	-rm client
	-rm server