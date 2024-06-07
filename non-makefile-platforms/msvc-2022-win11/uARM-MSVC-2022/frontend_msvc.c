#ifndef FRONTEND_MSVC
#define FRONTEND_MSVC

#include "../../../emulation-core/SoC.h"
#include <stdio.h>
#include <windows.h>
#include <consoleapi.h>
#include <conio.h>
#include <strsafe.h>

static HANDLE hStdin, hStdout;

void ErrorExit(LPSTR lpszMessage);


int readchar(int* ctlCSeen) {
	// Do a non-blocking read of a single character.
	if (*ctlCSeen)
		return CHAR_CTL_C;
	if (_kbhit()) {
		return (int)_getch();
	}
	else {
		return CHAR_NONE;
	}

	/*

	// Check to see whether any input events have happened; if not,
	// quit;
	DWORD numEvents;
	if (!GetNumberOfConsoleInputEvents(hStdin, &numEvents)) {
		ErrorExit("Windows: can't check number of input events")
	}

	if (numEvents == 0) {
		return CHAR_NONE;
	} else {
		// Otherwise, check for an input event:
		INPUT_RECORD inpRecord;
		DWORD numRead;
		if (!ReadConsoleInput(
			hStdin, inpRecord, 1, &numRead))
			ErrorExit("Windows: can't read in an input event");

		// And check to see whether it's an input event;
		if (inpRecord.EventType == KEY_EVENT) {
			return (int)inpRecord.Event.KeyEvent.AsciiChar;
		} else {
			return CHAR_NONE;
		}

	}
	*/
}

void writechar(int chr) {
	// Copied directly from frontend_posix.c
	if (!(chr & 0xFF00)) {

		printf("%c", chr);
	}
	else {
		printf("<<~~ EC_0x%x ~~>>", chr);
	}
	fflush(stdout);
}

void setupTerminal() {
	hStdin = GetStdHandle(STD_INPUT_HANDLE);
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hStdin == INVALID_HANDLE_VALUE || hStdout == INVALID_HANDLE_VALUE)
		ErrorExit("Windows: can't get HANDLE for stdin or stdout (or both)");

	// stdin: raw input, please. (consider some form of buffering on slower
	// computers...)
	// " When a console is created, all input modes except
	//   ENABLE_WINDOW_INPUT and ENABLE_VIRTUAL_TERMINAL_INPUT
	//   are enabled by default " (ConsoleApi.h reference.)
	DWORD rawMode = ENABLE_ECHO_INPUT | ENABLE_INSERT_MODE | ENABLE_LINE_INPUT |
		ENABLE_MOUSE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_QUICK_EDIT_MODE;
	if (!SetConsoleMode(hStdin, rawMode))
		ErrorExit("Windows: stdout can't enter raw mode");


	// stdout: VT100/ANSI control codes, please
	DWORD outMode = ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	BOOL foo = SetConsoleMode(hStdout, outMode);
	if (!SetConsoleMode(hStdout, outMode))
		ErrorExit("Windows: stdin can't turn VT100/ANSI escape sequence processing on");
}



void teardownTerminal(void) {
	// TODO: worry about restoring the old console modes we overwrote
	// in setupTerminal. For now, not needed since we're not running
	// uARM from an already running console/cmd.
}


void ErrorExit(LPSTR lpszMessage) {
	teardownTerminal();
	// Retrieve the system error message for the last-error code; 
	// blatantly copied from MSDN:
	// https://learn.microsoft.com/en-us/windows/win32/Debug/retrieving-the-last-error-code

	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError();

	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);

	// Display the error message and exit the process

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlenA((LPCTSTR)lpMsgBuf) + lstrlenA((LPCTSTR)lpszMessage) + 40) * sizeof(TCHAR));
	StringCchPrintfW((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%S failed with error %d: %S"),
		lpszMessage, dw, lpMsgBuf);
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
	ExitProcess(dw);
}


#endif