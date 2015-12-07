/*
** client.c -- a stream socket client demo
** from http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <arpa/inet.h>

#define PORT "3490" // the port client will be connecting to 
#define MAXDATASIZE 256 // max number of bytes we can get at once 

static int sockfd;
static pthread_attr_t attr;
static int *sdptr;

// prompt the user for input
void prompt() {
	printf("> ");
	char input[MAXDATASIZE];
	fgets(input, MAXDATASIZE, stdin);
	for(int i = 0; i < MAXDATASIZE; i++){
		if(input[i] == '\n') {
			input[i] = '\0';
			break;
		}
	}

	if(send(sockfd, input, strlen(input), 0) == -1) {
		perror("send");
	}
}

void* commandthread(void *arg) {
	while(1) {
		prompt();
		sleep(2);
	}
}

void* responsethread(void* arg) {
	int num_bytes_recieved;
	char output[MAXDATASIZE];
	while(1) {
		if ((num_bytes_recieved = recv(sockfd, output, MAXDATASIZE-1, 0)) == -1) {
			perror("recv");
			exit(1);
		}

		if(num_bytes_recieved == 0) {
			printf("Session closed by server.\n");
			exit(1);
		}

		output[num_bytes_recieved] = '\0';

		printf("%s\n", output);
	}
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	if (argc != 2) {
		fprintf(stderr,"usage: make runclient [hostname]\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	while(1) {
		// loop through all the results and connect to the first we can
		for(p = servinfo; p != NULL; p = p->ai_next) {
			if ((sockfd = socket(p->ai_family, p->ai_socktype,
					p->ai_protocol)) == -1) {
				continue;
			}

			if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
				close(sockfd);
				continue;
			}

			break;
		}

		if (p == NULL) {
			fprintf(stderr, "Failed to connect, trying again...\n");
		} else {
			break;
		}
		// wait for 3 seconds and try again
		sleep(3);
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("Connecting to %s\n", s);

	 // all done with this structure
	freeaddrinfo(servinfo);

	pthread_t cthread;
	pthread_t rthread;
	
	if (pthread_attr_init(&attr) != 0) {
		perror("client: error in p_thread_attr_init()");
		exit(EXIT_FAILURE);
	}
	
	sdptr = (int *)malloc(sizeof(int));
	*sdptr = sockfd;
	if (pthread_create(&cthread, &attr, commandthread, sdptr) != 0) {
		perror("client: error in creating in command thread");
		exit(EXIT_FAILURE);
	}
	
	sdptr = (int *)malloc(sizeof(int));
	*sdptr = sockfd;
	if (pthread_create(&rthread, &attr, responsethread, sdptr) != 0) {
		perror("client: error in creating response thread");
		exit(EXIT_FAILURE);
	}
	
	pthread_join(cthread, NULL);
	pthread_join(rthread, NULL);

	close(sockfd);

	return 0;
}