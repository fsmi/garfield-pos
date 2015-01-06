void sig_interrupt(int param){
	printf("\fSIGINT\r\nOut of order");
	//FIXME should probably do something sensible
	POS.shutdown=true;
}

void sig_terminate(int param){
	printf("\fSIGTERM\r\nOut of order");
	//FIXME should probably do something sensible
	POS.shutdown=true;
}
