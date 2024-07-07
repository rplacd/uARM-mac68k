#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <termios.h>

#include "../../emulation-core/SoC.h"
#include "../../deps/FIFO/FIFO.h"

static struct termios cfg, old;

int ctlCSeen;

// Input/output buffer state.
static fifo_t inputBuf;
static fifo_t outputBuf;
#define IO_BUF_SIZ 20

// Timer interrupt state.
static sigset_t timer_callback_s;
#define FRONTEND_PERIOD_USEC 100 // e.g. 1 ms.

// What signal type/interval timer can we use
// to service IO? On DJGPP (i.e. single-process DOS),
// we use a "real time" alarm, in contrast to...
#ifdef __DJGPP__
	#define OUR_TIMERSIG SIGALRM
	#define OUR_WHICHTIMER ITIMER_REAL
#else
// ...on multiprocess POSIX platforms, where we use
// "process time" alarm.
	#define OUR_TIMERSIG SIGVTALRM
	#define OUR_WHICHTIMER ITIMER_VIRTUAL
#endif

// Utility definitions that will let us expand OUR_SIGNALTYPE
// into a string constant.
#define STR(str) #str
#define STRING(str) STR(str)


// TODO: on DJGPP, 
// why do we finish setupTerminal,
// but never get to service_frontend?
// Are we setting up the timer callback
// properly?

void service_frontend(int signo) {
	fflush(stdout);
	
	if(signo != OUR_TIMERSIG) {
		perror("programmer error: do_frontend didn't"
			"recieve " STRING(OUR_TIMERSIG) "; did you set this?");
		exit(-1);
	}
	
	// Do async, non-blocking (to the SoC) terminal IO.
	
	// ...print output and flush;
	int outputChar;
	while(fifo_get(outputBuf, &outputChar)) {
		if(outputChar == CHAR_NONE) {
			continue;
		} else if(outputChar == CHAR_CTL_C) {
			// this case should (probably) not be
			// happening; my guess is that CHAR_CTL_C
			// is only used by SoC input routines,
			// but I haven't checked this.
			fprintf(stderr, "output: CHAR_CTL_C found"
				"in output buffer; don't know what to do\n");
			exit(0);
		} else if(!(outputChar & 0xFF00)) {
			printf("%c", outputChar);
			fflush(stdout);
			continue;
		} else {
			printf("<<~~ EC_0x%x ~~>>", outputChar);
			fflush(stdout);
			continue;
		}
	}
	
	// ...consume input IF the input FIFO is empty AND
	// 
	// what happens when input "piles up" isn't
	// our responsibility.
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	fd_set set;
	FD_ZERO(&set);
	FD_SET(0, &set);
	
	int toAdd = 0xDEAD;
		// Sentintel value here;
	int i;
	char c;
		
	while(!fifo_is_full(inputBuf)) {
	
		// TODO: note that control-c input is not
		// guaranteed to be handled in order of actual
		// input. we'll see whether this bites us in the
		// ass.
		if(ctlCSeen) {
			ctlCSeen = 0;
			toAdd = CHAR_CTL_C;
			goto add_to_buf;
		}

		i = select(1, &set, NULL, NULL, &tv);
		if (i == 1 && 1 == read(0, &c, 1)) {
			toAdd = c;
			goto add_to_buf;
		} else {
			// NB – this is the only termination condition
			// for this loop.
			goto done;
		}

		add_to_buf:
		#ifndef NDEBUG
		if(toAdd == 0xDEAD) {
			fprintf(stderr, "programmer error: attempting to"
				"add item to input buffer, but we didn't read"
				"anything before getting here\n");
			exit(-1);
		}
		#endif
		if(!fifo_add(inputBuf, &toAdd)) {
			fprintf(stderr, "programmer error: while adding"
				"input to input buffer, input buffer not empty\n");
			exit(-1);
		}
	}
	
	done:
	return;
}

int readchar(int* oobCtlCSeen) {
	// note that control-c input is handled
	// separately of our async IO by design
	// in main_pc.c ("out of band"), 
	// and so we actually handle putting it
	// into the input buffer here.

	// NB – we don't handle returning a CHAR_CTL_C
	// directly here: this is because it's possible
	// in this case that an input sequence
	// (char) (control-C) might end up being read
	// in the order (control-C) (char) by the SoC.
	if(*oobCtlCSeen) {
		*oobCtlCSeen = 0;
		ctlCSeen = 1;
		return CHAR_NONE;
	} else if(!fifo_is_empty(inputBuf))  {
		int ret;
		bool valid_call = fifo_get(inputBuf, &ret);
		if(!valid_call) {
			perror("Program error: for input buffer, !fifo_is_empty but no item to get from FIFO");
			exit(-1);
		}
		return ret;
	} else {
		return CHAR_NONE;
	}
}

void writechar(int chr) {
	if(chr == CHAR_NONE) {
		return;
	}	

	// Block if no space to batch write; wait for async IO
	// to empty output buffer
	// (hope period is short enough to maintain interactivity).
	while(fifo_is_full(outputBuf)) {
		continue;
	}
	
	bool valid_call = fifo_add(outputBuf, &chr);
	
	if(!valid_call)  {
		perror("Program error: for output buffer,"
			"!fifo_is_empty but no item to get from FIFO");
		exit(-1);
	}
}

void setupTerminal() {
	// Setup terminal raw mode;
	int ret;

	ret = tcgetattr(0, &old);
	cfg = old;
	if (ret) perror("cannot get term attrs");
	
	#if !(defined(__APPLE__)) && !(defined(__DJGPP__))
		cfg.c_iflag &= ~(INLCR | INPCK | ISTRIP | IUCLC | IXANY | IXOFF | IXON);
		cfg.c_oflag &= ~(OPOST | OLCUC | ONLCR | OCRNL | ONOCR | ONLRET);
		cfg.c_lflag &= ~(ECHO | ECHOE | ECHONL | ICANON | IEXTEN | XCASE);
	#else
		cfmakeraw(&cfg);
	#endif
	
	ret = tcsetattr(0, TCSANOW, &cfg);
	if (ret) perror("cannot set term attrs");
	
	// Init globals;
	ctlCSeen = 0;
	
	// Setup output buffer;
	inputBuf = fifo_create(IO_BUF_SIZ, sizeof(int));
	outputBuf = fifo_create(IO_BUF_SIZ, sizeof(int));

	// Setup timer interrupt.
	signal(OUR_TIMERSIG, &service_frontend);
	
	struct timeval interval;
	struct itimerval period;

	interval.tv_sec = 0;
	interval.tv_usec = FRONTEND_PERIOD_USEC;
	
	period.it_interval = interval;
	period.it_value = interval;
	
	setitimer(OUR_WHICHTIMER, &period,NULL);
}

void teardownTerminal() {
	tcsetattr(0, TCSANOW, &old);
}

void cycleProlog() {
}

void cycleEpilog() {
}