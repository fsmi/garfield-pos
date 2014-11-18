#include "tokenizer.c"
#include "statemachine.c"

int garfield_pos(CONFIG* cfg){
	struct timeval tv;
	fd_set readfds;
	int fd_max, i, c, error, bytes, read_offset=0, active_token=0, head_offset;
	int conn_it, scan_head;
	INPUT_TOKEN token;
	TRANSITION_RESULT trans;

	//set up initial state
	POS.state=STATE_IDLE;
	POS.cart=NULL;
	POS.items_allocated=0;
	POS.items=0;
	POS.shutdown=false;
	POS.transactions=0;
	POS.sold_items=0;
	POS.sales_volume=0.0;

	memset(INPUT.data, 0, sizeof(INPUT.data));
	INPUT.parse_head=INPUT.data;

	//FIXME handle output via tcp
	printf("\fGarfieldPOS v%s\n",VERSION);
	portable_sleep(1000);
	state_enter(POS.state);

	while(!POS.shutdown){	
		tv.tv_sec=10;
		tv.tv_usec=0;

		//select over i/o descriptors
		fd_max=-1;
		FD_ZERO(&readfds);
		for(conn_it=0;conn_it<cfg->connection_count;conn_it++){
			if(cfg->connections[conn_it].fd>0){
				FD_SET(cfg->connections[conn_it].fd, &readfds);
				if(cfg->connections[conn_it].fd>fd_max){
					fd_max=cfg->connections[conn_it].fd;
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
			fprintf(stderr, "Input ready from %d descriptors, read_offset %d\n", error, read_offset);
		}

		for(conn_it=0;conn_it<cfg->connection_count;conn_it++){
			if(cfg->connections[conn_it].fd>0&&FD_ISSET(cfg->connections[conn_it].fd, &readfds)){
				bytes=recv(cfg->connections[conn_it].fd, INPUT.data+read_offset, sizeof(INPUT.data)-read_offset-1, 0);
				
				if(cfg->verbosity>3){
					fprintf(stderr, "Buffer length is %lu, sentinel byte %02x, first byte %02x\n", sizeof(INPUT.data), INPUT.data[sizeof(INPUT.data)-1], INPUT.data[0]);
				}

				if(bytes<0){
					perror("read_tcp");
				}

				if(bytes==0&&read_offset<(sizeof(INPUT.data)-1)){
					if(cfg->verbosity>2){
						fprintf(stderr, "Connection %d lost, reconnecting with next iteration\n", conn_it);
					}
					cfg->connections[conn_it].fd=-1;
					continue;
				}

				//terminate
				INPUT.data[read_offset+bytes]=0;

				if(cfg->verbosity>2){
					fprintf(stderr, "%d bytes of data from conn %d\n", bytes, conn_it);
				}

				if(cfg->verbosity>3){
					fprintf(stderr, "Current buffer: \"%s\"\n", INPUT.data);
					fprintf(stderr, "active_token is at %d, begins with %02x\n", active_token, INPUT.data[active_token]);
				}
				
				INPUT.parse_head=INPUT.data;
				token=TOKEN_INVALID;
				scan_head=active_token;

				//FIXME this condition might be problematic with the buffer fixup below
				while(INPUT.data[scan_head]!=0){
					//recognize token
					token=tok_read(INPUT.data+scan_head);

					if(token==TOKEN_INCOMPLETE){
						break;
					}

					//do transition
					INPUT.active_token=INPUT.data+scan_head;
					trans=transition(POS.state, token, cfg);
					scan_head+=tok_length(token);

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
							for(i=0;INPUT.data[scan_head+i]!=0;i++){
								INPUT.data[scan_head+i-tok_length(token)]=INPUT.data[scan_head+i];
							}
							INPUT.data[scan_head+i-tok_length(token)]=0;
							scan_head-=tok_length(token);
							break;
						case TOKEN_CONSUME:
							//consume up to and including this token
							INPUT.parse_head=INPUT.data+scan_head;
							break;
						case TOKEN_REMOVE:
							//get offset of this and the last token
							i=tok_last_offset_from(INPUT.data, scan_head-tok_length(token));
							if(i<0){
								i=0;
							}
							//if token to be deleted is not NUMERAL or BACKSPACE, do not delete
							//FIXME should also delete TOKEN_COMMA and TOKEN_MINUS
							if(tok_read(INPUT.data+i)!=TOKEN_NUMERAL&&tok_read(INPUT.data+i)!=TOKEN_BACKSPACE){
								i+=tok_length(tok_read(INPUT.data+i));
							}
							//copy remaining buffer over deleted tokens
							for(c=0;INPUT.data[scan_head+c]!=0;c++){
								INPUT.data[i+c]=INPUT.data[scan_head+c];
							}
							INPUT.data[i]=0;
							scan_head=i;
							break;
					}
				}

				//fix input buffer overrun
				if(active_token==scan_head&&token!=TOKEN_INCOMPLETE&&trans.action!=TOKEN_DISCARD){
					//FIXME this breaks with some TOKEN_BACKSPACE actions 
					if(cfg->verbosity>3){
						fprintf(stderr, "Resetting buffer\n");
					}
					INPUT.parse_head+=active_token;
				}

				//shift parse_head to 0
				if(INPUT.parse_head!=INPUT.data){
					head_offset=INPUT.parse_head-INPUT.data;
					active_token=scan_head-head_offset;
					for(scan_head=0;INPUT.parse_head[scan_head]!=0;scan_head++){
						INPUT.data[scan_head]=INPUT.parse_head[scan_head];
					}
					INPUT.data[scan_head]=0;
					read_offset=scan_head;
				}
				else{
					head_offset=0;
					read_offset=strlen(INPUT.data);
					active_token=scan_head;
				}
				
				if(cfg->verbosity>3){
					fprintf(stderr, "parse_head offset is %d, active token is %d\n",head_offset, active_token);
					fprintf(stderr, "Buffer after processing: \"%s\"\n", INPUT.data);
					fprintf(stderr, "Current read offset is %d\n", read_offset);
				}
			}
			else if(cfg->connections[conn_it].fd<0){
				//reconnect lost connection
				if(cfg->verbosity>2){
					fprintf(stderr, "Reconnecting connection %d\n",conn_it);
				}
				cfg->connections[conn_it].fd=tcp_connect(cfg->connections[conn_it].host, cfg->connections[conn_it].port);
			}
		}
	}

	return 0;
}
