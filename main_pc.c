#include "emulation-core/SoC.h"
#include "emulation-core/callout_RAM.h"

	
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
	// Needed for ftruncate
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <signal.h>
#include <termios.h>

unsigned char* readFile(const char* name, UInt32* lenP){

	long len = 0;
	unsigned char *r = NULL;
	int i;
	FILE* f;
	
	f = fopen(name, "r");
	if(!f){
		perror("cannot open file");
		return NULL;
	}
	
	i = fseek(f, 0, SEEK_END);
	if(i){
		return NULL;
		perror("cannot seek to end");
	}
	
	len = ftell(f);
	if(len < 0){
		perror("cannot get position");
		return NULL;
	}
	
	i = fseek(f, 0, SEEK_SET);
	if(i){
		return NULL;
		perror("cannot seek to start");
	}
	
	
	r = malloc(len);
	if(!r){
		perror("cannot alloc memory");
		return NULL;
	}
	
	if(len != (long)fread(r, 1, len, f)){
		perror("canot read file");
		free(r);
		return NULL;
	}
	
	*lenP = len;
	return r;
}



static int ctlCSeen = 0;

static int readchar(void){
	// Do a non-blocking getchar, synthesizing control-c using
	// a custom SIGINT handler, and returning CHAR_NONE otherwise
	
	struct timeval tv;
	fd_set set;
	char c;
	int i, ret = CHAR_NONE;
	
	if(ctlCSeen){
		ctlCSeen = 0;
		return 0x03;
	}
	
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	
	FD_ZERO(&set);
	FD_SET(0, &set);
	
	i = select(1, &set, NULL, NULL, &tv);
	if(i == 1 && 1 == read(0, &c, 1)){
		
		ret = c;
	}

	return ret;
}

static void writechar(int chr){

	if(!(chr & 0xFF00)){
		
		printf("%c", chr);
	}
	else{
		printf("<<~~ EC_0x%x ~~>>", chr);
	}
	fflush(stdout);	
}

void ctl_cHandler(_UNUSED_ int v){	//handle SIGTERM      
	
//	exit(-1);
	ctlCSeen = 1;
}

int rootOps(void* userData, UInt32 sector, void* buf, UInt8 op){
	
	FILE* root = userData;
	int i;
	
	switch(op){
		case BLK_OP_SIZE:
			
			if(sector == 0){	//num blocks
				
				if(root){
					
					i = fseek(root, 0, SEEK_END);
					if(i) return false;
					
					 *(unsigned long*)buf = ftell(root) / BLK_DEV_BLK_SZ;
				}
				else{
					
					*(unsigned long*)buf = 0;
				}
			}
			else if(sector == 1){	//block size
				
				*(unsigned long*)buf = BLK_DEV_BLK_SZ;
			}
			else return 0;
			return 1;
		
		case BLK_OP_READ:
			
			i = fseek(root, sector * BLK_DEV_BLK_SZ, SEEK_SET);
			if(i) return false;
			return fread(buf, 1, BLK_DEV_BLK_SZ, root) == BLK_DEV_BLK_SZ;
		
		case BLK_OP_WRITE:
			
			i = fseek(root, sector * BLK_DEV_BLK_SZ, SEEK_SET);
			if(i) return false;
			return fwrite(buf, 1, BLK_DEV_BLK_SZ, root) == BLK_DEV_BLK_SZ;
	}
	return 0;	
}

SoC soc;
const char* ramFilePath = "16 MB of RAM";
FILE *ramFile; // 16MB, per RAM_SIZE in SoC.c


