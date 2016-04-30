#define VERSION "0.3-kif"
#define MAX_CFGLINE_LENGTH 1024
#define MAX_PASSWORD_LENGTH 128
#define PGRES_SSLMODE "require"
#define INPUT_BUFFER_LENGTH 128
#define MAX_USERNAME_LENGTH 128
//#define USER_LOOKUP_FALLBACK_ENABLED
#define DEFAULT_GARFIELD_LOCATION "1"

#ifdef USER_LOOKUP_FALLBACK_ENABLED
	#include <sys/types.h>
	#include <pwd.h>
#endif

typedef enum /*_CONNECTION_TYPE*/ {
	CONN_INPUT,
	CONN_OUTPUT
} CONNECTION_TYPE;

typedef enum /*_LOGIC_STATE*/ {
	STATE_IDLE,
	STATE_BARCODE,
	STATE_PLU,
	STATE_DISPLAY,
	STATE_ADD,
	STATE_STORNO,
	STATE_PAY,
	STATE_DEBUG,
	STATE_CREDIT
} POS_STATE;

typedef enum /*_INPUT_TOKEN*/ {
	TOKEN_INCOMPLETE,
	TOKEN_INVALID,
	TOKEN_PAY,
	TOKEN_PLU,
	TOKEN_STORNO,
	TOKEN_CANCEL,
	TOKEN_ENTER,
	TOKEN_BACKSPACE,
	TOKEN_NUMERAL,
	TOKEN_COMMA,
	TOKEN_MINUS,
	TOKEN_ADD
} INPUT_TOKEN;

typedef enum /*_TOKEN_ACTION*/ {
	TOKEN_KEEP,	//keep token in parse buffer
	TOKEN_DISCARD,	//discard the token
	TOKEN_REMOVE,	//remove this and the previous token
	TOKEN_CONSUME	//discard all tokens up to and including this one
} TOKEN_ACTION;

typedef struct /*_TRANSITION_RESULT*/ {
	POS_STATE state;
	TOKEN_ACTION action;
	bool force_redisplay;
} TRANSITION_RESULT;

typedef struct /*_CART_SNACK_ITEM*/ {
	int id;
	double price;
} CART_ITEM;

typedef struct /*_GARFIELD_USER*/ {
	int unixid;
	int account_no;
	double balance;
	char name[MAX_USERNAME_LENGTH+1];
} GARFIELD_USER;

typedef struct /*_CONNECTION_SPEC*/ {
	char* host;
	uint16_t port;
	CONNECTION_TYPE type;
	int fd;
} CONNECTION;

typedef struct /*_DB_SPEC*/ {
	char* server;
	uint16_t port;
	char* user;
	char* pass;
	char* db_name;
	bool persist_connection;
	bool use_pgpass;
	PGconn* conn;
} DATABASE;

typedef struct /*_CONFIG_PARAMS*/ {
	char* cfg_file;
	int verbosity;
	int connection_count;
	char* location;
	CONNECTION* connections;
	DATABASE db;
} CONFIG;

struct {
	POS_STATE state;
	CART_ITEM* cart;
	int items_allocated;
	int items;
	bool shutdown;
	unsigned transactions;
	unsigned sold_items;
	double sales_volume;
} POS;

struct {
	char data[INPUT_BUFFER_LENGTH];
	char* parse_head;
	char* active_token;
} INPUT;
