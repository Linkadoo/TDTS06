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
#endif

/*
struct sockaddr {
 unsigned short sa_family; // address family, AF_xxx
 char sa_data[14]; // 14 bytes of protocol address
};
*/


/*
struct addrinfo {
 int ai_flags; // AI_PASSIVE, AI_CANONNAME, etc.
 int ai_family; // AF_INET, AF_INET6, AF_UNSPEC
 int ai_socktype; // SOCK_STREAM, SOCK_DGRAM
 int ai_protocol; // use 0 for "any"
 size_t ai_addrlen; // size of ai_addr in bytes
 struct sockaddr *ai_addr; // struct sockaddr_in or _in6
 char *ai_canonname; // full canonical hostname
 struct addrinfo *ai_next; // linked list, next node
};
*/

class server_side
{
	int sock;  // socket handle
	struct addrinfo hints;
	struct addrinfo resp;
    struct sockaddr their_addr; // connector's address information
	int addr_status;
	int cport;
	
	public:
	void setPort(int port){
		cport = port;
		printf("%d", cport);
	}
	void init_socket(){
		//Define the socket type etc.
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE; // use my IP
		
		addr_status = getaddrinfo(NULL, cport, &hints, &resp)
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
	server_side* server = new server_side();
	client_side* client = new client_side();
	
	public:
	proxy(int c_port){
		server->setPort(c_port);
	}
	
	
};

int main(int argc, char** argv)
{
	int CPORT = atoi(argv[1]);  // the port users will be connecting to
	proxy* myProxy = new proxy(CPORT);
}
