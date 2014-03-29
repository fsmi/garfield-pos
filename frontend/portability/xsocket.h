#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	
	#ifndef SIGPIPE
		#define SIGPIPE 13
	#endif
#else
	#include <unistd.h> //close, select
	#define closesocket close
	#include <sys/types.h> //socket
	#include <sys/socket.h> //socket
	#include <netdb.h> //getaddrinfo
#endif

int inline xsocket_init(){
	#ifdef _WIN32
		WSADATA wsaData;
		if(WSAStartup(MAKEWORD(2,0),&wsaData)!=0){
			return -1;
		}
	#endif
	return 0;
}

void inline xsocket_teardown(){
	#ifdef _WIN32
		WSACleanup();
	#endif
}

//Use the closesocket call to close sockets
//Select only on socket descriptors