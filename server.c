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
#include <fcntl.h>
#include <sys/select.h>

#include "client_t.h"
#include "room_t.h"
#include "status.h"

#define PORT "3490"  /* the port users will be connecting to */
#define BACKLOG 10   /* how many pending connections queue will hold */
#define BUF_SIZE CLIENT_BUFFER_SIZE

static struct client_t *clients[BACKLOG];
static unsigned last_client;

static fd_set sockets; /* Used for select */

#define ROOM_COUNT 2
#define SERVER_THREAD_COUNT 1
/* The server will hold a thread for each room */
/*static pthread_t rooms[ROOM_COUNT + SERVER_THREAD_COUNT];*/
static struct room_t *rooms[ROOM_COUNT + SERVER_THREAD_COUNT];
static unsigned last_room; /* index of the last free room */

void build_select_list(int *sockfd, int *highsock)
{ /* Build the fd_set sockets structure */
	unsigned i = 0;
	FD_ZERO(&sockets);
	
	FD_SET(*sockfd, &sockets);
	while (i < BACKLOG) {
		if ((clients[i])->cl_sock != 0) {
			FD_SET((clients[i])->cl_sock, &sockets);
			if ((clients[i])->cl_sock > *highsock) 
				*highsock = (clients[i])->cl_sock;
		}
	}
}

void init_clients(void) 
{
	unsigned i = 0;
	while (i < BACKLOG) 
		clients[i++] = client_init();
}

void set_nonblocking(int *sock) 
{ /* Literally */
	int opts;

	opts = fcntl(*sock,F_GETFL);
	if (opts < 0) {
		perror("fcntl(F_GETFL)");
		exit(EXIT_FAILURE);
	}

	opts = (opts | O_NONBLOCK);
	if (fcntl(*sock,F_SETFL,opts) < 0) {
		perror("fcntl(F_SETFL)");
		exit(EXIT_FAILURE);
	}
}

int request_room(int request) 
{ /* See if there is any space in the requested room */
	if (request < 0 || request > ROOM_COUNT) {
		return -1;
	}
	return room_free_space(rooms[request]);
}

void handle_new_connection(int *sockfd) 
{
	int connection;
	char status;
	int room_request;

	/* FIXME: Save the IP somewhere, somehow */
	connection = accept(*sockfd, NULL, NULL); 
	
	if (connection < 0) {
		perror("accept");
		/* FIXME? */
		exit(EXIT_FAILURE);
	} else if (last_client == BACKLOG) {
		/* Error out, server is full */
		status = STATUS_SERVER_FULL;
		send(connection, &status, 1, 0);
		close(connection);
	} else if (
		(recv(connection, &room_request, sizeof(room_request), 0) > 0) 
		&& (request_room(room_request) < 0) 
		)
	{ 
		status = STATUS_ROOM_FULL;
		send(connection, &status, 1, 0);
		close(connection);
	} else {
		clients[last_client] = client_init();
		client_set_id(clients[last_client], last_client);
		client_set_sock(clients[last_client], connection);
		client_buff_clear(clients[last_client]);

		room_add_member(rooms[room_request], clients[last_client]);
		++last_client;
	}
}

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

void *get_in_addr(struct sockaddr *sa)
{ /* get sockaddr, IPv4 or IPv6: */

	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
	int sockfd, new_fd;  /* listen on sock_fd, new connection on new_fd */
	int *socks, highsock; /* selection list */
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; /* connector's address information */
	struct sigaction sa;
	socklen_t sin_size;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	char buf[BUF_SIZE];
	int rv, readsocks;
	struct timeval timeout;
	
	last_client = last_room = 0; 

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; /* Either IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; /* TCP */
	hints.ai_flags = AI_PASSIVE; /* use my IP */

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	/* loop through all the results and bind to the first we can */
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
						p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
					sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}
	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); /* all done with this structure */

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	/* reap all dead processes, legacy */
	sa.sa_handler = sigchld_handler; 
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	timeout.tv_sec = 2; /* Setting timeout to 2 seconds */
	timeout.tv_usec = 0; /* and 0 microseconds */
	set_nonblocking(&sockfd);
	
	highsock = sockfd;

	init_clients();
	while (1) {
		build_select_list(&sockfd, &highsock);

		readsocks = select(highsock + 1, &sockets, (fd_set *) 0, 
				(fd_set *) 0, &timeout);

		if (readsocks < 0) {
			perror("select");
			exit(EXIT_FAILURE);
		} else if (readsocks == 0) {
			printf("Nothing connected yet, just reporting\n");
		} else {
			handle_new_connection(&sockfd);
		}
	}

	#if 0
	while(1) {  /* main accept() loop */
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
				get_in_addr((struct sockaddr *)&their_addr),
				s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork()) { /* child */
			close(sockfd); /* child doesn't need the listener */
			/*if (send(new_fd, "Hello, world!", 13, 0) == -1) // This here does all the work
				perror("send");
			*/
			while (recv(new_fd, buf, BUF_SIZE, 0) != -1) 
				write(STDOUT_FILENO, buf, BUF_SIZE);
			close(new_fd);
			exit(0);
		}
		close(new_fd);  /* parent doesn't need this */
	}
	#endif
	
return 0;
}

