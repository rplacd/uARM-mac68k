#include "../../utilities/compiler_hacks.h"

#include "../../emulation-core/SoC.h"
#include "../../emulation-core/callout_RAM.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef _MSC_VER
	#include <unistd.h>
	// Needed for ftruncate
#else
	#include <io.h>
	#define ftruncate _chsize_s
#endif
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>


// DECLARATIONS for frontend functions to be defined in a frontend_*.c file.
// For standard POSIX, see frontend_POSIX.h.
int readchar(int* ctlCSeen);
void writechar(int chr);
void setupTerminal();
void teardownTerminal();
// A special routine, defined in frontend_asyncterm.c, to service
// the UI (i.e. read/print chars) and maintain interactivity.
void service_frontend();

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
	
	
	r = (unsigned char*)malloc(len);
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

void ctl_cHandler(_UNUSED_ int v){	//handle SIGTERM      
	
//	exit(-1);
	ctlCSeen = 1;
}

int readchar_wrapper() {
	// Why do we need a 'wrapper' for readchar (originally defined in
	// frontend_posix.h, etc.?)
	// We need to pass to readchar(int*) a piece of state - the flag
	// set by the control-c handler. uARM's SoC code doesn't do this
	// plumbing for us.
	return readchar(&ctlCSeen);
}

