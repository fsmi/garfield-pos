void sig_interrupt(int param){
	printf("\fTerminating...\r\nGone");
	//FIXME should probably do something sensible
	POS.shutdown=true;
}
