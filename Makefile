all:
	gcc client.c -o client.o
	gcc server.c -o server.o

runclient: all
	./client.o localhost 6969

runserver: all
	./server.o 6969

clean:
	rm *.o