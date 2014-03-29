#include "tokenizer.c"
#include "statemachine.c"

int garfield_pos(CONFIG* cfg){
	struct timeval tv;
	fd_set readfds;
	int fd_max, i, c, e,  error, bytes, offset=0, active_token=0, head_offset;
	INPUT_TOKEN token;
	TRANSITION_RESULT trans;

	//set up initial state
	POS.state=STATE_IDLE;
	POS.cart=NULL;
	POS.items_allocated=0;
	POS.items=0;
	POS.shutdown=false;

	memset(INPUT.data, 0, sizeof(INPUT.data));
	INPUT.parse_head=INPUT.data;

	//FIXME handle output via tcp
	printf("\f>GarfieldPOS v%s\n",VERSION);
	portable_sleep(1000);
	state_enter(POS.state);

	while(!POS.shutdown){	
		tv.tv_sec=10;
		tv.tv_usec=0;

		//select over i/o descriptors
		fd_max=-1;
		FD_ZERO(&readfds);
		for(i=0;i<cfg->connection_count;i++){
			if(cfg->connections[i].fd>0){
				FD_SET(cfg->connections[i].fd, &readfds);
				if(cfg->connections[i].fd>fd_max){
					fd_max=cfg->connections[i].fd;
				}
			}
		}

		error=select(fd_max+1, &readfds, NULL, NULL, &tv);
		if(error<0){
			perror("select");
			return -1;
		}

		//process input
		if(cfg->verbosity>3){
			fprintf(stderr, "Input ready from %d descriptors\n", error);
		}

		for(i=0;i<cfg->connection_count;i++){
			if(cfg->connections[i].fd>0&&FD_ISSET(cfg->connections[i].fd, &readfds)){
				bytes=recv(cfg->connections[i].fd, INPUT.data+offset, sizeof(INPUT.data)-offset, 0);
				
				if(bytes<0){
					perror("read_tcp");
				}

				if(bytes==0){
					if(cfg->verbosity>2){
						fprintf(stderr, "Connection %d lost, reconnecting with next iteration\n", i);
					}
					cfg->connections[i].fd=-1;
					continue;
				}

				//terminate
				INPUT.data[offset+bytes]=0;

				if(cfg->verbosity>2){
					fprintf(stderr, "%d bytes of data from conn %d\n", bytes, i);
				}

				if(cfg->verbosity>3){
					fprintf(stderr, "Current buffer: \"%s\"\n", INPUT.data);
				}
				
				INPUT.parse_head=INPUT.data;
				token=TOKEN_INVALID;
				c=active_token;

				while(INPUT.data[c]!=0){
					//recognize token
					token=tok_read(INPUT.data+c);

					if(token==TOKEN_INCOMPLETE){
						break;
					}

					//do transition
					trans=transition(POS.state, token, cfg);
					c+=tok_length(token);

					if(cfg->verbosity>3){
						fprintf(stderr, "(%s | %s) => %s %s\n", state_dbg_string(POS.state), tok_dbg_string(token), state_dbg_string(trans.state), action_dbg_string(trans.action));	

					}

					//display output	
					if(POS.state!=trans.state||trans.force_redisplay){
						state_enter(trans.state);
					}
					fflush(stdout);

					//apply results
					POS.state=trans.state;
					switch(trans.action){
						case TOKEN_KEEP:
							//do not move parse_head
							break;
						case TOKEN_DISCARD:
							//remove the last token
							for(e=0;INPUT.data[c+e]!=0;e++){
								INPUT.data[c-tok_length(token)+e]=INPUT.data[c+e];
							}
							INPUT.data[c-tok_length(token)+e]=0;
							c-=tok_length(token);
							break;
						case TOKEN_CONSUME:
							//consume up to and including this token
							INPUT.parse_head=INPUT.data+c;
							break;
						case TOKEN_REMOVE:
							//get offset of this and the last token
							e=tok_last_offset_from(INPUT.data, c-tok_length(token));
							if(e<0){
								e=0;
							}
							//if token to be deleted is not NUMERAL or BACKSPACE, do not delete
							if(tok_read(INPUT.data+e)!=TOKEN_NUMERAL&&tok_read(INPUT.data+e)!=TOKEN_BACKSPACE){
								e+=tok_length(tok_read(INPUT.data+e));
							}
							//copy remaining buffer over deleted tokens //FIXME broken
							for(;INPUT.data[c]!=0;e++){
								INPUT.data[e]=INPUT.data[c];
							}
							INPUT.data[e]=0;
							c=e;
							break;
					}
				}

				//shift parse_head to 0
				if(INPUT.parse_head!=INPUT.data){
					head_offset=INPUT.parse_head-INPUT.data;
					active_token=c-head_offset;
					for(c=0;INPUT.parse_head[c]!=0;c++){
						INPUT.data[c]=INPUT.parse_head[c];
					}
					INPUT.data[c]=0;
					offset=c;
				}
				else{
					head_offset=0;
					offset=strlen(INPUT.data);
					active_token=c;
				}
				
				if(cfg->verbosity>3){
					fprintf(stderr, "parse_head offset is %d, active token is %d\n",head_offset, active_token);
					fprintf(stderr, "Buffer after processing: \"%s\"\n", INPUT.data);
					fprintf(stderr, "Current input offset is %d\n", offset);
				}
			}
			else if(cfg->connections[i].fd<0){
				//reconnect lost connection
				if(cfg->verbosity>2){
					fprintf(stderr, "Reconnecting connection %d\n",i);
				}
				cfg->connections[i].fd=tcp_connect(cfg->connections[i].host, cfg->connections[i].port);
			}
		}
	}

	return 0;
}
