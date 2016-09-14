#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#ifdef __WIN32__
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

class server_side
{
	int sock;  // socket handle
	struct addrinfo hints;
	struct addrinfo* resp;
    struct sockaddr their_addr; // connector's address information
	int addr_status;
	char* cport;
	
	public:
	void setPort(char* port){
		cport = port;
		printf("%s \n", cport);
	}
	void init_socket(){
		//Define the socket type etc.
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE; // use my IP
		
		addr_status = getaddrinfo(NULL, cport, &hints, &resp);
		printf("getaddrinfo: %d\n", (addr_status));
		if (addr_status != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addr_status));
			exit(1);
		}
	}
};

class client_side
{

};

class proxy
{
	server_side* server;
	client_side* client;
	
	public:
	proxy(char* c_port){
		server = new server_side();
		client = new client_side();

		server->setPort(c_port);
		server->init_socket();
	}
};

int main(int argc, char** argv)
{
	char* CPORT = argv[1];  // the port users will be connecting to
	proxy* myProxy = new proxy(CPORT);
	return 1;
}
