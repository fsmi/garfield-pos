bool db_conn_begin(CONFIG* cfg){
	if(!cfg->db.conn){
		if(cfg->verbosity>2){
			fprintf(stderr, "Connecting to database\n");
		}
		return pq_connect(&(cfg->db));
	}

	switch(PQstatus(cfg->db.conn)){
		case CONNECTION_OK:
			return true;
		case CONNECTION_BAD:
			printf("\r\nDatabase disconnect\n");
			portable_sleep(1000);
			printf("\fReconnecting....");
			if(pq_connect(&(cfg->db))){
				portable_sleep(1000);
				printf("\f");
				return true;
			}
			else{
				portable_sleep(1000);
				printf("\f");
				return false;
			}
		default:
			if(cfg->verbosity>0){
				fprintf(stderr, "Invalid database status returned\n");
			}
	}
	return false;
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

bool db_delta_transaction(CONFIG* cfg, GARFIELD_USER user, double delta){
	static const char* INSERT_DELTA_TRANSACTION="SELECT garfield.delta_transaction_create($1::integer, $2::real)";
	char delta_str[20];
	bool rv=false;

	if(!db_conn_begin(cfg)){
		if(cfg->verbosity>0){
			fprintf(stderr, "Failed to begin database communication\n");
		}
		return false;
	}
	
	int user_id=htonl(user.account_no);
	snprintf(delta_str, sizeof(delta_str)-1, "%.2f", delta);

	const char* values[2]={(char*) &user_id, delta_str};
	int lengths[2]={sizeof(user_id), strlen(delta_str)};
	int binary[2]={1,0};

	PGresult* result=PQexecParams(cfg->db.conn, INSERT_DELTA_TRANSACTION, 2, NULL, values, lengths, binary, 0);
	if(!result){
		if(cfg->verbosity>0){
			fprintf(stderr, "Database failed hard at db_delta_transaction\n");
		}
		return false;
	}
	if(PQresultStatus(result)==PGRES_TUPLES_OK){
		switch(PQgetvalue(result, 0, 0)[0]){
			case 't':
				rv=true;
				break;
			default:
				rv=false;
				break;
		}
	}
	else if(cfg->verbosity>1){
		fprintf(stderr, "Failed to create delta transaction for user %d, delta %f (%s)\n", user.account_no, delta, PQresultErrorMessage(result));
		rv=false;
	}
	PQclear(result);
	return rv;
}

GARFIELD_USER db_query_user(CONFIG* cfg, int unixid){
	static const char* QUERY_USER_BY_UNIXID="SELECT print_account_no AS unixid, \
							users.user_id AS accountno, \
							user_name, \
							balances.balance \
						FROM garfield.print_accounts \
						JOIN garfield.users \
							ON print_accounts.user_id=users.user_id \
						JOIN garfield.balances \
							ON balances.user=users.user_id \
						WHERE print_account_no=$1::integer";

	GARFIELD_USER rv = {
		.unixid = -1,
		.account_no = -1,
		.name = ""
	};

	#ifdef USER_LOOKUP_FALLBACK_ENABLED
		static const char* QUERY_ACCOUNT_BY_USERNAME="SELECT user_id FROM garfield.users WHERE user_name = $1";
		int uid=unixid;
	#endif

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
				rv.balance=strtod(PQgetvalue(result, 0, 3), NULL);
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
		WHERE snack_available AND location_id=$2 AND snack_barcode = $1";

	int i;
	CART_ITEM rv = {0, 0.0};
	char* barcode_buffer = NULL;
	char* query_params[2];

	if(!db_conn_begin(cfg)){
		if(cfg->verbosity > 0){
			fprintf(stderr, "Failed to begin database communication\n");
		}
		return rv;
	}

	barcode_buffer = calloc(INPUT_BUFFER_LENGTH, sizeof(char));
	if(!barcode_buffer){
		//FIXME error message
		return rv;
	}

	for(i = 0; isdigit(barcode[i]) && i < INPUT_BUFFER_LENGTH; i++){
		barcode_buffer[i] = barcode[i];
	}

	if(cfg->verbosity > 3){
		fprintf(stderr, "Searching for barcode %s\n", barcode_buffer);
		//printf("QUERY: %s\n",QUERY_SNACK);
	}

	//prepare query parameters
	query_params[0] = barcode_buffer;
	query_params[1] = cfg->location;

	//query item
	PGresult* result = PQexecParams(cfg->db.conn, QUERY_SNACK, 2, NULL, (const char**)query_params, NULL, NULL, 0);

	if(result){
		if(PQresultStatus(result) == PGRES_TUPLES_OK){
			if(PQntuples(result) == 1){
				rv.id = strtoul(PQgetvalue(result, 0, 0), NULL, 10);
				rv.price = strtof(PQgetvalue(result, 0, 2), NULL);
				if(cfg->verbosity > 3){
					fprintf(stderr, "Queried snack: %s\n", PQgetvalue(result, 0, 1));
				}
			}
			else if(cfg->verbosity > 1){
				fprintf(stderr, "Query returned %d matches\n", PQntuples(result));
			}
		}
		PQclear(result);
	}

	free(barcode_buffer);
	db_conn_end(cfg);
	return rv;
}
