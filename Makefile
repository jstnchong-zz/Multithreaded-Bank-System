all:
	gcc client.c -o client.o
	gcc server.c -o server.o

runclient: all
	./client.o localhost

runserver: all
	./server.o

clean:
	rm *.o