#include <string>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <regex.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <algorithm>
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
#define MAXDATASIZE 1000000
class HTTP_parser
{
public:
	// Search the input string for string find. The first value that is found
	// will be replaced by replace_string and the new string returned.
	// If it's not found the process is exited.
	std::string replace_content(std::string input, std::string find,
																std::string replace_string){
		std::string return_string;

		int find_index = input.find(find);
		int first_part = find_index + find.size();
		int second_part = input.find("\r\n", find_index);

		if(first_part == find.size()-1 || second_part == -1)
			//Bad request
			exit(1);

		std::string start = input.substr(0,first_part);
		std::string end = input.substr(second_part);

		return_string = start + replace_string + end;
		return return_string;
	}

	// Searches input string and removes the characters from string "find" to
	// \r\n including \r\n, that is the rest of that line. The new string without
	// that line is returned. If the string is not found the process exites.
	std::string remove_content(std::string input, std::string find){
		std::string return_string;

		int find_index = input.find(find);
		int first_part = find_index;
		int second_part = input.find("\r\n", find_index);

		if(first_part == find.size()-1 || second_part == -1)
			//Bad request
			exit(1);

		std::string start = input.substr(0,first_part);
		std::string end = input.substr(second_part+2);

		return_string = start + end;
		return return_string;
	}

	// Searches the string request for the string find_string and returns the
	// string after find_string which ends with \r\n. Returns the URL as string.
	std::string get_url_info(std::string request, std::string find_string){
		std::string URL;
		int url_index;

		int start_index = request.find(find_string) + find_string.length();
		int end_index = request.find("\r\n", start_index);
		int length = end_index - start_index;
		URL = request.substr(start_index, length);
		return URL;
	}

	// Returns true if text, false if image
	bool is_text(std::string message){
		std::string str = "Content-Type: ";

		int start_index = message.find(str) + str.length();
		int end_index = message.find("\r\n", start_index);

		//Checking substring (content-type) after type
		int length = end_index - start_index;
		std::string type = message.substr(start_index, length);
		if (type.find("text") != -1)
			return true;
		return false;
	}

	// returns the content_length of the message as an integer
	int get_content_length(char* msg){
		const std::string content_length("Content-Length: ");
		std::string input(msg);
		int index = input.find(content_length) + content_length.size();
		int second_index = input.find("\r\n", index);

		std::string length = input.substr(index,second_index-index);
		return atoi(length.c_str());
	}

	// returns true if inappropriate content is found in the input text
	bool inappropriate_content(std::string text){
		bool inappropriate = false;
		std::string spongebob = "spongebob";
		std::string britney = "britney spears";
		std::string paris = "paris hilton";
		std::string norr = "norrköping";
		std::string norr1 = "norrkoping";
		std::string norr2 = "norrk&ouml;ping";

		if (text.find(spongebob) != -1 ||
			text.find(britney) != -1 ||
			text.find(paris) != -1 ||
			text.find(norr) != -1 ||
			text.find(norr1) != -1 ||
			text.find(norr2) != -1){
				inappropriate = true;
			}
		return inappropriate;
	}
};


// Internal class for handling communication between client and proxy
class internal_side
{
public:
	int sock;  // socket handle for listening process
	int connected_sock;	//socket for processes with a connection
	int pid_;

	int addr_status;
	struct addrinfo hints;
	struct addrinfo* resp;
	struct addrinfo* p;
	struct sockaddr their_addr; // connector's address information
	socklen_t sin_size;
	char* cport;
	char s[INET6_ADDRSTRLEN];
	char forward_buffer[MAXDATASIZE];

	struct sigaction sa;

	static void sigchld_handler(int s){
	    // waitpid() might overwrite errno, so we save and restore it:
	    int saved_errno = errno;

	    while(waitpid(-1, NULL, WNOHANG) > 0);

	    errno = saved_errno;
	}

