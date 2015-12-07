/*
** server.c -- a stream socket server demo
** from http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

#define PORT "3490"  // the port users will be connecting to
#define MAXDATASIZE 64
#define FILESIZE 1048576

#define BACKLOG 10     // how many pending connections queue will hold

typedef struct account {
	char name[100];
	float balance;
	char insession;
} account;

static	account* mapped_mem; // mapped memory
static int sockfd;
static pthread_attr_t attr;
static int *sdptr;

int findaccount(char* name, account* mapped_mem, int numaccounts) {
	for(int i = 0; i < numaccounts; i++) {
		if(strcmp(name, mapped_mem[i].name) == 0) {
			return i;
		}
	}
	return -1;
}

char* argument(char* input) {
	// look for spaces
	int space = -1;
	for(int i = 0; i < strlen(input); i++) {
		if(input[i] == ' ') {
			if(space == -1) {
				space = i;
			} else {
				printf("Commands are at most two words\n");
				return NULL;
			}
		}
	}

	// one word
	if(space == -1) {
		return NULL;
	}

	// two words
	else {
		input[space] = '\0';
		return &(input[space + 1]);
	}
}

void sigchld_handler(int s) {
	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;
	while(waitpid(-1, NULL, WNOHANG) > 0);
	errno = saved_errno;
}

void print_bank_state() {
	printf("----------------------------------------\n");
	printf("\nCURRENT BANK STATE:\n");
		for(int i = 0; i < (((int*)mapped_mem)[(FILESIZE/sizeof(int))-1]); i++) {
			printf("%s\t\t%.2f\n", mapped_mem[i].name, mapped_mem[i].balance);
		}
	printf("----------------------------------------\n");
}

void monitor_bank_state() {
	while(1) {
		print_bank_state();
		sleep(20);
	}
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
	if(sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void) {
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	int pagesize;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes = 1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	// setup memory mapping
	mapped_mem = mmap(NULL, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
	(((int*)mapped_mem)[(FILESIZE/sizeof(int))-1]) = 0;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if(p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	if(listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if(sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	// spawn a thread to monitor the bank's state
	pthread_t print_thread;
	
	if (pthread_attr_init(&attr) != 0) {
		perror("client: error in p_thread_attr_init()");
		exit(EXIT_FAILURE);
	}
	
	sdptr = (int *)malloc(sizeof(int));
	*sdptr = sockfd;
	if (pthread_create(&print_thread, &attr, monitor_bank_state, sdptr) != 0) {
		perror("client: error in creating response thread");
		exit(EXIT_FAILURE);
	}

	printf("server: waiting for connections...\n");
	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if(new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("New connection from %s\n", s);

		// spawn child process
		if(!fork()) {
			// child doesn't need the listener
			close(sockfd);

			char input[MAXDATASIZE];
			char output[MAXDATASIZE];
			char* arg;
			int num_bytes_recieved;
			int session = -1;
			while(1) {
				for(int i = 0; i < MAXDATASIZE; i++){
					input[i] = 0;
				}
				// recieve input
				if((num_bytes_recieved = recv(new_fd, input, MAXDATASIZE-1, 0)) == -1) {
					perror("recv");
					exit(1);
				}

				if(num_bytes_recieved == 0) {
					printf("Connection closed by client.\n");
					exit(1);
				}

				// do things
				switch(input[0]) {
					case 'o': // open an account
					arg = argument(input);
					if(session != -1) {
						strcpy(output, "You cannot open an account with an session started.");
					} else {
						// look for the account with that name via linear search
						session = findaccount(arg, mapped_mem, (((int*)mapped_mem)[(FILESIZE/sizeof(int))-1]));
						// if the account was found...
						if(session != -1) {
							strcpy(output, "An account under that name already exists.");
						} else {
							mapped_mem[(((int*)mapped_mem)[(FILESIZE/sizeof(int))-1])].insession = 0;
							strcpy(mapped_mem[(((int*)mapped_mem)[(FILESIZE/sizeof(int))-1])].name, arg);
							mapped_mem[(((int*)mapped_mem)[(FILESIZE/sizeof(int))-1])].balance = 0;
							(((int*)mapped_mem)[(FILESIZE/sizeof(int))-1])++;
							printf("Created account with name '%s'.\n", arg);
							strcpy(output, "Success.");
						}
					}
					break;

					case 's': // start an account session
					arg = argument(input);
					// find the account with that name via linear search
					if(session != -1) {
						strcpy(output, "You already have a session active.");
						break;
					}
					session = findaccount(arg, mapped_mem, (((int*)mapped_mem)[(FILESIZE/sizeof(int))-1]));
					// if the account was not found
					if(session == -1) {
						strcpy(output, "No accounts under that name exist.");
					} else if(mapped_mem[session].insession != 1) {
						mapped_mem[session].insession = 1;
						printf("Started session for account '%s'.\n", arg);
						strcpy(output, "Success.");
					} else {
						strcpy(output, "That account is currently being accessed.");
					}
					break;

					case 'c': // credit an account
					arg = argument(input);
					if(session == -1) {
						strcpy(output, "You must start a session to credit an account.");
					} else {
						mapped_mem[session].balance += atof(arg);
						printf("Credited '%f' to current account.\n", atof(arg));
						strcpy(output, "Success.");
					}
					break;

					case 'd': // debit an account
					arg = argument(input);
					if(session == -1) {
						strcpy(output, "You must start a session to debit an account.");
					} else {
						mapped_mem[session].balance -= atof(arg);
						printf("Debited '%f' from current account.\n", atof(arg));
						strcpy(output, "Success.");
					}
					break;

					case 'b': // get the balance for an account
					if(session == -1) {
						strcpy(output, "You must start a session to get your balance.");
					} else {
						char balancestr[50];
						sprintf(balancestr, "%f", mapped_mem[session].balance);
						strcpy(output, strcat("Your balance is ", balancestr));
					}
					break;

					case 'f': // finish up
					if(session == -1) {
						strcpy(output, "You are not in a session.");
					} else {
						mapped_mem[session].insession = 0;
						session = -1;
						printf("Closed session.\n");
						strcpy(output, "Success.");
					}
					break;

					default:
					strcpy(output, "Invalid command.");
				}

				// send output
				if(send(new_fd, output, strlen(output), 0) == -1)
					perror("send");
			}
			close(new_fd);
			exit(0);
		}
		// parent doesn't need this
		close(new_fd);
		int status;
	}
	
	pthread_join(print_thread, NULL);

	return 0;
}