#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

int main(int argc, char** argv){
	char* input_device=NULL;
	int fd, bytes;
	struct input_event ev;

	if(argc<2){
		printf("Insufficient arguments\n");
		exit(1);
	}
	
	input_device=argv[1];

	fd=open(input_device, O_RDONLY);

	if(fd<0){
		printf("Failed to open device\n");
		exit(1);
	}

	//get exclusive control
	bytes=ioctl(fd, EVIOCGRAB, 1);
	
	while(true){
		bytes=read(fd, &ev, sizeof(struct input_event));
		if(bytes<0){
			printf("read() error\n");
			close(fd);
			exit(1);
		}
		if(ev.type==EV_KEY){
			if(ev.value!=0){
				continue;
			}
			printf("Scancode %d\n", ev.code);
		}
	}

	return 0;
}