	// get sockaddr, IPv4 or IPv6:
	void *get_IP_addr(struct sockaddr *sa){
	    if (sa->sa_family == AF_INET) {
	        return &(((struct sockaddr_in*)sa)->sin_addr);
	    }

	    return &(((struct sockaddr_in6*)sa)->sin6_addr);
	}

	public:
	/* Used to probe for a message on the specified cport. When a connection is
	 made
	 * the main process keeps probing while the forked process returns with the
	 * handle to a connection in connected_sock. */
	void probe_and_fork_connection(){
		while(true){
			sin_size = sizeof their_addr;
			connected_sock = accept(sock, (struct sockaddr*) &their_addr, &sin_size);
			if(connected_sock == -1){
				perror("accept");
				//continue; //TODO: reimplement
			}
			inet_ntop(their_addr.sa_family,
				 get_IP_addr((struct sockaddr*) &their_addr), s, sizeof(s));
			pid_ = fork();
			if(!pid_){
				//Close listening socket for the processes with a connection
				fprintf(stderr, "Process starting\n");
				close(sock);
				return;
			}
			close(connected_sock); // Close connected socket for
		}
	}

	//	Receive http request from client and forward to external side
	std::string receive_request(){
		char *buffer = new char[MAXDATASIZE];
		recv(connected_sock, buffer, MAXDATASIZE-1, 0);
		std::string request(buffer);
		return request;
	}

	void terminate_connection(){
		close(connected_sock);
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

	// returns the http request for bad content
	const char* reroute_content(){
		std::string url ="http://www.ida.liu.se/~TDTS04/labs/2011/ass2/error2.html";
		std::string rerouted_url = "HTTP/1.1 301 Moved Permanently \r\n"
												"Location: " + url + " \r\n"
												"Content-Type: text/html \r\n"
												"Content-Length: 174 \r\n\r\n"
												"<html> \r\n"
												"<head> \r\n"
												"<title>Moved</title> \r\n"
												"</head> \r\n"
												"<body> \r\n"
												"<h1>Moved</h1> \r\n"
												"<p>This page has moved to <a href=" + url +">" + url +
												 "</a>.</p> \r\n"
												"</body> \r\n"
												"</html> \r\n\r\n";
		return rerouted_url.c_str();
	}

	// returns the http request for bad url
	const char* reroute_url(){
		std::string url ="http://www.ida.liu.se/~TDTS04/labs/2011/ass2/error1.html";
		std::string rerouted_url = "HTTP/1.1 301 Moved Permanently \r\n"
												"Location: " + url + " \r\n"
												"Content-Type: text/html \r\n"
												"Content-Length: 174 \r\n\r\n"
												"<html> \r\n"
												"<head> \r\n"
												"<title>Moved</title> \r\n"
												"</head> \r\n"
												"<body> \r\n"
												"<h1>Moved</h1> \r\n"
												"<p>This page has moved to <a href=" + url +">"
												 + url + "</a>.</p> \r\n"
												"</body> \r\n"
												"</html> \r\n\r\n";
		return rerouted_url.c_str();
	}

	// sends the response to client from proxy
	void respond(char* msg){
		int help = 1;
		int sum = 0;
		while (help > 0){
			help = send(connected_sock, &(msg[sum]), sizeof(msg), 0);
			sum += help;
		}
	}

	// Forwards nontext content to client from proxy
	void forward_nontext(char* buffer, int size){
			int left_to_send = size;
			int sent = 0;
			int send_index = 0;
			while(left_to_send>0){
				sent = send(connected_sock, &(buffer[send_index]), size, 0);
				left_to_send -= sent;
				send_index += sent;
			}
	}
}; //End of class internal_side


// External class for communication between proxy and server
class external_side
{
public:
	HTTP_parser *parser; //Parser to manipulate received HTTP's.

	int sock;	//Handle to socket
	int numbytes;
  char buffer[MAXDATASIZE]; //Buffer for incoming messages
  struct addrinfo hints;
  struct addrinfo* resp;
  struct addrinfo* p; //
  int addr_status;
  char s[INET6_ADDRSTRLEN];
  const char*		 hostname_;
  char* port;

