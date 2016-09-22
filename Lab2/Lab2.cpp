#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef __WIN32__
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#define MAX_QUEUE_SIZE 10
#define MAXDATASIZE 5000
class HTTP_parser
{
	// Search a header for a key-value. Returns the key value if there is any.
	// If a the header is not found a null string is returned.
	std::string replace_connection_to_closed(std::string input){
		std::string str = input;

		std::string connection("Connection: ");
		std::string replace_string ("close");
		int connection_index = str.find(&connection);
		int end_index = str.find("\\r\\n", connection_index);

		int start_pos = connection_index+connection.size();
		int replace_length = end_index - start_pos;
		str.replace(start_pos, replace_length, &replace_string );

		return str;
	}
};

class internal_side
{

	int sock;  // socket handle for listening process
	int connected_sock;	//socket for processes with a connection

	int addr_status;
	struct addrinfo hints;
	struct addrinfo* resp;
	struct addrinfo* p;
	struct sockaddr their_addr; // connector's address information
	socklen_t sin_size;
	char* cport;
  char s[INET6_ADDRSTRLEN];

	struct sigaction sa;

	static void sigchld_handler(int s)
	{
	    // waitpid() might overwrite errno, so we save and restore it:
	    int saved_errno = errno;

	    while(waitpid(-1, NULL, WNOHANG) > 0);

	    errno = saved_errno;
	}

	// get sockaddr, IPv4 or IPv6:
	void *get_IP_addr(struct sockaddr *sa)
	{
	    if (sa->sa_family == AF_INET) {
	        return &(((struct sockaddr_in*)sa)->sin_addr);
	    }

	    return &(((struct sockaddr_in6*)sa)->sin6_addr);
	}

	public:
	/* Used to probe for a message on the specified cport. When a connection is made
	 * the main process keeps probing while the forked process returns with the
	 * handle to a connection in connected_sock. */
	void probe_and_fork_connection(){
		while(true){
			sin_size = sizeof their_addr;
			connected_sock = accept(sock, (struct sockaddr*) &their_addr, &sin_size);
			if(connected_sock == -1){
				perror("accept");
				continue;
			}
			inet_ntop(their_addr.sa_family, get_IP_addr((struct sockaddr*) &their_addr), s, sizeof(s));
			printf("server: got connection from %s\n", s);

			if(!fork()){
				//Close listening socket for the processes with a connection
				close(sock);
				return;
			}
			close(connected_sock); // Close connected socket for
		}
	}


	//	Receive http request from client and forward to external side
	std::string receive_request(){
		char *buffer = new char[MAXDATASIZE];
		recv(connected_sock, buffer, MAXDATASIZE-1, 0);	//TODO: implement received buffer and process http get request.
		fprintf(stderr,"%s \n", buffer);			// Forward get request to external side.

		std::string request(str);
		return request;
	}

	void terminate_connection(){
		close(connected_sock);
		exit(0);
	}

	/* Method used to initialize the listening socket. */
	void init_socket(char* port){
		cport = port;
		//Define the socket type etc.
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE; // use my IP

		addr_status = getaddrinfo(NULL, cport, &hints, &resp);
		if (addr_status != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addr_status));
			exit(1);
		}


		//Loop through internal_side addrinfo response
		for(p=resp; p!=NULL; p=p->ai_next){
			if((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
				perror("server: socket");
				continue;
			}
			int yes = 1;
			if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
				perror("setsockopt");
				exit(1);
			}

			if(bind(sock, p->ai_addr, p->ai_addrlen) == -1){
				close(sock);
				perror("server: bind");
				continue;
			}
			break;
		}

		freeaddrinfo(resp);	//Free unused addr_info structures

		if(p==NULL){
			fprintf(stderr, "server: failed to bind\n");
			exit(1);
		}

		if (listen(sock, MAX_QUEUE_SIZE) == -1) {
			perror("listen");
			exit(1);
		}

		sa.sa_handler = sigchld_handler; // reap all dead processes
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART;
		if (sigaction(SIGCHLD, &sa, NULL) == -1) {
			perror("sigaction");
			exit(1);
		}

		printf("server: waiting for connections...\n");
	}
};

class external_side
{
	HTTP_parser parser; //Parser to manipulate received HTTP's.

	int sock;	//Handle to socket
	int numbytes; //
    char buf[MAXDATASIZE]; //Buffer for incoming messages
    struct addrinfo hints;
    struct addrinfo* resp;
    struct addrinfo* p; //
    int addr_status;
    char s[INET6_ADDRSTRLEN];
    char* _hostname;
    char* port;

	public:
    external_side(){
    	port = "80";
    }
	// get sockaddr, IPv4 or IPv6:
	void *get_in_addr(struct sockaddr *sa)
	{
    	if (sa->sa_family == AF_INET) {
    	    return &(((struct sockaddr_in*)sa)->sin_addr);
   		}

    	return &(((struct sockaddr_in6*)sa)->sin6_addr);
	}
	void init_socket(){
		//Define the socket type etc.
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		addr_status = getaddrinfo(_hostname, port, &hints, &resp);
		if (addr_status != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addr_status));
			exit(1);
		}

		//Loop through internal_side addrinfo response
		for(p=resp; p!=NULL; p=p->ai_next){
			if((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
				perror("client: socket");
				continue;
			}

			if(connect(sock, p->ai_addr, p->ai_addrlen) == -1){
				close(sock);
				perror("client: connect");
				continue;
			}
			break;
		}

		if(p==NULL){
			fprintf(stderr, "client: failed to connect\n");
			exit(2);
		}

		inet_ntop(p->ai_family, get_in_addr((struct sockaddr*) p->ai_addr), s, sizeof(s));
		printf("client: connecting to %s\n", s);

		freeaddrinfo(resp); // all done with this structure

		numbytes = recv(sock, buf, MAXDATASIZE-1, 0);
		fprintf(stderr, "numbytes: %d \n", numbytes);

		if (numbytes == -1) {
        	perror("recv");
        	exit(1);
    	}

    	buf[numbytes] = '\0';

    	printf("client: received '%s'\n",buf);

    	close(sock);
	}
 	void send_request(char** request){
		//TODO: Implement
	}
};

class proxy
{
	public:
	internal_side* internal;
	external_side* external;

	proxy(){
		internal = new internal_side();
		external = new external_side();
	}
	void init(char* c_port){
		internal->init_socket(c_port);
		//external->init_socket();
	}
	void run(){
		// The listening process will remain in this call:
		internal->probe_and_fork_connection();

		// Processes with a connection will start here:
		std::string request = internal->receive_request();

		external->send_request(&request);

		// Kill connected processes
		internal->terminate_connection();
	}
};

int main(int argc, char** argv)
{
	if (argc != 2) {
        fprintf(stderr,"usage: %s port\n",argv[0]);
        exit(1);
    }
	char* CPORT = argv[1];  // the port users will be connecting to
	proxy* myProxy = new proxy();
	myProxy->init(CPORT);
	myProxy->run();
	return 0;
}