int rootOps(void* userData, UInt32 sector, void* buf, UInt8 op){	
	// NB: rootOps is of type ArmMemAccessF.
	
	FILE* root = (FILE*)userData;
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


Boolean coRamAccess(_UNUSED_ void* ram, UInt32 addr, UInt8 size, Boolean write, void* bufP) {
	// NB: coRamAccess is of type mem.h::ArmRamAccessF.
	// NB: Below, we ran socInit(&soc, socRamModeCallout, coRamAccess, ...
	// 		As a result, this fucntion (coRamAccess) is a callback
	//		that handles memory operations.
	// Perform memory load/store operations via reading/writing
	// to a ramfile. Because disk IO here is the biggest hog
	// on performance, and because every time we call
	// this function we read/write bytes in multiples
	// of the constant BLK_DEV_BLK_SZ, we can interleave calls
	// to service_frontend() with individual byte read/writes.
	// Hopefully, the time it takes to read/write an individual byte
	// is small enough (as opposed to an entire block) that running
	// service_frontend() between bytes is enough to maintain
	// interactivity.

	UInt8* b = (UInt8*)bufP;
	
	// The physical memory map seen by the SoC maps RAM to the region RAM_BASE, RAM_BASE + RAM_SIZE.
	// Therefore, to map to RAM device adresses, subtract RAM_BASE.
	addr = addr - RAM_BASE;
	
	#ifndef NDEBUG
	// Optionally, double-check to see whether addr <= RAM_SIZE; this catches cases where
	// 'addr' is a logical address instead of a physical one.
	if(addr >= RAM_SIZE) {
		printf("Error accessing ramfile: requested address larger than physical limits. Are you using logical addresses?\n");
		printf("%x >= %lu\n", addr, RAM_SIZE);
		exit(-1);
	}
	#endif
	
	if(fseek(ramFile, addr, SEEK_SET) != 0) {
		perror("Error accessing location in ramfile");
		exit(-1);
	}

	if(write) {
		// pseudocode: ramWrite(addr, b, size), while
		// interleaving service_frontend() between
		// every byte.
		size_t i, result = 0;
		for(i = 0; i < size; ++i) {
			service_frontend();
			result += fwrite(b + i, 1, 1, ramFile);
		}
		
		// the C standard doesn't specify how 'fwrite' will
		// signal an error condition with its return value,
		// so we manually check for error conditions:
		if (feof(ramFile)) {
			printf("Error writing to ramfile: end of file");
			exit(-1);
		} else if (result != size || ferror(ramFile)) {
			perror("Error writing to ramfile");
			exit(-1);
		}
	} else {
		size_t i, result = 0;
		
		// pseudocode: ramRead(addr, b, size), while
		// interleaving service_frontend() between
		// every byte.
		for(i = 0; i < size; ++i) {
			service_frontend();
			result += fread(b + i, 1, 1, ramFile);
		}
		
		if(result != size) {
			if (feof(ramFile)) {
				printf("Error writing to ramfile: end of file");
				exit(-1);
			} else if (ferror(ramFile)) {
				perror("Error writing to ramfile");
				exit(-1);
			} else {
				printf("Error writing to ramfile: some other error");
				exit(-1);
			}
		}
	}

	return true;
}

int main(int argc, char** argv){
	
	FILE* root = NULL;
	int gdbPort = 0;
	
	if(argc <= 1 || argc >= 4){
		PRINT_HELP_AND_QUIT:
		fprintf(stderr,"usage: %s path_to_disk [with-telemetry] [gdbPort]]\n", argv[0]);
		return -1;	
	}
	
	
	//setup the terminal
	setupTerminal();
	
	// custom SIGINT (ctrl-c) handler, that sets a flag to
	// pass along an interrupt char to the emulated OS

	signal(SIGINT, &ctl_cHandler);
	
	
	// handle argv[2] – boot image
	root = fopen(argv[1], "r+b");
	if(!root){
		fprintf(stderr,"Failed to open root device\n");
		exit(-1);
	}
	
	// handle argv[3] - with-statusfile
	Boolean withStatusFile = 0;
	
		// If with-statusfile is given, this is the state
		// we will put into the statusfile
	FILE *statusfile = NULL;
	unsigned long totalCycles = 0;
	time_t lastCycleAt = clock();
	
	if(argc >= 3) {
		// The only item that can appear as a second param is "with-telemetry"
		// but double check anyway
		if(0 == strncmp("with-telemetry", argv[2], 15)) {
			withStatusFile = 1;
			
			statusfile = fopen("Telemetry File", "w");
			
			if(NULL == statusfile) {
				perror("Can't create or open statusfile");
				exit(-1);
			}
	
		} else goto PRINT_HELP_AND_QUIT;
	}
	
	
	// handle argv[4] - gdb port
	
	if(argc >= 4) gdbPort = atoi(argv[3]);
	
	
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
	socInit(&soc, socRamModeCallout, coRamAccess, readchar_wrapper, writechar, rootOps, root);
	
	socRunState runState;
	socRunStateInit(&runState);
	
	Boolean going;
	
	// Main loop. Might choose to receive UI events here, for example.
	do {
		service_frontend();
			// Service UI here, as well as while (interleaved with)
			// ramfile read/writes...
		
		going = socCycle(&soc, gdbPort, &runState);
		
		if(withStatusFile) {
		// Overwrite statusfile (that is, refresh status displayed.)
			totalCycles++;
			time_t now = clock();
			time_t cycleTime = difftime(now, lastCycleAt);
			lastCycleAt = now;
			
			rewind(statusfile);
			
			// write new status
			fprintf(statusfile, "at time: %lu\ntotal cycles (possible overflow): %lu\ntime this cycle took: %lu\n",
				now, totalCycles, cycleTime);
			fflush(statusfile);
		}
	} while(going);
	
	//teardown the RAM disk
	{
		fclose(ramFile);
	}
	
	//cleanup opened files
	fclose(root);

	teardownTerminal();
	
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
	/* the original implementation of uARM by D.G. used *nix 
	   sys/time.h and returned a Unix timestamp: 
			struct timeval tv;
			gettimeofday(&tv, NULL);
			return tv.tv_sec;	
			
		the following re-implmentation using C standard libraries
		will (probably) do the exact same thing on *nixes with
		sys/time.h, but its behavior on non-*nix platforms is unknown.
	*/
	return time(NULL);
}

void err_str(const char* str){
	
	fprintf(stderr, "%s", str);	
}

