bool db_conn_begin(CONFIG* cfg){
	if(!cfg->db.conn){
		if(cfg->verbosity>2){
			fprintf(stderr, "Connecting to database\n");
		}
		return pq_connect(&(cfg->db));
	}
	return cfg->db.conn!=NULL;
}

void db_conn_end(CONFIG* cfg){
	if(!cfg->db.persist_connection&&cfg->db.conn){
		if(cfg->verbosity>2){
			fprintf(stderr, "Closing non-persistent database connection\n");
		}
		pq_close(&(cfg->db));
	}
}

bool db_buy_snack(CONFIG* cfg, GARFIELD_USER user, CART_ITEM snack){
	static const char* BUY_SINGLE_SNACK="SELECT garfield.snack_buy($1::integer, $2::integer)";
	bool rv=false;

	if(!db_conn_begin(cfg)){
		if(cfg->verbosity>0){
			fprintf(stderr, "Failed to begin database communication\n");
		}
		return false;
	}

	int user_id=htonl(user.account_no);
	int snack_id=htonl(snack.id);

	const char* values[2]={(char*) &user_id, (char*) &snack_id};
	int lengths[2]={sizeof(user_id), sizeof(snack_id)};
	int binary[2]={1,1};

	PGresult* result=PQexecParams(cfg->db.conn, BUY_SINGLE_SNACK, 2, NULL, values, lengths, binary, 0);

	if(result){
		if(PQresultStatus(result)==PGRES_TUPLES_OK){
			if(cfg->verbosity>3){
				fprintf(stderr, "Successfully bought snack %d\n", snack.id);
			}
			rv=true;
		}
		else if(cfg->verbosity>1){
			fprintf(stderr, "Failed to buy snack %d (%s)\n", snack.id, PQresultErrorMessage(result));
		}
		PQclear(result);
	}

	db_conn_end(cfg);
	return rv;
}

GARFIELD_USER db_query_user(CONFIG* cfg, int unixid){
	static const char* QUERY_USER_BY_UNIXID="SELECT print_account_no AS unixid, \
							users.user_id AS accountno, \
							user_name \
						FROM garfield.print_accounts \
						JOIN garfield.users \
							ON print_accounts.user_id=users.user_id \
						WHERE print_account_no=$1::integer";

	static const char* QUERY_ACCOUNT_BY_USERNAME="SELECT user_id FROM garfield.users WHERE user_name = $1";
	GARFIELD_USER rv;
	int uid=unixid;
	rv.unixid=-1;

	if(!db_conn_begin(cfg)){
		if(cfg->verbosity>0){
			fprintf(stderr, "Failed to begin database communication\n");
		}
		return rv;
	}

	if(cfg->verbosity>3){
		fprintf(stderr, "Searching for user with id %d\n", unixid);
	}

	unixid=htonl(unixid);
	const char* values[1]={(char*)&unixid};
	int lengths[1]={sizeof(unixid)};
	int binary[1]={1};

	PGresult* result=PQexecParams(cfg->db.conn, QUERY_USER_BY_UNIXID, 1, NULL, values, lengths, binary, 0);

	if(result){
		if(PQresultStatus(result)==PGRES_TUPLES_OK){
			if(PQntuples(result)==1){
				rv.unixid=strtoul(PQgetvalue(result, 0, 0), NULL, 10);
				rv.account_no=strtoul(PQgetvalue(result, 0, 1), NULL, 10);
				strncpy(rv.name, PQgetvalue(result, 0, 2), MAX_USERNAME_LENGTH);
				if(cfg->verbosity>2){
					fprintf(stderr, "Resolved to user %s\n", rv.name);
				}
			}
			else if(cfg->verbosity>1){
				fprintf(stderr, "Query returned %d matches\n", PQntuples(result));
			}
		}
		else if(cfg->verbosity>0){
			fprintf(stderr, "User query failed\n");
		}
		PQclear(result);
	}

	#ifdef USER_LOOKUP_FALLBACK_ENABLED
		if(rv.unixid<=0){
			struct passwd* info=getpwuid(uid);
			if(info){
				if(cfg->verbosity>3){
					fprintf(stderr, "User lookup fallback found match\n");
				}

				strncpy(rv.name, info->pw_name, MAX_USERNAME_LENGTH);
				PGresult* result=PQexecParams(cfg->db.conn, QUERY_ACCOUNT_BY_USERNAME, 1, NULL, (const char**)&(info->pw_name), NULL, NULL, 0);
				if(result){
					if(PQresultStatus(result)==PGRES_TUPLES_OK){
						if(PQntuples(result)==1){
							rv.unixid=uid;
							rv.account_no=strtoul(PQgetvalue(result, 0, 0), NULL, 10);
							if(cfg->verbosity>2){
								fprintf(stderr, "Fallback resolved user %s\n", rv.name);
							}
						}
						else if(cfg->verbosity>1){
							fprintf(stderr, "Fallback resolved to %d rows\n", PQntuples(result));
						}
					}
					else if(cfg->verbosity>0){
						fprintf(stderr, "Fallback query failed\n");
					}
					PQclear(result);
				}
			}
		}
	#endif

	db_conn_end(cfg);
	return rv;
}

CART_ITEM db_query_item(CONFIG* cfg, char* barcode){
	static const char* QUERY_SNACK="SELECT \
		snacks.snack_id, snack_name, snack_price \
		FROM garfield.snacks JOIN garfield.snacks_available \
		ON snacks_available.snack_id = snacks.snack_id \
		WHERE snack_available AND location_id=1 AND snack_barcode = $1";

	//FIXME FSI hardcoded. ugly
	int i;
	CART_ITEM rv={0, 0.0};

	char* barcode_buffer=NULL;
	
	if(!db_conn_begin(cfg)){
		if(cfg->verbosity>0){
			fprintf(stderr, "Failed to begin database communication\n");
		}
		return rv;
	}

	barcode_buffer=malloc(INPUT_BUFFER_LENGTH);
	if(!barcode_buffer){
		//FIXME error message
		return rv;
	}

	for(i=0;isdigit(barcode[i]);i++){
		barcode_buffer[i]=barcode[i];
	}
	barcode_buffer[i]=0;

	if(cfg->verbosity>3){
		fprintf(stderr, "Searching for barcode %s\n", barcode_buffer);
		//printf("QUERY: %s\n",QUERY_SNACK);
	}

	//query item
	PGresult* result=PQexecParams(cfg->db.conn, QUERY_SNACK, 1, NULL, (const char**)&barcode_buffer, NULL, NULL, 0);

	if(result){
		if(PQresultStatus(result)==PGRES_TUPLES_OK){
			if(PQntuples(result)==1){
				rv.id=strtoul(PQgetvalue(result, 0, 0), NULL, 10);
				rv.price=strtof(PQgetvalue(result, 0, 2), NULL);	
				if(cfg->verbosity>3){
					fprintf(stderr, "Queried snack: %s\n", PQgetvalue(result, 0, 1));
				}
			}
			else if(cfg->verbosity>1){
				fprintf(stderr, "Query returned %d matches\n", PQntuples(result));
			}
		}
		PQclear(result);
	}

	free(barcode_buffer);
	db_conn_end(cfg);
	return rv;
}
