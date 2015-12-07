all:
	gcc client.c -std=c99 -o client.o
	gcc server.c -std=c99 -o server.o

runclient: all
	./client.o localhost

runserver: all
	./server.o

clean:
	rm *.o
