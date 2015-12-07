all:
	gcc client.c -pthread -std=gnu99 -o client.o
	gcc server.c -pthread -std=gnu99 -o server.o

runclient: all
	./client.o localhost

runserver: all
	./server.o

clean:
	rm *.o
