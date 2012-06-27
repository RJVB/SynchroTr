/*
 *  threadTest.cpp
 *  cseTest
 *
 *  Created by René J.V. Bertin on 20120626.
 *  Copyright 2012 IFSTTAR — LEPSIS. All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include "Thread/Thread.h"

#if defined(WIN32) || defined(_MSC_VER)
char *winError( DWORD err )
{ static char errStr[512];
	FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM
			    | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
			    NULL, err, 0, errStr, sizeof(errStr), NULL );
	return errStr;
}
#	ifndef __MINGW32__
static int snprintf( char *buffer, size_t count, const char *format, ... )
{ int n;
	va_list ap;
	va_start( ap, format );
	n = _vsnprintf( buffer, count, format, ap );
	if( n < 0 ){
		buffer[count-1] = '\0';
	}
	return n;
}
#	endif

#else
#	define winError(err)	strerror(err)
#endif

class DemoThread : public Thread
{
	virtual DWORD Run( LPVOID arg )
	{ DWORD *count = (DWORD*) arg;
		*count = 0;
		while( 1 ){
			fprintf( stderr, "##%lu(%p) Threaded Object Code \n", GetCurrentThreadId(), GetThread() );
			*count += 1;
			Sleep(1000);
		}
		fprintf( stderr, "##%lu(%p) returning 123\n", GetCurrentThreadId(), GetThread() );
		return 123;
	}
};

int main( int argc, char *argv[] )
{ DWORD counter, stopRet, startRet = GetLastError();
	if( startRet != 0 ){
		fprintf( stderr, "Error %d = %s\n", startRet, winError(startRet) );
	}
	SetLastError(0);
	DemoThread dmt;
	startRet = GetLastError();
	if( startRet != 0 ){
		fprintf( stderr, "Error %d = %s\n", startRet, winError(startRet) );
	}
	fprintf( stderr, ">>%lu started %p == %lu, sleeping 5s\n",
		   GetCurrentThreadId(), dmt.GetThread(), (startRet = dmt.Start(&counter)) );
	if( startRet != 0 ){
		fprintf( stderr, "Error %d = %s\n", startRet, winError(startRet) );
	}
	Sleep(5000);
	stopRet = dmt.Stop(false);
	fprintf( stderr, ">>%lu %p->Stop(FALSE) == %ld, ExitCode=%lu\n", GetCurrentThreadId(), &dmt, stopRet, dmt.GetExitCode() );
	stopRet = dmt.Stop(true);
	fprintf( stderr, ">>%lu %p->Stop(TRUE) == %ld, ExitCode=%lu\n", GetCurrentThreadId(), &dmt, stopRet, dmt.GetExitCode() );
	fprintf( stderr, "counter=%lu\n", counter );
	exit(0);
}