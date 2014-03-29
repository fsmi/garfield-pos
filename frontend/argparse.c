bool arg_parse(CONFIG* cfg, int argc, char** argv){
	int i,c;

	for(i=0;i<argc;i++){
		if(argv[i][0]=='-'){
			switch(argv[i][1]){
				case 'f':
					if(argc>i+1){
						cfg->cfg_file=argv[i+1];
						i++;
					}
					else{
						fprintf(stderr, "No config file specified\n");
						return false;
					}
					break;
				case 'h':
					return false;
				case 'v':
					for(c=1;argv[i][c]=='v';c++){
					}
					cfg->verbosity=c-1;
					break;
				default:
					fprintf(stderr, "Unrecognized option %s\n", argv[i]);
					return false;
			}
		}
		else{
			fprintf(stderr, "Not an option: %s\n", argv[i]);
			return false;
		}
	}

	return true;
}
