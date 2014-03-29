int tcp_connect(char* host, int portnum){
	int status, fd=-1;
	char port[10];
	struct addrinfo hints;
	struct addrinfo* list;
	struct addrinfo* list_it;
	
	memset(&hints, 0, sizeof(hints));
	snprintf(port, sizeof(port), "%d", portnum);
	hints.ai_family=AF_UNSPEC;
	hints.ai_socktype=SOCK_STREAM;

	status=getaddrinfo(host, port, &hints, &list);
	if(status!=0){
		fprintf(stderr, "tcp_connect: %s\n", gai_strerror(status));
		return -1;
	}

	//walk list
	for(list_it=list;list_it!=NULL;list_it=list_it->ai_next){
		fd=socket(list_it->ai_family, list_it->ai_socktype, list_it->ai_protocol);
		if(fd>0){
			break;
		}
	}

	if(fd<0){
		perror("comms_open");
		freeaddrinfo(list);
		return -1;
	}


	//FIXME this might need to be in the loop. check it.
	status=connect(fd, list_it->ai_addr, list_it->ai_addrlen);
	if(status<0){
		perror("connect");
		freeaddrinfo(list);
		return -1;
	}

	freeaddrinfo(list);
	return fd;
}

bool comms_open(CONFIG* cfg){
	int status, i;

	status=xsocket_init();
	if(status<0){
		return false;
	}

	for(i=0;i<cfg->connection_count;i++){
		cfg->connections[i].fd=tcp_connect(cfg->connections[i].host, cfg->connections[i].port);
		if(cfg->connections[i].fd<0){
			if(cfg->verbosity>0){
				fprintf(stderr, "Failed to connect to %s on port %d\n", cfg->connections[i].host, cfg->connections[i].port);
			}
			return false;
		}
		if(cfg->verbosity>2){
			fprintf(stderr, "Connection to %s:%d established\n", cfg->connections[i].host, cfg->connections[i].port);
		}
	}

	return true;
}

void comms_close(CONFIG* cfg){
	int i;

	for(i=0;i<cfg->connection_count;i++){
		if(cfg->connections[i].fd>0){
			if(cfg->verbosity>2){
				fprintf(stderr, "Closing connection to %s:%d\n", cfg->connections[i].host, cfg->connections[i].port);
			}
			closesocket(cfg->connections[i].fd);
		}
	}

	xsocket_teardown();
}
