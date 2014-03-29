#include "database.c"
#include "cart.c"

TRANSITION_RESULT state_idle(INPUT_TOKEN token, CONFIG* cfg){
	TRANSITION_RESULT res={STATE_IDLE, TOKEN_DISCARD, false};

	switch(token){
		case TOKEN_PLU:
			res.state=STATE_PLU;
			break;
		case TOKEN_NUMERAL:
			res.state=STATE_BARCODE;
			res.action=TOKEN_KEEP;
			break;
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
				printf(">Not recognized\n");
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
	int last_numeric;
	CART_ITEM item;

	switch(token){
		case TOKEN_BACKSPACE:
			printf("\b \b");
			res.action=TOKEN_REMOVE;
			break;
		case TOKEN_NUMERAL:
			last_numeric=tok_lasttype_offset(TOKEN_NUMERAL);
			printf("%c",INPUT.parse_head[last_numeric]);
			res.action=TOKEN_KEEP;
			break;
		case TOKEN_ENTER:
			item=db_query_item(cfg, INPUT.parse_head);
			if(cfg->verbosity>2){
				fprintf(stderr, "Returned item ID: %d, Price: %f\n", item.id, item.price);
			}
			
			if(item.id<=0){
				printf("\r\n>Not recognized\n");
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
	TRANSITION_RESULT res={STATE_DISPLAY, TOKEN_DISCARD, false};
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
			res.action=TOKEN_CONSUME;
			res.state=STATE_IDLE;
			break;
		case TOKEN_STORNO:
			res.state=STATE_STORNO;
			res.action=TOKEN_CONSUME;
			break;
		case TOKEN_PAY:
			res.state=STATE_PAY;
			res.action=TOKEN_CONSUME;
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
			res.action=TOKEN_CONSUME;
			break;
		default:
			return res;
	}
	return res;
}

TRANSITION_RESULT state_storno(INPUT_TOKEN token, CONFIG* cfg){
	TRANSITION_RESULT res={STATE_STORNO, TOKEN_DISCARD, false};
	int last_numeral, i;
	CART_ITEM item;

	switch(token){
		case TOKEN_NUMERAL:
			last_numeral=tok_lasttype_offset(TOKEN_NUMERAL);
			printf("%c",INPUT.parse_head[last_numeral]);
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
				printf("\r\n>Not recognized\n");
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
	int last_numeral, i;
	GARFIELD_USER user;

	switch(token){
		case TOKEN_NUMERAL:
			last_numeral=tok_lasttype_offset(TOKEN_NUMERAL);
			printf("%c",INPUT.parse_head[last_numeral]);
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
				printf(">Not recognized\n");
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
			}

			printf(">%s    -%.2f\n", user.name, cart_get_total());
			portable_sleep(1000);

			POS.items=0;
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
	}
	TRANSITION_RESULT def={STATE_IDLE, TOKEN_DISCARD, false};
	return def;
}

void state_enter(POS_STATE s){
	switch(s){
		case STATE_IDLE:
			printf("\f> ** GarfieldPOS ** \n");
			break;
		case STATE_BARCODE:
			printf("\f>Scanning...\n");
			break;
		case STATE_PLU:
			printf("\f>PLU: ");
			break;
		case STATE_DISPLAY:
			printf("\f>Last item: %.2f\r\n>%3d Total: %.2f\n", (POS.items>0)?POS.cart[POS.items-1].price:0.f, POS.items, cart_get_total());
			break;
		case STATE_STORNO:
			printf("\f>Storno: ");
			break;
		case STATE_PAY:
			printf("\f>UID: ");
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
