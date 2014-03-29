bool pq_connect(DATABASE* cfg){
	char port[10];
	snprintf(port, sizeof(port), "%d", cfg->port);
	char const* keywords[]={"host", "port", "dbname", "user", "sslmode", (cfg->pass)?"password":NULL,  NULL};
	char* values[]={cfg->server, port, cfg->db_name, cfg->user, PGRES_SSLMODE, cfg->pass, NULL};

	cfg->conn=PQconnectdbParams(keywords, (char const**)values, 0);

	if(!cfg->conn){
		fprintf(stderr, "Failed to allocate memory for connection\n");
		return false;
	}

	if(PQstatus(cfg->conn)!=CONNECTION_OK){
		fprintf(stderr, "Failed to connect to database server: %s\n", PQerrorMessage(cfg->conn));
		PQfinish(cfg->conn);
		return false;
	}

	return true;
}

void pq_close(DATABASE* db){
	if(db->conn){
		PQfinish(db->conn);
		db->conn=NULL;
	}
}