Boolean coRamAccess(_UNUSED_ CalloutRam* ram, UInt32 addr, UInt8 size, Boolean write, void* bufP) {
// Below, we ran socInit(&soc, socRamModeCallout, coRamAccess, ...
// As a result, this fucntion (coRamAccess) is a callback that handles memory operations.

	UInt8* b = bufP;
	
	// The physical memory map seen by the SoC maps RAM to the region RAM_BASE, RAM_BASE + RAM_SIZE.
	// Therefore, to map to RAM device adresses, subtract RAM_BASE.
	addr = addr - RAM_BASE;
	
	#ifndef NDEBUG
	// Optionally, double-check to see whether addr <= RAM_SIZE; this catches cases where
	// 'addr' is a logical address instead of a physical one.
	if(addr >= RAM_SIZE) {
		printf("Error accessing ramfile: requested address larger than physical limits. Are you using logical addresses?\n");
		printf("%x >= %x\n", addr, RAM_SIZE);
		exit(-1);
	}
	#endif
	
	if(fseek(ramFile, addr, SEEK_SET) != 0) {
		perror("Error accessing location in ramfile");
		exit(-1);
	}

	if(write) {
		// pseudocode: ramWrite(addr, b, size);
		fwrite(b, 1, size, ramFile);
		
		// the C standard doesn't specify how 'fwrite' will
		// signal an error condition with its return value,
		// so we manually check for error conditions:
		if (feof(ramFile)) {
			printf("Error writing to ramfile: end of file");
			exit(-1);
		} else if (ferror(ramFile)) {
			perror("Error writing to ramfile");
			exit(-1);
		}
	} else {
		// pseudocode: ramRead(addr, b, size);
		if(fread(b, 1, size, ramFile) != size) {
			if (feof(ramFile)) {
				printf("Error writing to ramfile: end of file");
				exit(-1);
			} else if (ferror(ramFile)) {
				perror("Error writing to ramfile");
				exit(-1);
			} else {
				printf("Error writing to ramfile: some other error\n");
				exit(-1);
			}
		}
	}

	return true;
}

int main(int argc, char** argv){
	
	struct termios cfg, old;
	FILE* root = NULL;
	int gdbPort = 0;
	
	if(argc != 3 && argc != 2){
		fprintf(stderr,"usage: %s path_to_disk [gdbPort]\n", argv[0]);
		return -1;	
	}
	
	
	//setup the terminal
	{
		int ret;
		
		ret = tcgetattr(0, &old);
		cfg = old;
		if(ret) perror("cannot get term attrs");
		
		#ifndef __APPLE__
		
			cfg.c_iflag &=~ (INLCR | INPCK | ISTRIP | IUCLC | IXANY | IXOFF | IXON);
			cfg.c_oflag &=~ (OPOST | OLCUC | ONLCR | OCRNL | ONOCR | ONLRET);
			cfg.c_lflag &=~ (ECHO | ECHOE | ECHONL | ICANON | IEXTEN | XCASE);
		#else
			cfmakeraw(&cfg);
		#endif
		
		ret = tcsetattr(0, TCSANOW, &cfg);
		if(ret) perror("cannot set term attrs");
	}
	
	// custom SIGINT (ctrl-c) handler, that sets a flag to
	// pass along an interrupt char to the emulated OS
	signal(SIGINT, &ctl_cHandler);
	
	
	root = fopen(argv[1], "r+b");
	if(!root){
		fprintf(stderr,"Failed to open root device\n");
		exit(-1);
	}
	
	if(argc >= 3) gdbPort = atoi(argv[2]);
	
	
	//setup the RAM disk
	{
		ramFile = fopen(ramFilePath, "w+b");
		if (ramFile == NULL) {
			perror("Creating/opening ramfile failed");
			exit(-1);
		}
		if(ftruncate(fileno(ramFile), RAM_SIZE) != 0) {
			perror("Expanding ramfile to 16MB (SoC.h/RAM_SIZE) failed");
			exit(-1);
		}
	}
	
	// Set up SoC, and then run the main loop
	socInit(&soc, socRamModeCallout, coRamAccess, readchar, writechar, rootOps, root);
	
	socRunState runState;
	socRunStateInit(&runState);
	
	Boolean going;
	
	// Main loop. Might choose to receive UI events here, for example.
	do {
		going = socCycle(&soc, gdbPort, &runState);
	} while(going);
	
	//teardown the RAM disk
	{
		fclose(ramFile);
	}
	
	fclose(root);
	tcsetattr(0, TCSANOW, &old);
	
	return 0;
}


//////// runtime things

void* emu_alloc(UInt32 size){
	
	return calloc(size,1);	
}

void emu_free(void* ptr){
	
	free(ptr);
}

UInt32 rtcCurTime(void){
	
	struct timeval tv;
	
	gettimeofday(&tv, NULL);
	
	return tv.tv_sec;	
}

void err_str(const char* str){
	
	fprintf(stderr, "%s", str);	
}

