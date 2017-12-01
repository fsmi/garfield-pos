#ifdef _WIN32
	#include <windows.h>
	
	int portable_sleep(int millis){
		Sleep(millis);
		return 0;
	}
#else
	#include <unistd.h>
	
	int portable_sleep(int millis){
		return usleep(millis*1000);
	}
#endif
