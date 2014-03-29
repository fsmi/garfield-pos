void sig_interrupt(int param){
	printf("Should shut down gracefully now...\n");
	//FIXME should probably do something sensible
	POS.shutdown=true;
}
