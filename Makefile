default: server client
all: default

server: server.o
	gcc -o server server.o

client: client.o
	gcc -o client client.o

server.o: server.c
	gcc -pthread -std=gnu99 -c server.c 

client.o: client.c
	gcc -pthread -std=gnu99 -c client.c
	
clean:
	rm -f server
	rm -f client
	rm -f *.o
