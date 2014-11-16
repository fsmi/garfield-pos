#include "database.c"
#include "cart.c"
#include <math.h>

TRANSITION_RESULT state_idle(INPUT_TOKEN token, CONFIG* cfg){
	TRANSITION_RESULT res={STATE_IDLE, TOKEN_CONSUME, false};

	switch(token){
		case TOKEN_PLU:
			res.state=STATE_PLU;
			break;
		case TOKEN_NUMERAL:
			res.state=STATE_BARCODE;
			res.action=TOKEN_KEEP;
			break;
		case TOKEN_BACKSPACE:
			res.state=STATE_DEBUG;
			break;
		case TOKEN_PAY:
			res.state=STATE_CREDIT;
		default:
			return res;
	}
	return res;
}

TRANSITION_RESULT state_barcode(INPUT_TOKEN token, CONFIG* cfg){
	TRANSITION_RESULT res={STATE_BARCODE, TOKEN_DISCARD, false};
	CART_ITEM item;

	switch(token){
		case TOKEN_BACKSPACE:
			res.action=TOKEN_REMOVE;
			break;
		case TOKEN_NUMERAL:
			res.action=TOKEN_KEEP;
			break;
		case TOKEN_ENTER:
			//resolve snack, add to cart
			item=db_query_item(cfg, INPUT.parse_head);
			if(cfg->verbosity>2){
				fprintf(stderr, "Returned item ID: %d, Price: %f\n", item.id, item.price);
			}
			
			if(item.id<=0){
				printf("Not recognized\n");
				portable_sleep(1000);
			}

			cart_store(item, cfg);
			//no break here
		case TOKEN_CANCEL:
			res.action=TOKEN_CONSUME;
			res.state=STATE_DISPLAY;
			break;
		default:
			return res;
	}
	return res;
}

TRANSITION_RESULT state_plu(INPUT_TOKEN token, CONFIG* cfg){
	TRANSITION_RESULT res={STATE_PLU, TOKEN_DISCARD, false};
	CART_ITEM item;

	switch(token){
		case TOKEN_BACKSPACE:
			printf("\b \b");
			res.action=TOKEN_REMOVE;
			break;
		case TOKEN_NUMERAL:
			printf("%c",INPUT.active_token[0]);
			res.action=TOKEN_KEEP;
			break;
		case TOKEN_ENTER:
			item=db_query_item(cfg, INPUT.parse_head);
			if(cfg->verbosity>2){
				fprintf(stderr, "Returned item ID: %d, Price: %f\n", item.id, item.price);
			}
			
			if(item.id<=0){
				printf("\r\nNot recognized\n");
				portable_sleep(1000);
			}

			cart_store(item, cfg);
			
			///no break here
		case TOKEN_CANCEL:
			res.action=TOKEN_CONSUME;
			res.state=STATE_DISPLAY;
			break;
		default:
			return res;
	}
	return res;
}

TRANSITION_RESULT state_display(INPUT_TOKEN token, CONFIG* cfg){
	TRANSITION_RESULT res={STATE_DISPLAY, TOKEN_CONSUME, false};
	CART_ITEM item;

	switch(token){
		case TOKEN_NUMERAL:
			res.state=STATE_BARCODE;
			res.action=TOKEN_KEEP;
			break;
		case TOKEN_PLU:
			res.state=STATE_PLU;
			break;
		case TOKEN_CANCEL:
			POS.items=0;
			res.state=STATE_IDLE;
			break;
		case TOKEN_STORNO:
			res.state=STATE_STORNO;
			break;
		case TOKEN_PAY:
			res.state=STATE_PAY;
			break;
		case TOKEN_ADD:
			if(POS.items>0){
				item=POS.cart[POS.items-1];
				cart_store(item, cfg);
				res.force_redisplay=true;
			}
			else if(cfg->verbosity>2){
				fprintf(stderr, "No item to be duplicated\n");
			}
			break;
		default:
			return res;
	}
	return res;
}