  external_side(){
			parser = new HTTP_parser();
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

		addr_status = getaddrinfo(hostname_, port, &hints, &resp);
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

		inet_ntop(p->ai_family, get_in_addr((struct sockaddr*) p->ai_addr),
		 s, sizeof(s));
		fprintf(stderr, "client: connecting to %s\n", s);

		freeaddrinfo(resp); // all done with this structure
	}

	void terminate_connection(){
    	close(sock);
	}

	// Sends the request to server (with keep-alive replaced)
 	int send_request(std::string *request){
		std::string send_request =
		parser->replace_content(*request, "Connection: ", "close");
		send_request = parser->remove_content(send_request, "Accept-Encoding: ");
		int msg_length = send_request.length();

		//hostname_ is used to connect to host in init_socket()
		std::string tmp = parser->get_url_info(*request, "Host: ");
		hostname_ = tmp.c_str();
		init_socket();

		//Send the HTTP message to host
		return send(sock, send_request.c_str(), msg_length, 0);
	}

	// Receives until \r\n\r\n has been received. Returns the length of the
	// message received.
	int receive_header(){
		int received = 0;

		while(std::string(buffer).find("\r\n\r\n") == -1)
				received += recv(sock, &(buffer[received]), MAXDATASIZE-1, 0);

		return received;
  }

	//Forwards to Parser's is_text
  bool is_text(){
		return parser->is_text(std::string(buffer));
	}

	// Uses the recv() function once. The received data gets placed in buffer and
	// returns the number of received bytes.
	int receive_image(){
		return recv(sock, &buffer, MAXDATASIZE-1, 0);
	}

	// returns the received buffer
	char* receive_text(int received_bytes){
		int buf_index = received_bytes;

		while(received_bytes > 0){
			received_bytes = recv(sock, &(buffer[buf_index]), MAXDATASIZE-1, 0);
			buf_index += received_bytes;
		}

		return buffer;
	}
}; // End of class external_side



// Proxy class for communication between client and sever
class proxy
{
	public:
	internal_side* internal;
	external_side* external;
	char* transfer_buffer;

	proxy(){
		internal = new internal_side();
		external = new external_side();
		transfer_buffer = new char[MAXDATASIZE];
	}

	//Inits the internal_side instance with the specified port.
	void init(char* c_port){
		internal->init_socket(c_port);
	}

	// The function that describes the flow of the proxy.
	void run(){
		// The listening process will remain in this call:
		internal->probe_and_fork_connection();

		// Processes with a request from browser will "start" here:
		std::string request = internal->receive_request();

		// Check for bad URL request
		std::string url = external->parser->get_url_info(request, "GET ");
		std::transform(url.begin(), url.end(), url.begin(), ::tolower);

		if(external->parser->inappropriate_content(url)){
			request = internal->reroute_url();
			internal->respond((char*)request.c_str());

			// terminate all conenctions
			external->terminate_connection();
			internal->terminate_connection();
		}

		// Connects the external socket to host and sends the HTTP request
		if(external->send_request(&request) == -1)
			perror("send"); //TODO? Error handling?

		// Check http header if text or image
		int received_bytes = external->receive_header();

		if(external->is_text()){
			transfer_buffer = external->receive_text(received_bytes);
			std::string tmp = std::string(transfer_buffer);
			std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);

			if(external->parser->inappropriate_content(tmp)){
				//inappropriate content, reroute to other website
				transfer_buffer = (char*) internal->reroute_content();
			}

			internal->respond(transfer_buffer);
		} else { //Image
			forward_nontext(received_bytes);
		}
		// End connection to host
		external->terminate_connection();
		// Kill connected processes
		internal->terminate_connection();
	}

	// Forwards nontext messages continuously
	void forward_nontext(int received_bytes){
		int received = 0;
		internal->forward_nontext(external->buffer, received_bytes);

		while((received = external->receive_image()) > 0)	//Header already received
			internal->forward_nontext(external->buffer, received);
	}
}; //End of class Proxy

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
