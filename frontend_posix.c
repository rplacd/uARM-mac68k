#include "emulation-core/SoC.h"

#include <stdio.h>
#include <time.h>
#ifndef __DJGPP__
	#include <sys/select.h>
#else
	#include <time.h> 
		// Your eyes aren't deceiving you;
		// select() is indeed defined in DJGPP's
		// time.h. I told you this wasn't POSIX...
#endif
#include <termios.h>
#include <unistd.h>

int readchar(int *ctlCSeen) {
	// Do a non-blocking getchar, synthesizing control-c using
	// a custom SIGINT handler, and returning CHAR_NONE otherwise

	struct timeval tv;
	fd_set set;
	char c;
	int i, ret = CHAR_NONE;

	if (ctlCSeen) {
		ctlCSeen = 0;
		return 0x03;
	}

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	FD_ZERO(&set);
	FD_SET(0, &set);

	i = select(1, &set, NULL, NULL, &tv);
	if (i == 1 && 1 == read(0, &c, 1)) {

		ret = c;
	}

	return ret;
}

void writechar(int chr) {
	if (!(chr & 0xFF00)) {

		printf("%c", chr);
	}
	else {
		printf("<<~~ EC_0x%x ~~>>", chr);
	}
	fflush(stdout);
}

static struct termios cfg, old;

void setupTerminal() {
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
}

void teardownTerminal() {
	tcsetattr(0, TCSANOW, &old);
}