TRANSITION_RESULT state_storno(INPUT_TOKEN token, CONFIG* cfg){
	TRANSITION_RESULT res={STATE_STORNO, TOKEN_DISCARD, false};
	int i;
	CART_ITEM item;

	switch(token){
		case TOKEN_NUMERAL:
			printf("%c",INPUT.active_token[0]);
			res.action=TOKEN_KEEP;
			break;
		case TOKEN_BACKSPACE:
			printf("\b \b");
			res.action=TOKEN_REMOVE;
			break;
		case TOKEN_STORNO:
			//remove last item from cart
			if(POS.items>0){
				POS.items--;
			}
			res.state=STATE_DISPLAY;
			res.action=TOKEN_CONSUME;
			break;
		case TOKEN_ENTER:
			//resolve snack
			item=db_query_item(cfg, INPUT.parse_head);
			if(cfg->verbosity>2){
				fprintf(stderr, "Returned item ID: %d, Price: %f\n", item.id, item.price);
			}
			
			if(item.id<=0){
				printf("\r\nNot recognized\n");
				portable_sleep(1000);
			}
			else{
				for(i=0;i<POS.items;i++){
					if(POS.cart[i].id==item.id){
						if(cfg->verbosity>3){
							fprintf(stderr, "Item to be removed found at cart position %d\n", i);
						}
						for(;i<POS.items-1;i++){
							POS.cart[i]=POS.cart[i+1];
						}
						POS.items--;
						break;
					}
				}
			}

			//no break here
		case TOKEN_CANCEL:
			res.action=TOKEN_CONSUME;
			res.state=STATE_DISPLAY;
			break;
		default:
			return res;
	}
	return res;
}

TRANSITION_RESULT state_pay(INPUT_TOKEN token, CONFIG* cfg){
	TRANSITION_RESULT res={STATE_PAY, TOKEN_DISCARD, false};
	int i;
	GARFIELD_USER user;

	switch(token){
		case TOKEN_NUMERAL:
			printf("%c",INPUT.active_token[0]);
			res.action=TOKEN_KEEP;
			break;
		case TOKEN_BACKSPACE:
			printf("\b \b");
			res.action=TOKEN_REMOVE;
			break;
		case TOKEN_ENTER:
			printf("\n");
			user.unixid=strtoul(INPUT.parse_head, NULL, 10);
			user=db_query_user(cfg, user.unixid);

			if(user.unixid<=0){
				printf("Not recognized\n");
				portable_sleep(1000);
				res.state=STATE_DISPLAY;
				res.action=TOKEN_CONSUME;
				break;
			}

			//buy snacks
			for(i=0;i<POS.items;i++){
				if(!db_buy_snack(cfg, user, POS.cart[i])){
					if(cfg->verbosity>1){
						fprintf(stderr, "Failed to buy snack %d\n", POS.cart[i].id);
					}
				}
				else{
					POS.sold_items++;
					POS.sales_volume+=POS.cart[i].price;
				}
			}

			printf("%s    -%.2f\n", user.name, cart_get_total());
			portable_sleep(1000);

			POS.items=0;
			POS.transactions++;
			if(cfg->verbosity>2){
				fprintf(stderr, "Processed %d transactions, sold %d items\n", POS.transactions, POS.sold_items);
			}

			res.state=STATE_IDLE;
			res.action=TOKEN_CONSUME;
			break;
		case TOKEN_CANCEL:
			res.action=TOKEN_CONSUME;
			res.state=STATE_DISPLAY;
			break;
		default:
			return res;
	}
	return res;
}

TRANSITION_RESULT state_credit(INPUT_TOKEN token, CONFIG* cfg){
	TRANSITION_RESULT res={STATE_CREDIT, TOKEN_DISCARD, false};
	char* scan_head;
	int enters_read=0;
	double delta;
	INPUT_TOKEN scan_token=TOKEN_INCOMPLETE;
	GARFIELD_USER user;
	
	switch(token){
		case TOKEN_COMMA:
		case TOKEN_MINUS:
		case TOKEN_NUMERAL:
			printf("%c",INPUT.active_token[0]);
			res.action=TOKEN_KEEP;
			break;
		case TOKEN_ENTER:
			//scan input buffer for enter tokens
			for(scan_head=INPUT.parse_head;scan_head<INPUT.active_token;scan_head+=tok_length(scan_token)){
				scan_token=tok_read(scan_head);
				if(scan_token==TOKEN_ENTER){
					enters_read++;
				}
			}

			//read uid
			user.unixid=strtoul(INPUT.parse_head, NULL, 10);
					
			//query info
			user=db_query_user(cfg, user.unixid);

			if(user.unixid<=0){
				printf("Not recognized\n");
				portable_sleep(1000);
				res.state=STATE_IDLE;
				res.action=TOKEN_CONSUME;
				break;
			}

			switch(enters_read){
				case 0:
					//print
					printf(" @%6.02f %c\r\nDelta: ", fabs(user.balance), (user.balance>0)?'H':'S');

					res.action=TOKEN_KEEP;
					break;
				case 1:
					//read delta
					for(scan_head=INPUT.parse_head;scan_head<INPUT.active_token;scan_head+=tok_length(scan_token)){
						scan_token=tok_read(scan_head);
						if(scan_token==TOKEN_ENTER){
							scan_head+=tok_length(scan_token);
							break;
						}
					}
				
					//submit transaction
					delta=strtod(scan_head, NULL);
					if(cfg->verbosity>2){
						fprintf(stderr, "Transaction delta is %f\n", delta);
					}
					
					if(fabs(delta)>0.01){
						if(db_delta_transaction(cfg, user, delta)){
							printf("\rTransaction cleared\n");
						}
						else{
							printf("\rTransaction failed\n");
						}
					}
					else{
						printf("\rNothing changed\n");
					}
					portable_sleep(1000);	

					res.action=TOKEN_CONSUME;
					res.state=STATE_IDLE;
					break;
				default:
					if(cfg->verbosity>1){
						fprintf(stderr, "Invalid sequence in STATE_CREDIT\n");
					}
					res.state=STATE_IDLE;
					res.action=TOKEN_CONSUME;
					break;
			}
			break;
		case TOKEN_BACKSPACE:
			//pay total (hacky)
			if(db_delta_transaction(cfg, user, -user.balance)){
				printf("\rAccount paid\n");
			}
			else{
				printf("\rTransaction failed\n");
			}
			portable_sleep(1000);	

			res.action=TOKEN_CONSUME;
			res.state=STATE_IDLE;
			break;
		case TOKEN_CANCEL:
			res.action=TOKEN_CONSUME;
			res.state=STATE_IDLE;
			break;
		default:
			return res;
	}
	return res;
}

