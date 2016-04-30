garfield-pos core
-----------------
This folder contains the garfield-pos core implementation,
parsing input from the multiple kbserver instances and processing
them into sales transactions via a state machine.

Input is read from all defined sources into a parse buffer,
after which that buffer is tokenized and the state machine transition
function called once for every recognized token.
Incomplete tokens are recognized if they appear at the end of the parse
buffer, accounting for segmented transmission over the network.
Unrecognized parts of the parse buffer are ignored and deleted.
For more information, see logic.c

The transition result consists of the result state and an action to
perform on the token buffer. Further information may be found in
statemachine.c

Communication with the backing postgres server is done via prepared
queries, mainly calling stored procedures on the database.
More information may be found in database.c

Recognized Tokens
-----------------
In its basic form, garfield-pos recognizes the following tokens as
valid inputs to the state machine (in kbserver stringspec format
where applicable)

	'PAY' 13 10		as TOKEN_PAY
	'PLU' 13 10		as TOKEN_PLU
	'STORNO' 13 10		as TOKEN_STORNO
	'CANCEL' 13 10		as TOKEN_CANCEL
	'ENTER' 13 10		as TOKEN_ENTER
	'BACKSPACE'		as TOKEN_BACKSPACE
	'ADD' 13 10		as TOKEN_ADD
	<DIGITS 0-9>		as TOKEN_NUMERAL
	
Extending the system
--------------------
Adding tokens requires adding an identifier to the
INPUT_TOKEN enum in garfield-pos.h as well as adding the new
token and its input stream representation to the functions
	tok_read
	tok_length
	tok_dbg_string
in tokenizer.c

Introducing additional states to the state machine requires
adding identifiers to the POS_STATE enum in garfield-pos.h
and corresponding modifications to statemachine.c, consisting
of declaring a transition function for the new states,
adding that to the router in the main transition function,
adding an initial text display in the state_enter function if 
required and adding the new states to the state_dbg_string function.

When implementing new states, special attention should be paid 
to the tokens TOKEN_CANCEL and TOKEN_BACKSPACE. 
TOKEN_CANCEL should always take the user to a recognizably 'safe' 
state, while TOKEN_BACKSPACE should be the only token returning 
an action of TOKEN_REMOVE.

States may examine but not directly modify the parse buffer
using the global INPUT structure. The supported method of
operating on the parse buffer is using the action member
of the TRANSITION_RESULT structure.

Command-line options
--------------------
The main executable parses its commandline with regard to the
following parameters

	-f <configfile>			Configuration file to use
	-h				Print help and exit
	-v[v[v[v]]]			Increase verbosity

All other configuration is provided in a configuration file.

Configuration file format
-------------------------
The confiuration file is read line by line.
Lines beginning with '#' are designated comments and are ignored
by the parser.

Valid directives are as follows

	input <host> <port>		Connect to an input server
	db_server <host> <port>		Specify postgres database server
	db_user	<username>		Specify database user
	db_name	<dbname>		Specify database to use
	db_persist <true|false>		Keep the database connection alive
	use_pgpass <true|false>		Read database password from ~/.pgpass

Caveats
-------
Note that the database schema is always "garfield".
The table garfield.print_accounts is unfortunately named.
It simply contains a mapping from user identification numbers
(the ones which are typed in) to the internal database user IDs.

Build prerequisites
-------------------
	- a C compiler (tcc should do)
	- PostgreSQL client header files (libpq-dev)
	- make (not a hard requirement)

The core should be able to build on windows when not defining 
	USER_LOOKUP_FALLBACK_ENABLED
in garfield-pos.h.
This disables a fallback method of resolving user identification codes to user 
names that only works on unices.