TRANSITION_RESULT state_debug(INPUT_TOKEN token, CONFIG* cfg){
	TRANSITION_RESULT res={STATE_IDLE, TOKEN_CONSUME, false};
	
	return res;
}

TRANSITION_RESULT transition(POS_STATE state, INPUT_TOKEN token, CONFIG* cfg){
	switch(state){
		case STATE_IDLE:
			return state_idle(token, cfg);
		case STATE_BARCODE:
			return state_barcode(token, cfg);
		case STATE_PLU:
			return state_plu(token, cfg);
		case STATE_DISPLAY:
			return state_display(token, cfg);
		case STATE_STORNO:
			return state_storno(token, cfg);
		case STATE_PAY:
			return state_pay(token, cfg);
		case STATE_DEBUG:
			return state_debug(token, cfg);
		case STATE_CREDIT:
			return state_credit(token, cfg);
	}
	TRANSITION_RESULT def={STATE_IDLE, TOKEN_DISCARD, false};
	return def;
}

void state_enter(POS_STATE s){
	switch(s){
		case STATE_IDLE:
			printf("\f ** GarfieldPOS **\n");
			break;
		case STATE_BARCODE:
			printf("\fScanning...\n");
			break;
		case STATE_PLU:
			printf("\fPLU: ");
			break;
		case STATE_DISPLAY:
			printf("\fLast item: %.2f\r\n%3d Total: %.2f\n", (POS.items>0)?POS.cart[POS.items-1].price:0.f, POS.items, cart_get_total());
			break;
		case STATE_STORNO:
			printf("\fStorno: ");
			break;
		case STATE_PAY:
			printf("\fUID: ");
			break;
		case STATE_DEBUG:
			printf("\f%d TX %d IT\r\nVol %0.2f\n", POS.transactions, POS.sold_items, POS.sales_volume);
			break;
		case STATE_CREDIT:
			printf("\fUID: ");
			break;
	}
	fflush(stdout);
}

const char* state_dbg_string(POS_STATE s){
	switch(s){
		case STATE_IDLE:
			return "STATE_IDLE";
		case STATE_BARCODE:
			return "STATE_BARCODE";
		case STATE_PLU:
			return "STATE_PLU";
		case STATE_DISPLAY:
			return "STATE_DISPLAY";
		case STATE_STORNO:
			return "STATE_STORNO";
		case STATE_PAY:
			return "STATE_PAY";
		case STATE_DEBUG:
			return "STATE_DEBUG";
		case STATE_CREDIT:
			return "STATE_CREDIT";
	}
	return "UNKNOWN";
}

const char* action_dbg_string(TOKEN_ACTION a){
	switch(a){
		case TOKEN_KEEP:
			return "TOKEN_KEEP";
		case TOKEN_DISCARD:
			return "TOKEN_DISCARD";
		case TOKEN_REMOVE:
			return "TOKEN_REMOVE";
		case TOKEN_CONSUME:
			return "TOKEN_CONSUME";
	}
	return "UNKNOWN";
}
